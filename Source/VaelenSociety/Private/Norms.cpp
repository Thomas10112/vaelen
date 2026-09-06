// VAELEN - VaelenSociety
// Phase 05.03: norms.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic tests in Tests/Society

#include "Vaelen/Society/Norms.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Society
{
	namespace
	{
		constexpr uint64 NormSalt = 0x4e4f524dull; // "NORM"

		/// A draw in [Min, Max] from the identity and a slot.
		uint32 Draw(Hash64 Identity, uint32 Slot, uint32 Min, uint32 Max) noexcept
		{
			const uint64 H = Noise::LatticeHash(Identity ^ NormSalt, static_cast<int32>(Slot), 0);
			return Max > Min ? Min + static_cast<uint32>(H % (uint64{Max - Min} + 1u)) : Min;
		}

		struct CultureRef
		{
			EntityHandle Handle;
			History::CultureInfo Info;
		};
	} // namespace

	const char* NormFieldName(NormField F) noexcept
	{
		switch (F)
		{
		case NormField::MarryFrom:
			return "marrying age";
		case NormField::MarryTo:
			return "last marrying age";
		case NormField::MaxAgeGap:
			return "age gap";
		case NormField::FaithMatters:
			return "faith in marriage";
		case NormField::Marriages:
			return "eagerness to marry";
		case NormField::Descent:
			return "descent";
		case NormField::Tolerance:
			return "tolerance";
		case NormField::Mobility:
			return "mobility";
		case NormField::Bondage:
			return "bondage";
		case NormField::Count:
		default:
			return "?";
		}
	}

	NormTypes NormTypes::Declare(World& W)
	{
		NormTypes T;
		T.Norms = W.Types().Register<NormSet>("NormSet");
		T.Marriage = W.Types().Register<Population::MarriageNorms>("MarriageNorms");
		W.Components().CreatePool(T.Norms);
		W.Components().CreatePool(T.Marriage);
		return T;
	}

	NormSet NormsFromIdentity(Hash64 Identity, const NormRules& Rules) noexcept
	{
		NormSet N;
		N.Marriage.MarryFrom = Draw(Identity, 0, Rules.MarryFromMin, Rules.MarryFromMax);
		N.Marriage.MarryTo = Draw(Identity, 1, Rules.MarryToMin, Rules.MarryToMax);
		N.Marriage.MaxAgeGap = Draw(Identity, 2, Rules.GapMin, Rules.GapMax);
		N.Marriage.FaithMatters = Draw(Identity, 3, 0, 999) < Rules.FaithMattersPerMille ? 1u : 0u;
		N.Marriage.MarriagesPerMille = Draw(Identity, 4, Rules.MarriagesMin, Rules.MarriagesMax);
		N.Descent_ = Draw(Identity, 5, 0, 999) < Rules.MatrilinealPerMille ? static_cast<uint32>(Descent::Matrilineal)
																		   : static_cast<uint32>(Descent::Patrilineal);
		N.FaithTolerancePerMille = Draw(Identity, 6, Rules.ToleranceMin, Rules.ToleranceMax);
		N.MobilityPerMille = Draw(Identity, 7, Rules.MobilityMin, Rules.MobilityMax);
		N.BondageAllowed = 0;
		for (uint32 Bit = 0; Bit < 3; ++Bit)
		{
			N.BondageAllowed |= Draw(Identity, 8 + Bit, 0, 999) < Rules.BondageBitPerMille ? (1u << Bit) : 0u;
		}
		return N;
	}

	uint32 NormValue(const NormSet& N, NormField F) noexcept
	{
		switch (F)
		{
		case NormField::MarryFrom:
			return N.Marriage.MarryFrom;
		case NormField::MarryTo:
			return N.Marriage.MarryTo;
		case NormField::MaxAgeGap:
			return N.Marriage.MaxAgeGap;
		case NormField::FaithMatters:
			return N.Marriage.FaithMatters;
		case NormField::Marriages:
			return N.Marriage.MarriagesPerMille;
		case NormField::Descent:
			return N.Descent_;
		case NormField::Tolerance:
			return N.FaithTolerancePerMille;
		case NormField::Mobility:
			return N.MobilityPerMille;
		case NormField::Bondage:
			return N.BondageAllowed;
		case NormField::Count:
		default:
			return 0;
		}
	}

	NormSet NormsOfChild(const NormSet& Parent, Hash64 Identity, const NormRules& Rules) noexcept
	{
		NormSet N = Parent;
		N.Drifts = 0;
		N.Since = 0;
		const NormSet Own = NormsFromIdentity(Identity, Rules);
		// One custom of the child's own, the rest the parent's.
		switch (static_cast<NormField>(Draw(Identity, 20, 0, static_cast<uint32>(NormField::Count) - 1u)))
		{
		case NormField::MarryFrom:
			N.Marriage.MarryFrom = Own.Marriage.MarryFrom;
			break;
		case NormField::MarryTo:
			N.Marriage.MarryTo = Own.Marriage.MarryTo;
			break;
		case NormField::MaxAgeGap:
			N.Marriage.MaxAgeGap = Own.Marriage.MaxAgeGap;
			break;
		case NormField::FaithMatters:
			N.Marriage.FaithMatters = Own.Marriage.FaithMatters;
			break;
		case NormField::Marriages:
			N.Marriage.MarriagesPerMille = Own.Marriage.MarriagesPerMille;
			break;
		case NormField::Descent:
			N.Descent_ = Own.Descent_;
			break;
		case NormField::Tolerance:
			N.FaithTolerancePerMille = Own.FaithTolerancePerMille;
			break;
		case NormField::Mobility:
			N.MobilityPerMille = Own.MobilityPerMille;
			break;
		case NormField::Bondage:
		case NormField::Count:
		default:
			N.BondageAllowed = Own.BondageAllowed;
			break;
		}
		return N;
	}

	void NormSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		std::vector<CultureRef> Cultures;
		W.Components()
			.GetPool(Types.Population.Culture)
			.ForEach([&](EntityHandle H, const History::CultureInfo& C) { Cultures.push_back(CultureRef{H, C}); });
		std::sort(Cultures.begin(), Cultures.end(),
				  [](const CultureRef& A, const CultureRef& B) { return A.Info.Index < B.Info.Index; });
		auto HandleOf = [&](uint32 Culture) -> EntityHandle
		{
			for (const CultureRef& C : Cultures)
			{
				if (C.Info.Index == Culture)
				{
					return C.Handle;
				}
			}
			return EntityHandle{};
		};
		auto Mirror = [&](EntityHandle H, const NormSet& N)
		{
			Population::MarriageNorms* M = W.Components().GetPool(Norms.Marriage).TryGet(H);
			if (M != nullptr)
			{
				*M = N.Marriage;
			}
			else
			{
				W.Components().GetPool(Norms.Marriage).Add(H, N.Marriage);
			}
		};
		// 1. Customs for every culture that lacks them, parents before children (index order).
		for (const CultureRef& C : Cultures)
		{
			if (W.Components().GetPool(Norms.Norms).TryGet(C.Handle) != nullptr)
			{
				continue;
			}
			const NormSet* Parent = nullptr;
			if (C.Info.Parent != 0)
			{
				const EntityHandle PH = HandleOf(C.Info.Parent);
				Parent = PH.IsNull() ? nullptr : W.Components().GetPool(Norms.Norms).TryGet(PH);
			}
			NormSet N = Parent != nullptr ? NormsOfChild(*Parent, C.Info.Identity, Rules)
										  : NormsFromIdentity(C.Info.Identity, Rules);
			N.Since = Context.Tick;
			W.Components().GetPool(Norms.Norms).Add(C.Handle, N);
			Mirror(C.Handle, N);
		}
		// 2. Drifts from last year's events.
		auto Change = [&](uint32 Culture, NormField Field, uint32 After)
		{
			const EntityHandle H = HandleOf(Culture);
			if (H.IsNull())
			{
				return;
			}
			NormSet* N = W.Components().GetPool(Norms.Norms).TryGet(H);
			if (N == nullptr)
			{
				return;
			}
			const uint32 Before = NormValue(*N, Field);
			if (Before == After)
			{
				return;
			}
			switch (Field)
			{
			case NormField::FaithMatters:
				N->Marriage.FaithMatters = After;
				break;
			case NormField::Tolerance:
				N->FaithTolerancePerMille = After;
				break;
			case NormField::Mobility:
				N->MobilityPerMille = After;
				break;
			default:
				return;
			}
			++N->Drifts;
			N->Since = Context.Tick;
			Mirror(H, *N);
			Context.Events->Publish(Context.Tick, NormChangedEvent,
									NormPayload{Culture, static_cast<uint32>(Field), Before, After},
									W.Entities().GetId(H));
		};
		std::vector<EntityHandle> RegionHandles;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= RegionHandles.size())
					{
						RegionHandles.resize(usize{R.Index} + 1u);
					}
					RegionHandles[R.Index] = H;
				});
		auto MajorityOf = [&](uint32 Region) -> uint32
		{
			if (Region >= RegionHandles.size() || RegionHandles[Region].IsNull())
			{
				return 0;
			}
			const History::RegionPopulation* P =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[Region]);
			return P != nullptr ? P->Majority : 0u;
		};
		const std::vector<Event>& Events = W.Log().All();
		std::vector<const Event*> Recent;
		for (usize i = Events.size(); i > 0; --i)
		{
			const Event& E = Events[i - 1];
			if (E.Tick + History::TicksPerYear <= Context.Tick)
			{
				break;
			}
			if (E.Is(History::SchismEvent) || E.Is(History::DisasterStruckEvent))
			{
				Recent.push_back(&E);
			}
		}
		std::reverse(Recent.begin(), Recent.end());
		for (const Event* E : Recent)
		{
			if (E->Is(History::SchismEvent))
			{
				const History::ReligionPayload P = E->Get<History::ReligionPayload>();
				const uint32 Culture = P.Culture != 0 ? P.Culture : MajorityOf(P.Region);
				const NormSet* N =
					Culture != 0 ? W.Components().GetPool(Norms.Norms).TryGet(HandleOf(Culture)) : nullptr;
				if (N == nullptr)
				{
					continue;
				}
				Change(Culture, NormField::FaithMatters, 1);
				const uint32 Tolerance = N->FaithTolerancePerMille;
				Change(Culture, NormField::Tolerance,
					   Tolerance > Rules.SchismToleranceLoss ? Tolerance - Rules.SchismToleranceLoss : 0u);
			}
			else
			{
				const History::DisasterPayload P = E->Get<History::DisasterPayload>();
				if (P.Severity < Rules.DriftSeverity)
				{
					continue;
				}
				const uint32 Culture = MajorityOf(P.Region);
				const NormSet* N =
					Culture != 0 ? W.Components().GetPool(Norms.Norms).TryGet(HandleOf(Culture)) : nullptr;
				if (N == nullptr)
				{
					continue;
				}
				Change(Culture, NormField::Mobility, std::min(1000u, N->MobilityPerMille + Rules.DisasterMobilityGain));
			}
		}
	}

	const NormSet* NormsOf(const World& W, const History::PreHistoryTypes& Types, const NormTypes& Norms,
						   uint32 Culture)
	{
		const NormSet* Found = nullptr;
		W.Components()
			.GetPool(Types.Population.Culture)
			.ForEach(
				[&](EntityHandle H, const History::CultureInfo& C)
				{
					if (C.Index == Culture && Found == nullptr)
					{
						Found = W.Components().GetPool(Norms.Norms).TryGet(H);
					}
				});
		return Found;
	}

	bool SetNorms(World& W, const History::PreHistoryTypes& Types, const NormTypes& Norms, uint32 Culture,
				  const NormSet& Value)
	{
		EntityHandle Found;
		W.Components()
			.GetPool(Types.Population.Culture)
			.ForEach(
				[&](EntityHandle H, const History::CultureInfo& C)
				{
					if (C.Index == Culture && Found.IsNull())
					{
						Found = H;
					}
				});
		if (Found.IsNull())
		{
			return false;
		}
		NormSet* N = W.Components().GetPool(Norms.Norms).TryGet(Found);
		if (N != nullptr)
		{
			*N = Value;
		}
		else
		{
			W.Components().GetPool(Norms.Norms).Add(Found, Value);
		}
		Population::MarriageNorms* M = W.Components().GetPool(Norms.Marriage).TryGet(Found);
		if (M != nullptr)
		{
			*M = Value.Marriage;
		}
		else
		{
			W.Components().GetPool(Norms.Marriage).Add(Found, Value.Marriage);
		}
		return true;
	}

	NormStats MeasureNorms(const World& W, const History::PreHistoryTypes& Types, const NormTypes& Norms)
	{
		NormStats S;
		std::vector<std::pair<uint32, NormSet>> All;
		W.Components()
			.GetPool(Types.Population.Culture)
			.ForEach(
				[&](EntityHandle H, const History::CultureInfo& C)
				{
					++S.Cultures;
					const NormSet* N = W.Components().GetPool(Norms.Norms).TryGet(H);
					if (N == nullptr)
					{
						return;
					}
					++S.WithNorms;
					All.push_back({C.Index, *N});
					const Population::MarriageNorms* M = W.Components().GetPool(Norms.Marriage).TryGet(H);
					const bool Same = M != nullptr && M->MarryFrom == N->Marriage.MarryFrom &&
									  M->MarryTo == N->Marriage.MarryTo && M->MaxAgeGap == N->Marriage.MaxAgeGap &&
									  M->FaithMatters == N->Marriage.FaithMatters &&
									  M->MarriagesPerMille == N->Marriage.MarriagesPerMille;
					S.MirrorMismatch += Same ? 0u : 1u;
					S.Matrilineal += N->Descent_ == static_cast<uint32>(Descent::Matrilineal) ? 1u : 0u;
					S.FaithMatters += N->Marriage.FaithMatters != 0 ? 1u : 0u;
					S.Drifts += N->Drifts;
				});
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		Hash64 D = HashString("NormSet");
		for (const auto& [Index, N] : All)
		{
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&N), sizeof(N)));
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Society
