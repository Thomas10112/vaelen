// VAELEN - VaelenPolitics
// Phase 07.02: law and the taking of dues.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics

#include "Vaelen/Politics/Law.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Politics
{
	namespace
	{
		constexpr uint32 G_GRAIN = static_cast<uint32>(Economy::Good::Grain);

		uint32 Saturate(uint64 V) noexcept
		{
			return V > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(V);
		}
	} // namespace

	namespace
	{
		/// Write a polity's share onto every region it rules, in region order.
		void Proclaim(World& W, const std::vector<uint32>& RuledBy, const std::vector<EntityHandle>& RegionHandles,
					  uint32 Polity, uint32 PerMille, ComponentType<Economy::RegionDues> Dues)
		{
			for (uint32 R = 1; R < RuledBy.size(); ++R)
			{
				if (RuledBy[R] != Polity || RegionHandles[R].IsNull())
				{
					continue;
				}
				Economy::RegionDues* Owed = W.Components().GetPool(Dues).TryGet(RegionHandles[R]);
				if (Owed == nullptr)
				{
					Economy::RegionDues Fresh;
					Fresh.PerMille = PerMille;
					W.Components().GetPool(Dues).Add(RegionHandles[R], Fresh);
					continue;
				}
				Owed->PerMille = PerMille;
			}
		}
	} // namespace

	LawTypes LawTypes::Declare(World& W)
	{
		LawTypes T;
		T.Law = W.Types().Register<PolityLaw>("PolityLaw");
		T.Hoard = W.Types().Register<Treasury>("Treasury");
		T.Dues = W.Types().Register<Economy::RegionDues>("RegionDues");
		W.Components().CreatePool(T.Law);
		W.Components().CreatePool(T.Hoard);
		W.Components().CreatePool(T.Dues);
		return T;
	}

	void LawSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
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
		const usize N = RegionHandles.size();

		// Whose every region is, and how many live there, in region order.
		std::vector<uint32> RuledBy(N, 0u);
		std::vector<uint64> People(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			const EntityHandle RH = RegionHandles[R];
			if (RH.IsNull())
			{
				continue;
			}
			const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RH);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
			const History::RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			People[R] = Counts != nullptr ? Counts->Total : 0u;
		}

		// The polities, in index order, so that two regions of one polity are
		// always collected in the same order.
		struct Seat
		{
			EntityHandle Handle;
			uint32 Index = 0;
			bool Standing = false;
		};
		std::vector<Seat> Seats;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach([&](EntityHandle H, const PolityInfo& P) { Seats.push_back(Seat{H, P.Index, P.Dissolved == 0}); });
		std::sort(Seats.begin(), Seats.end(), [](const Seat& A, const Seat& B) { return A.Index < B.Index; });

		for (const Seat& S : Seats)
		{
			PolityLaw* Law = W.Components().GetPool(Laws.Law).TryGet(S.Handle);
			if (!S.Standing)
			{
				// A polity that is gone demands nothing more; its treasury stays
				// as the record of what it once took.
				if (Law != nullptr && Law->TaxPerMille != 0)
				{
					Law->TaxPerMille = 0;
					Law->Since = Context.Tick;
					++Law->Changes;
					Context.Events->Publish(Context.Tick, LawChangedEvent, PolityPayload{S.Index, 0, 0, 0},
											W.Entities().GetId(S.Handle));
				}
				continue;
			}
			// A law is written in the year of the founding and does not move in
			// it: the first year is the law's own, and what it demands that year
			// is exactly what the rules say a new polity demands. It is written
			// onto the regions at once, nothing is owed yet, and the collector
			// comes for the first time the year after.
			if (Law == nullptr)
			{
				PolityLaw Fresh;
				Fresh.Polity = S.Index;
				Fresh.TaxPerMille = std::clamp(Rules.TaxAtFounding, Rules.TaxFloor, Rules.TaxCeiling);
				Fresh.Since = Context.Tick;
				W.Components().GetPool(Laws.Law).Add(S.Handle, Fresh);
				W.Components().GetPool(Laws.Hoard).Add(S.Handle, Treasury{});
				Context.Events->Publish(Context.Tick, LawChangedEvent, PolityPayload{S.Index, 0, 0, Fresh.TaxPerMille},
										W.Entities().GetId(S.Handle));
				Proclaim(W, RuledBy, RegionHandles, S.Index, Fresh.TaxPerMille, Laws.Dues);
				continue;
			}
			Treasury* Hoard = W.Components().GetPool(Laws.Hoard).TryGet(S.Handle);
			if (Hoard == nullptr)
			{
				continue;
			}

			// 1. Collect what the regions of this polity owe, in region order.
			uint64 Ruled = 0;
			uint32 Short = 0;
			for (uint32 R = 1; R < N; ++R)
			{
				if (RuledBy[R] != S.Index)
				{
					continue;
				}
				Ruled += People[R];
				const EntityHandle RH = RegionHandles[R];
				Economy::RegionDues* Owed = W.Components().GetPool(Laws.Dues).TryGet(RH);
				if (Owed == nullptr || Owed->Owed == 0)
				{
					continue;
				}
				Economy::RegionStock* Stock = W.Components().GetPool(Economy.Region).TryGet(RH);
				const uint32 Have = Stock != nullptr ? Stock->Amount[G_GRAIN] : 0u;
				const uint32 Took = std::min(Owed->Owed, Have);
				if (Took != 0)
				{
					Stock->Amount[G_GRAIN] -= Took;
					Hoard->Amount[G_GRAIN] = Saturate(uint64{Hoard->Amount[G_GRAIN]} + Took);
					Law->Taken += Took;
					Owed->Owed -= Took;
					Context.Events->Publish(Context.Tick, DuesPaidEvent, PolityPayload{S.Index, R, 0, Took},
											W.Entities().GetId(RH));
				}
				if (Owed->Owed != 0)
				{
					++Short;
					Context.Events->Publish(Context.Tick, DuesUnpaidEvent, PolityPayload{S.Index, R, 0, Owed->Owed},
											W.Entities().GetId(RH));
				}
			}
			Law->Failures = Short != 0 ? Law->Failures + 1u : 0u;

			// 2. Let the law move, then write it down onto every region it rules.
			//    A polity that cannot fill its store demands more; one that is
			//    full, or that has failed to collect for years, demands less.
			const uint64 Want = Ruled * Rules.WantPerPerson;
			const uint64 Held = Hoard->Amount[G_GRAIN];
			uint32 Wanted = Law->TaxPerMille;
			if (Law->Failures >= Rules.FailuresBeforeRelief || (Want != 0 && Held >= Want * Rules.FatMultiple))
			{
				Wanted = Law->TaxPerMille > Rules.TaxStep ? Law->TaxPerMille - Rules.TaxStep : 0u;
			}
			else if (Held < Want)
			{
				Wanted = Law->TaxPerMille + Rules.TaxStep;
			}
			Wanted = std::clamp(Wanted, Rules.TaxFloor, Rules.TaxCeiling);
			if (Wanted != Law->TaxPerMille)
			{
				Law->TaxPerMille = Wanted;
				Law->Since = Context.Tick;
				++Law->Changes;
				Law->Failures = 0;
				Context.Events->Publish(Context.Tick, LawChangedEvent, PolityPayload{S.Index, 0, 0, Wanted},
										W.Entities().GetId(S.Handle));
			}
			Proclaim(W, RuledBy, RegionHandles, S.Index, Law->TaxPerMille, Laws.Dues);
		}

		// 3. A region nobody rules is demanded nothing of. What it still owes it
		//    keeps owing: a polity that comes back finds the arrears waiting.
		for (uint32 R = 1; R < N; ++R)
		{
			if (RuledBy[R] != 0 || RegionHandles[R].IsNull())
			{
				continue;
			}
			Economy::RegionDues* Owed = W.Components().GetPool(Laws.Dues).TryGet(RegionHandles[R]);
			if (Owed != nullptr)
			{
				Owed->PerMille = 0;
			}
		}
	}

	namespace
	{
		EntityHandle SeatOf(const World& W, const PolityTypes& Polities, uint32 Polity)
		{
			EntityHandle Found;
			W.Components()
				.GetPool(Polities.Polity)
				.ForEach(
					[&](EntityHandle H, const PolityInfo& P)
					{
						if (P.Index == Polity && Found.IsNull())
						{
							Found = H;
						}
					});
			return Found;
		}
	} // namespace

	const PolityLaw* LawOf(const World& W, const PolityTypes& Polities, const LawTypes& Laws, uint32 Polity)
	{
		const EntityHandle H = SeatOf(W, Polities, Polity);
		return H.IsNull() ? nullptr : W.Components().GetPool(Laws.Law).TryGet(H);
	}

	const Treasury* TreasuryOf(const World& W, const PolityTypes& Polities, const LawTypes& Laws, uint32 Polity)
	{
		const EntityHandle H = SeatOf(W, Polities, Polity);
		return H.IsNull() ? nullptr : W.Components().GetPool(Laws.Hoard).TryGet(H);
	}

	const Economy::RegionDues* DuesOf(const World& W, const History::PreHistoryTypes& Types, const LawTypes& Laws,
									  uint32 Region)
	{
		const Economy::RegionDues* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Laws.Dues).TryGet(H);
					}
				});
		return Found;
	}

	LawStats MeasureLaws(const World& W, const History::PreHistoryTypes& Types, const PolityTypes& Polities,
						 const LawTypes& Laws, const LawRules& Rules)
	{
		LawStats S;
		std::vector<PolityInfo> All;
		W.Components().GetPool(Polities.Polity).ForEach([&](EntityHandle, const PolityInfo& P) { All.push_back(P); });
		std::sort(All.begin(), All.end(), [](const PolityInfo& A, const PolityInfo& B) { return A.Index < B.Index; });

		struct Held
		{
			uint32 Index = 0;
			PolityLaw Law;
			Treasury Hoard;
			bool Standing = false;
		};
		std::vector<Held> Rows;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle H, const PolityInfo& P)
				{
					const PolityLaw* L = W.Components().GetPool(Laws.Law).TryGet(H);
					const Treasury* T = W.Components().GetPool(Laws.Hoard).TryGet(H);
					if (L == nullptr)
					{
						return;
					}
					Rows.push_back(Held{P.Index, *L, T != nullptr ? *T : Treasury{}, P.Dissolved == 0});
				});
		std::sort(Rows.begin(), Rows.end(), [](const Held& A, const Held& B) { return A.Index < B.Index; });

		std::vector<std::pair<uint32, Economy::RegionDues>> Dues_;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const Economy::RegionDues* D = W.Components().GetPool(Laws.Dues).TryGet(H);
					if (D != nullptr)
					{
						Dues_.push_back({R.Index, *D});
					}
				});
		std::sort(Dues_.begin(), Dues_.end(), [](const auto& A, const auto& B) { return A.first < B.first; });

		// Whose every region is, for the check that nobody demands without ruling.
		std::vector<uint32> RuledBy;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(H);
					if (R.Index >= RuledBy.size())
					{
						RuledBy.resize(usize{R.Index} + 1u, 0u);
					}
					RuledBy[R.Index] = Rule != nullptr ? Rule->Polity : 0u;
				});

		Hash64 D = HashString("Law");
		for (const Held& Row : Rows)
		{
			++S.Laws;
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Row.Law), sizeof(Row.Law)));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Row.Hoard), sizeof(Row.Hoard)));
			S.Held += Row.Hoard.Amount[G_GRAIN];
			if (!Row.Standing)
			{
				S.Bad += Row.Law.TaxPerMille != 0 ? 1u : 0u; // a polity that is gone still demanding
				continue;
			}
			S.Bad += Row.Law.TaxPerMille < Rules.TaxFloor || Row.Law.TaxPerMille > Rules.TaxCeiling ? 1u : 0u;
		}
		for (const auto& [Index, Owed] : Dues_)
		{
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Owed), sizeof(Owed)));
			S.Taxed += Owed.PerMille != 0 ? 1u : 0u;
			S.Owing += Owed.Owed != 0 ? 1u : 0u;
			S.Arrears += Owed.Owed;
			const uint32 Master = Index < RuledBy.size() ? RuledBy[Index] : 0u;
			if (Owed.PerMille != 0 && Master == 0)
			{
				++S.Bad; // demanded of by nobody
			}
		}
		for (const Event& E : W.Log().All())
		{
			S.Paid += E.Is(DuesPaidEvent) ? 1u : 0u;
			S.Unpaid += E.Is(DuesUnpaidEvent) ? 1u : 0u;
			S.Changes += E.Is(LawChangedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Politics
