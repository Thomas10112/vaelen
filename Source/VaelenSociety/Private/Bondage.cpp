// VAELEN - VaelenSociety
// Phase 05.04: bondage and slavery.
//
// STATUS: VALIDATED (Phase 05) - unit/integration/edge tests in Tests/Society

#include "Vaelen/Society/Bondage.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Society
{
	using Population::LifeState;
	using Population::PersonInfo;

	namespace
	{
		struct Ref
		{
			EntityHandle Handle;
			PersonInfo Info;
		};

		bool IsAlive(const PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(LifeState::Alive);
		}
	} // namespace

	const char* BondKindName(BondKind K) noexcept
	{
		switch (K)
		{
		case BondKind::Free:
			return "free";
		case BondKind::Bonded:
			return "bonded";
		case BondKind::Enslaved:
			return "enslaved";
		default:
			return "?";
		}
	}

	const char* BondEntryName(BondEntry E) noexcept
	{
		switch (E)
		{
		case BondEntry::None:
			return "none";
		case BondEntry::Debt:
			return "debt";
		case BondEntry::Birth:
			return "birth";
		case BondEntry::Capture:
			return "capture";
		case BondEntry::Promotion:
			return "the region's strata";
		default:
			return "?";
		}
	}

	const char* BondExitName(BondExit E) noexcept
	{
		switch (E)
		{
		case BondExit::None:
			return "none";
		case BondExit::Manumission:
			return "manumission";
		case BondExit::Flight:
			return "flight";
		case BondExit::HolderDied:
			return "the holder's death";
		case BondExit::Death:
			return "death";
		case BondExit::Departure:
			return "departure";
		default:
			return "?";
		}
	}

	BondageTypes BondageTypes::Declare(World& W)
	{
		BondageTypes T;
		T.Bond = W.Types().Register<BondState>("BondState");
		T.Strata = W.Types().Register<RegionStrata>("RegionStrata");
		W.Components().CreatePool(T.Bond);
		W.Components().CreatePool(T.Strata);
		return T;
	}

	void BondageSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr || Context.Random == nullptr)
		{
			return;
		}
		World& W = *Owner;
		RandomStream& Random = *Context.Random;
		std::vector<uint32> Regions;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const Population::RegionDetail& D) { Regions.push_back(D.Region); });
		std::sort(Regions.begin(), Regions.end());
		if (Regions.empty())
		{
			return;
		}
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
		// The customs of every culture (bondage bits), once.
		std::vector<uint32> Allowed;
		W.Components()
			.GetPool(Types.Population.Culture)
			.ForEach(
				[&](EntityHandle H, const History::CultureInfo& C)
				{
					const NormSet* N = W.Components().GetPool(Norms.Norms).TryGet(H);
					if (C.Index >= Allowed.size())
					{
						Allowed.resize(usize{C.Index} + 1u, 0u);
					}
					Allowed[C.Index] = N != nullptr ? N->BondageAllowed : 0u;
				});
		auto Allows = [&](uint32 Culture, Bondage Institution)
		{ return Culture < Allowed.size() && (Allowed[Culture] & static_cast<uint32>(Institution)) != 0; };
		// Last year's deaths and births, for causes.
		const std::vector<Event>& Events = W.Log().All();
		std::vector<std::pair<uint32, PersistentId>> Died; // person -> death event
		std::vector<std::pair<uint32, PersistentId>> Born; // person -> birth event
		for (usize i = Events.size(); i > 0; --i)
		{
			const Event& E = Events[i - 1];
			if (E.Tick + History::TicksPerYear <= Context.Tick)
			{
				break;
			}
			if (E.Is(Population::PersonDiedEvent))
			{
				Died.push_back({E.Get<Population::PersonPayload>().Person, E.Id});
			}
			else if (E.Is(Population::PersonBornEvent))
			{
				Born.push_back({E.Get<Population::PersonPayload>().Person, E.Id});
			}
		}
		auto EventOf = [](const std::vector<std::pair<uint32, PersistentId>>& List, uint32 Person)
		{
			for (const auto& [Index, Id] : List)
			{
				if (Index == Person)
				{
					return Id;
				}
			}
			return PersistentId{};
		};

		for (const uint32 Region : Regions)
		{
			if (Region >= RegionHandles.size() || RegionHandles[Region].IsNull())
			{
				continue;
			}
			std::vector<Ref> People;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region == Region)
						{
							People.push_back(Ref{H, P});
						}
					});
			std::sort(People.begin(), People.end(),
					  [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
			auto FindRef = [&](uint32 Index) -> const Ref*
			{
				for (const Ref& R : People)
				{
					if (R.Info.Index == Index)
					{
						return &R;
					}
				}
				return nullptr;
			};
			// Holders: the elite, each with a count of the held.
			std::vector<std::pair<uint32, uint32>> Holders; // person index, held
			for (const Ref& R : People)
			{
				const PersonStanding* S = W.Components().GetPool(Standing.Standing).TryGet(R.Handle);
				if (IsAlive(R.Info) && S != nullptr && S->Tier_ == static_cast<uint8>(Tier::Elite) &&
					W.Components().GetPool(Bonds.Bond).TryGet(R.Handle) == nullptr)
				{
					Holders.push_back({R.Info.Index, 0u});
				}
			}
			for (const Ref& R : People)
			{
				const BondState* B = W.Components().GetPool(Bonds.Bond).TryGet(R.Handle);
				if (B == nullptr || !IsAlive(R.Info))
				{
					continue;
				}
				for (auto& [Holder, Held] : Holders)
				{
					Held += Holder == B->Holder ? 1u : 0u;
				}
			}
			auto Leave = [&](const Ref& R, BondExit Exit, PersistentId Cause)
			{
				const BondState* B = W.Components().GetPool(Bonds.Bond).TryGet(R.Handle);
				const uint32 Kind = B != nullptr ? B->Kind : 0u;
				W.Components().GetPool(Bonds.Bond).Remove(R.Handle);
				Context.Events->Publish(Context.Tick, BondLeftEvent,
										BondPayload{R.Info.Index, Region, Kind, static_cast<uint32>(Exit)},
										W.Entities().GetId(R.Handle), Cause);
			};
			auto NextHolder = [&]() -> uint32
			{
				uint32 Best = 0;
				uint32 Fewest = 0xffffffffu;
				for (const auto& [Holder, Held] : Holders)
				{
					if (Held < Fewest && Held < Rules.MaxHeldPerHolder)
					{
						Fewest = Held;
						Best = Holder;
					}
				}
				for (auto& [Holder, Held] : Holders)
				{
					Held += Holder == Best ? 1u : 0u;
				}
				return Best;
			};
			// 0. A region detailed again: its strata bind the new persons before
			//    anything else happens to them (the social shape survives the grain).
			bool AnyBond = false;
			for (const Ref& R : People)
			{
				AnyBond =
					AnyBond || (IsAlive(R.Info) && W.Components().GetPool(Bonds.Bond).TryGet(R.Handle) != nullptr);
			}
			const RegionStrata* Kept = W.Components().GetPool(Bonds.Strata).TryGet(RegionHandles[Region]);
			if (!AnyBond && Kept != nullptr && Kept->Bonded + Kept->Enslaved > 0 && !Holders.empty())
			{
				uint32 ToEnslave = Kept->Enslaved;
				uint32 ToBind = Kept->Bonded;
				for (const Ref& R : People)
				{
					if (ToEnslave + ToBind == 0)
					{
						break;
					}
					const PersonStanding* S = W.Components().GetPool(Standing.Standing).TryGet(R.Handle);
					if (!IsAlive(R.Info) || S == nullptr || S->Tier_ != static_cast<uint8>(Tier::Common) ||
						Population::AgeYears(R.Info, Context.Tick) < Rules.DebtFromAge)
					{
						continue;
					}
					const uint32 Holder = NextHolder();
					if (Holder == 0)
					{
						break;
					}
					const bool Enslave = ToEnslave > 0;
					BondState B;
					B.Kind = static_cast<uint8>(Enslave ? BondKind::Enslaved : BondKind::Bonded);
					B.Entry = static_cast<uint8>(BondEntry::Promotion);
					B.Holder = Holder;
					B.Since = Context.Tick;
					if (Enslave)
					{
						--ToEnslave;
					}
					else
					{
						--ToBind;
					}
					W.Components().GetPool(Bonds.Bond).Add(R.Handle, B);
					Context.Events->Publish(Context.Tick, BondEnteredEvent,
											BondPayload{R.Info.Index, Region, B.Kind, B.Entry},
											W.Entities().GetId(R.Handle));
				}
			}
			// 1. Exits: the dead, the departed, the freed, the fled, the bonded whose holder died.
			for (const Ref& R : People)
			{
				const BondState* B = W.Components().GetPool(Bonds.Bond).TryGet(R.Handle);
				if (B == nullptr)
				{
					continue;
				}
				if (!IsAlive(R.Info))
				{
					const bool Gone = R.Info.State == static_cast<uint8>(LifeState::Gone);
					Leave(R, Gone ? BondExit::Departure : BondExit::Death,
						  Gone ? PersistentId{} : EventOf(Died, R.Info.Index));
					continue;
				}
				const Ref* Holder = B->Holder != 0 ? FindRef(B->Holder) : nullptr;
				const bool HolderGone = B->Holder != 0 && (Holder == nullptr || !IsAlive(Holder->Info));
				if (HolderGone && B->Kind == static_cast<uint8>(BondKind::Bonded))
				{
					Leave(R, BondExit::HolderDied, EventOf(Died, B->Holder));
					continue;
				}
				if (Random.Below(1000) < Rules.ManumissionPerMille)
				{
					Leave(R, BondExit::Manumission, PersistentId{});
					continue;
				}
				if (Random.Below(1000) < Rules.FlightPerMille)
				{
					Leave(R, BondExit::Flight, PersistentId{});
					continue;
				}
				BondState& Live = W.Components().GetPool(Bonds.Bond).Get(R.Handle);
				if (HolderGone)
				{
					Live.Holder = NextHolder(); // the enslaved pass to the next holder
				}
				// 2. Hardening: bondage unredeemed for years becomes slavery.
				if (Live.Kind == static_cast<uint8>(BondKind::Bonded) &&
					Context.Tick >= Live.Since + uint64{Rules.HardenAfterYears} * History::TicksPerYear)
				{
					Live.Kind = static_cast<uint8>(BondKind::Enslaved);
					Context.Events->Publish(Context.Tick, BondEnteredEvent,
											BondPayload{R.Info.Index, Region, Live.Kind, Live.Entry},
											W.Entities().GetId(R.Handle));
				}
			}
			// 3. Entries: by birth to an enslaved mother, by debt for the common.
			for (const Ref& R : People)
			{
				if (!IsAlive(R.Info) || W.Components().GetPool(Bonds.Bond).TryGet(R.Handle) != nullptr)
				{
					continue;
				}
				const PersistentId Birth = EventOf(Born, R.Info.Index);
				if (Birth.IsValid() && Rules.BirthFollowsMother != 0 && Allows(R.Info.Culture, Bondage::Birth))
				{
					const Ref* Mother = FindRef(R.Info.Mother);
					const BondState* M =
						Mother != nullptr ? W.Components().GetPool(Bonds.Bond).TryGet(Mother->Handle) : nullptr;
					if (M != nullptr && M->Kind == static_cast<uint8>(BondKind::Enslaved))
					{
						BondState B;
						B.Kind = static_cast<uint8>(BondKind::Enslaved);
						B.Entry = static_cast<uint8>(BondEntry::Birth);
						B.Holder = M->Holder;
						B.Since = Context.Tick;
						W.Components().GetPool(Bonds.Bond).Add(R.Handle, B);
						Context.Events->Publish(Context.Tick, BondEnteredEvent,
												BondPayload{R.Info.Index, Region, B.Kind, B.Entry},
												W.Entities().GetId(R.Handle), Birth);
						continue;
					}
				}
				if (!Allows(R.Info.Culture, Bondage::Debt) ||
					Population::AgeYears(R.Info, Context.Tick) < Rules.DebtFromAge)
				{
					continue;
				}
				const PersonStanding* S = W.Components().GetPool(Standing.Standing).TryGet(R.Handle);
				if (S == nullptr || S->Tier_ != static_cast<uint8>(Tier::Common) || Holders.empty())
				{
					continue;
				}
				if (Random.Below(1000) >= Rules.DebtPerMille)
				{
					continue;
				}
				const uint32 Holder = NextHolder();
				if (Holder == 0)
				{
					continue; // every holder is full
				}
				BondState B;
				B.Kind = static_cast<uint8>(BondKind::Bonded);
				B.Entry = static_cast<uint8>(BondEntry::Debt);
				B.Holder = Holder;
				B.Since = Context.Tick;
				W.Components().GetPool(Bonds.Bond).Add(R.Handle, B);
				Context.Events->Publish(Context.Tick, BondEnteredEvent,
										BondPayload{R.Info.Index, Region, B.Kind, B.Entry},
										W.Entities().GetId(R.Handle));
			}
			// 4. The strata, from the living.
			RegionStrata Strata;
			for (const Ref& R : People)
			{
				if (!IsAlive(R.Info))
				{
					continue;
				}
				const BondState* B = W.Components().GetPool(Bonds.Bond).TryGet(R.Handle);
				if (B == nullptr)
				{
					++Strata.Free;
				}
				else if (B->Kind == static_cast<uint8>(BondKind::Enslaved))
				{
					++Strata.Enslaved;
				}
				else
				{
					++Strata.Bonded;
				}
			}
			RegionStrata* Existing = W.Components().GetPool(Bonds.Strata).TryGet(RegionHandles[Region]);
			if (Existing != nullptr)
			{
				*Existing = Strata;
			}
			else
			{
				W.Components().GetPool(Bonds.Strata).Add(RegionHandles[Region], Strata);
			}
		}
	}

	const BondState* BondOf(const World& W, const Population::PersonTypes& Persons, const BondageTypes& Types,
							uint32 Person)
	{
		const BondState* Found = nullptr;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index == Person && Found == nullptr)
					{
						Found = W.Components().GetPool(Types.Bond).TryGet(H);
					}
				});
		return Found;
	}

	const RegionStrata* StrataOf(const World& W, const History::PreHistoryTypes& Types, const BondageTypes& Types_,
								 uint32 Region)
	{
		const RegionStrata* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Types_.Strata).TryGet(H);
					}
				});
		return Found;
	}

	BondageStats MeasureBondage(const World& W, const History::PreHistoryTypes& Types,
								const Population::PersonTypes& Persons, const BondageTypes& Types_, uint32 Region)
	{
		BondageStats S;
		struct Row
		{
			uint32 Index;
			BondState Bond;
		};
		std::vector<Row> Rows;
		std::vector<PersonInfo> Living;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					if (IsAlive(P))
					{
						Living.push_back(P);
					}
				});
		W.Components()
			.GetPool(Types_.Bond)
			.ForEach(
				[&](EntityHandle H, const BondState& B)
				{
					const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					if (P == nullptr)
					{
						++S.Stale;
						return;
					}
					Rows.push_back(Row{P->Index, B});
					if (!IsAlive(*P))
					{
						++S.Stale;
						return;
					}
					if (Region != 0 && P->Region != Region)
					{
						return;
					}
					S.Bonded += B.Kind == static_cast<uint8>(BondKind::Bonded) ? 1u : 0u;
					S.Enslaved += B.Kind == static_cast<uint8>(BondKind::Enslaved) ? 1u : 0u;
					bool HolderAlive = B.Holder == 0;
					for (const PersonInfo& L : Living)
					{
						HolderAlive = HolderAlive || (L.Index == B.Holder && L.Region == P->Region);
					}
					S.HolderLost += HolderAlive ? 0u : 1u;
				});
		for (const Event& E : W.Log().All())
		{
			if (E.Is(BondEnteredEvent))
			{
				const BondPayload P = E.Get<BondPayload>();
				if (Region == 0 || P.Region == Region)
				{
					++S.Entered[P.Reason < 5 ? P.Reason : 0];
					S.Caused += E.Cause.IsValid() ? 1u : 0u;
				}
			}
			else if (E.Is(BondLeftEvent))
			{
				const BondPayload P = E.Get<BondPayload>();
				if (Region == 0 || P.Region == Region)
				{
					++S.Left[P.Reason < 6 ? P.Reason : 0];
					S.Caused += E.Cause.IsValid() ? 1u : 0u;
				}
			}
		}
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Index < B.Index; });
		Hash64 D = HashString("BondState");
		for (const Row& R : Rows)
		{
			D = HashCombine(D, HashUInt64(R.Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R.Bond), sizeof(R.Bond)));
		}
		std::vector<std::pair<uint32, RegionStrata>> Strata;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionStrata* St = W.Components().GetPool(Types_.Strata).TryGet(H);
					if (St != nullptr)
					{
						Strata.push_back({R.Index, *St});
					}
				});
		std::sort(Strata.begin(), Strata.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		for (const auto& [Index, St] : Strata)
		{
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&St), sizeof(St)));
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Society
