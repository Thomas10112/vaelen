// VAELEN - VaelenPolitics
// Phase 07.05: factions.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics

#include "Vaelen/Politics/Factions.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Politics
{
	namespace
	{
		constexpr uint64 FactionSalt = 0x464143544e; // "FACTN"
	} // namespace

	const char* GrievanceName(Grievance G) noexcept
	{
		switch (G)
		{
		case Grievance::PassedOver:
			return "PassedOver";
		case Grievance::Neglect:
			return "Neglect";
		default:
			return "Unknown";
		}
	}

	FactionTypes FactionTypes::Declare(World& W)
	{
		FactionTypes T;
		T.Faction = W.Types().Register<FactionInfo>("FactionInfo");
		T.Patience = W.Types().Register<RegionPatience>("RegionPatience");
		W.Components().CreatePool(T.Faction);
		W.Components().CreatePool(T.Patience);
		return T;
	}

	void FactionSystem::Tick(TickContext& Context)
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
		if (N <= 1)
		{
			return;
		}
		std::vector<uint32> RuledBy(N, 0u);
		std::vector<uint32> HoldOf(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
			const RegionAuthority* A = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
			HoldOf[R] = A != nullptr ? A->Hold : 0u;
		}

		struct Seat
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Ruler = 0;
			uint32 Region = 0;
			bool Standing = false;
		};
		std::vector<Seat> Seats;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach([&](EntityHandle H, const PolityInfo& P)
					 { Seats.push_back(Seat{H, P.Index, P.Ruler, P.Seat, P.Dissolved == 0}); });
		std::sort(Seats.begin(), Seats.end(), [](const Seat& A, const Seat& B) { return A.Index < B.Index; });
		std::vector<uint32> Capitals;
		for (const Seat& S : Seats)
		{
			if (S.Standing && S.Region != 0)
			{
				Capitals.push_back(S.Region);
			}
		}
		std::sort(Capitals.begin(), Capitals.end());
		auto IsCapital = [&](uint32 Region) { return std::binary_search(Capitals.begin(), Capitals.end(), Region); };
		auto SeatOf = [&](uint32 Polity) -> const Seat*
		{
			const auto At = std::lower_bound(Seats.begin(), Seats.end(), Polity,
											 [](const Seat& S, uint32 V) { return S.Index < V; });
			return At != Seats.end() && At->Index == Polity ? &*At : nullptr;
		};

		// 1. The factions that already stand, in index order.
		struct Standing
		{
			EntityHandle Handle;
			uint32 Index = 0;
		};
		std::vector<Standing> Order;
		uint32 Highest = 0;
		W.Components()
			.GetPool(Factions.Faction)
			.ForEach(
				[&](EntityHandle H, const FactionInfo& F)
				{
					Highest = std::max(Highest, F.Index);
					if (F.Ended == 0)
					{
						Order.push_back(Standing{H, F.Index});
					}
				});
		std::sort(Order.begin(), Order.end(), [](const Standing& A, const Standing& B) { return A.Index < B.Index; });

		std::vector<uint32> Alive; // polities that still have a faction after this pass
		for (const Standing& It : Order)
		{
			FactionInfo* F = W.Components().GetPool(Factions.Faction).TryGet(It.Handle);
			if (F == nullptr)
			{
				continue;
			}
			const Seat* Master = SeatOf(F->Polity);
			const bool Lost =
				Master == nullptr || !Master->Standing || F->Region >= N || RuledBy[F->Region] != F->Polity;
			bool Aggrieved = false;
			if (!Lost)
			{
				if (F->Cause == static_cast<uint32>(Grievance::PassedOver))
				{
					const Population::PersonInfo* Claimant = Population::FindPerson(W, Persons, F->Claimant);
					const bool Living =
						Claimant != nullptr && Claimant->State == static_cast<uint8>(Population::LifeState::Alive);
					// Answered when the one they wanted is the one who sits.
					Aggrieved = Living && Master->Ruler != F->Claimant;
					if (!Living)
					{
						F->Ended = Context.Tick;
						Context.Events->Publish(Context.Tick, FactionFadedEvent,
												PolityPayload{F->Polity, F->Region, F->Claimant, F->Strength},
												W.Entities().GetId(It.Handle));
						continue;
					}
				}
				else
				{
					Aggrieved = !IsCapital(F->Region) && HoldOf[F->Region] < Rules.NeglectUnderHold;
				}
			}
			if (Lost)
			{
				F->Ended = Context.Tick;
				Context.Events->Publish(Context.Tick, FactionFadedEvent,
										PolityPayload{F->Polity, F->Region, F->Claimant, F->Strength},
										W.Entities().GetId(It.Handle));
				continue;
			}
			if (Aggrieved)
			{
				F->Strength = std::min(F->Strength + Rules.StrengthPerYearAggrieved, Rules.StrengthCeiling);
			}
			else
			{
				F->Strength = F->Strength > Rules.StrengthLostPerYear ? F->Strength - Rules.StrengthLostPerYear : 0u;
			}
			if (F->Strength == 0)
			{
				F->Ended = Context.Tick;
				Context.Events->Publish(Context.Tick, FactionFadedEvent,
										PolityPayload{F->Polity, F->Region, F->Claimant, 0},
										W.Entities().GetId(It.Handle));
				continue;
			}
			if (F->Strength >= Rules.RevoltAt)
			{
				// It takes the ground and is done: it wanted that, and it has it.
				RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[F->Region]);
				if (Rule != nullptr)
				{
					Rule->Polity = 0;
					Rule->Since = Context.Tick;
				}
				RegionAuthority* A = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[F->Region]);
				if (A != nullptr)
				{
					A->Polity = 0;
					A->Hold = 0;
					A->Distance = 0;
				}
				RuledBy[F->Region] = 0;
				if (HasDues)
				{
					Economy::RegionDues* Owed = W.Components().GetPool(Dues).TryGet(RegionHandles[F->Region]);
					if (Owed != nullptr)
					{
						Owed->PerMille = 0;
					}
				}
				F->Ended = Context.Tick;
				Context.Events->Publish(Context.Tick, FactionRevoltedEvent,
										PolityPayload{F->Polity, F->Region, F->Claimant, F->Strength},
										W.Entities().GetId(RegionHandles[F->Region]));
				continue;
			}
			Alive.push_back(F->Polity);
		}

		// 2. The years of loose holding, counted on the region itself.
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			RegionPatience* P = W.Components().GetPool(Factions.Patience).TryGet(RegionHandles[R]);
			// A polity's own seat is not a province of it: it cannot be neglected
			// by itself, and 07.03 already refuses to let it slip.
			const bool Loose = RuledBy[R] != 0 && !IsCapital(R) && HoldOf[R] < Rules.NeglectUnderHold;
			if (P == nullptr)
			{
				if (!Loose)
				{
					continue;
				}
				W.Components().GetPool(Factions.Patience).Add(RegionHandles[R], RegionPatience{RuledBy[R], 1u});
				continue;
			}
			if (!Loose || P->Polity != RuledBy[R])
			{
				P->Polity = RuledBy[R];
				P->Years = Loose ? 1u : 0u;
				continue;
			}
			++P->Years;
		}

		// 3. What this year has given cause for. A polity bears one faction at a
		//    time: a second grievance waits its turn rather than piling on.
		std::sort(Alive.begin(), Alive.end());
		auto Free = [&](uint32 Polity) { return !std::binary_search(Alive.begin(), Alive.end(), Polity); };

		auto Form = [&](uint32 Polity, uint32 Region, uint32 Claimant, Grievance Cause)
		{
			++Highest;
			FactionInfo Fresh;
			Fresh.Index = Highest;
			Fresh.Polity = Polity;
			Fresh.Region = Region;
			Fresh.Claimant = Claimant;
			Fresh.Strength = Rules.StrengthAtBirth;
			Fresh.Cause = static_cast<uint32>(Cause);
			Fresh.Formed = Context.Tick;
			Fresh.Identity = Noise::LatticeHash(W.Config().Seed ^ FactionSalt, static_cast<int32>(Fresh.Index),
												static_cast<int32>(Region));
			const EntityHandle H = W.CreateEntity(IdKind::Faction);
			W.Components().GetPool(Factions.Faction).Add(H, Fresh);
			Alive.insert(std::lower_bound(Alive.begin(), Alive.end(), Polity), Polity);
			Context.Events->Publish(Context.Tick, FactionFormedEvent,
									PolityPayload{Polity, Region, Claimant, static_cast<uint32>(Cause)},
									W.Entities().GetId(H));
		};

		// A claimant the council passed over this very year, from the log.
		const std::vector<Event>& Log = W.Log().All();
		for (usize i = Log.size(); i > 0; --i)
		{
			const Event& E = Log[i - 1];
			if (E.Tick != Context.Tick)
			{
				break;
			}
			if (!E.Is(SuccessionDisputedEvent))
			{
				continue;
			}
			const PolityPayload P = E.Get<PolityPayload>();
			const uint32 PassedOver = P.Value;
			if (PassedOver == 0 || !Free(P.Polity))
			{
				continue;
			}
			const Population::PersonInfo* Claimant = Population::FindPerson(W, Persons, PassedOver);
			// A faction takes ground, never the throne. One that could only rise
			// in the seat itself has nothing to take: the grievance of a passed
			// over claimant at the capital is already in the polity's unrest.
			if (Claimant == nullptr || Claimant->State != static_cast<uint8>(Population::LifeState::Alive) ||
				Claimant->Region >= N || RuledBy[Claimant->Region] != P.Polity || IsCapital(Claimant->Region))
			{
				continue;
			}
			Form(P.Polity, Claimant->Region, PassedOver, Grievance::PassedOver);
		}

		// A region held too loosely for too long, in region order.
		for (uint32 R = 1; R < N; ++R)
		{
			if (RuledBy[R] == 0 || !Free(RuledBy[R]))
			{
				continue;
			}
			const RegionPatience* P = W.Components().GetPool(Factions.Patience).TryGet(RegionHandles[R]);
			if (P == nullptr || P->Years < Rules.NeglectYears)
			{
				continue;
			}
			Form(RuledBy[R], R, 0, Grievance::Neglect);
		}

		// 4. What every standing faction costs the polity it is inside. Written
		//    on the line, felt by the reach the year after: a faction is not news
		//    the day it forms.
		for (const Seat& S : Seats)
		{
			if (!S.Standing)
			{
				continue;
			}
			PolityLine* Line = W.Components().GetPool(Lines.Line).TryGet(S.Handle);
			if (Line == nullptr)
			{
				continue;
			}
			const usize Count = static_cast<usize>(std::count(Alive.begin(), Alive.end(), S.Index));
			if (Count == 0)
			{
				continue;
			}
			Line->Unrest =
				std::min(Line->Unrest + static_cast<uint32>(Count) * Rules.UnrestPerFaction, Rules.UnrestCeiling);
		}
	}

	const FactionInfo* FactionOf(const World& W, const FactionTypes& Factions, uint32 Faction)
	{
		const FactionInfo* Found = nullptr;
		W.Components()
			.GetPool(Factions.Faction)
			.ForEach(
				[&](EntityHandle H, const FactionInfo& F)
				{
					if (F.Index == Faction && Found == nullptr)
					{
						Found = W.Components().GetPool(Factions.Faction).TryGet(H);
					}
				});
		return Found;
	}

	void FactionsOf(const World& W, const FactionTypes& Factions, uint32 Polity, std::vector<uint32>& Out)
	{
		Out.clear();
		if (Polity == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Factions.Faction)
			.ForEach(
				[&](EntityHandle, const FactionInfo& F)
				{
					if (F.Polity == Polity && F.Ended == 0)
					{
						Out.push_back(F.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	FactionStats MeasureFactions(const World& W, const History::PreHistoryTypes& Types,
								 const Population::PersonTypes& Persons, const PolityTypes& Polities,
								 const FactionTypes& Factions, const FactionRules& Rules)
	{
		FactionStats S;
		std::vector<uint32> StandingPolities;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle, const PolityInfo& P)
				{
					if (P.Dissolved == 0)
					{
						StandingPolities.push_back(P.Index);
					}
				});
		std::sort(StandingPolities.begin(), StandingPolities.end());

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

		std::vector<FactionInfo> All;
		W.Components().GetPool(Factions.Faction).ForEach([&](EntityHandle, const FactionInfo& F) { All.push_back(F); });
		std::sort(All.begin(), All.end(), [](const FactionInfo& A, const FactionInfo& B) { return A.Index < B.Index; });

		std::vector<std::pair<uint32, RegionPatience>> Waits;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionPatience* P = W.Components().GetPool(Factions.Patience).TryGet(H);
					if (P != nullptr)
					{
						Waits.push_back({R.Index, *P});
					}
				});
		std::sort(Waits.begin(), Waits.end(), [](const auto& A, const auto& B) { return A.first < B.first; });

		Hash64 D = HashString("Factions");
		for (const FactionInfo& F : All)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&F), sizeof(F)));
			if (F.Ended != 0)
			{
				++S.Ended;
				continue;
			}
			++S.Standing;
			S.Strongest = std::max(S.Strongest, F.Strength);
			S.Bad += F.Strength > Rules.StrengthCeiling ? 1u : 0u;
			S.Bad += std::binary_search(StandingPolities.begin(), StandingPolities.end(), F.Polity) ? 0u : 1u;
			S.Bad += F.Region < RuledBy.size() && RuledBy[F.Region] == F.Polity ? 0u : 1u;
			if (F.Claimant != 0)
			{
				const Population::PersonInfo* Claimant = Population::FindPerson(W, Persons, F.Claimant);
				S.Bad += Claimant == nullptr || Claimant->State != static_cast<uint8>(Population::LifeState::Alive)
							 ? 1u
							 : 0u;
			}
		}
		for (const auto& [Region, Wait] : Waits)
		{
			D = HashCombine(D, HashUInt64(Region));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Wait), sizeof(Wait)));
			S.Aggrieved += Wait.Years != 0 ? 1u : 0u;
		}
		for (const Event& E : W.Log().All())
		{
			S.Formed += E.Is(FactionFormedEvent) ? 1u : 0u;
			S.Revolts += E.Is(FactionRevoltedEvent) ? 1u : 0u;
			S.Faded += E.Is(FactionFadedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Politics
