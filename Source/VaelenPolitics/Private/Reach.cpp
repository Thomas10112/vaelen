// VAELEN - VaelenPolitics
// Phase 07.03: authority and reach.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics

#include "Vaelen/Politics/Reach.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <queue>

namespace Vaelen::Politics
{
	namespace
	{
		constexpr uint32 G_GRAIN = static_cast<uint32>(Economy::Good::Grain);
		constexpr uint32 Unreached = 0xffffffffu;
	} // namespace

	ReachTypes ReachTypes::Declare(World& W)
	{
		ReachTypes T;
		T.Authority = W.Types().Register<RegionAuthority>("RegionAuthority");
		T.Reach = W.Types().Register<PolityReach>("PolityReach");
		W.Components().CreatePool(T.Authority);
		W.Components().CreatePool(T.Reach);
		return T;
	}

	void ReachSystem::Tick(TickContext& Context)
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
		// The region graph does not change once the world is generated; it is
		// rebuilt only when the count of regions does, which is never after
		// generation but is once at a snapshot restored into a fresh world.
		if (GraphRegions != static_cast<uint32>(N))
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Types.World.Regions);
			GraphRegions = static_cast<uint32>(N);
		}

		std::vector<uint32> RuledBy(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
		}

		struct Seat
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Region = 0;
			bool Standing = false;
		};
		std::vector<Seat> Seats;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach([&](EntityHandle H, const PolityInfo& P)
					 { Seats.push_back(Seat{H, P.Index, P.Seat, P.Dissolved == 0}); });
		std::sort(Seats.begin(), Seats.end(), [](const Seat& A, const Seat& B) { return A.Index < B.Index; });

		std::vector<uint32> Distance(N, Unreached);
		std::vector<uint32> Mine;
		for (const Seat& S : Seats)
		{
			PolityReach* Far = W.Components().GetPool(Reaches.Reach).TryGet(S.Handle);
			if (!S.Standing)
			{
				continue; // a polity that is gone holds nothing; its regions were freed at its end
			}
			if (Far == nullptr)
			{
				PolityReach Fresh;
				Fresh.Polity = S.Index;
				Fresh.Reach = 1;
				W.Components().GetPool(Reaches.Reach).Add(S.Handle, Fresh);
				Far = W.Components().GetPool(Reaches.Reach).TryGet(S.Handle);
				if (Far == nullptr)
				{
					continue;
				}
			}

			// 1. How far every region it rules is from the seat, walking only
			//    through ground it holds. What it cannot walk to is unreached.
			std::fill(Distance.begin(), Distance.end(), Unreached);
			Mine.clear();
			if (S.Region < N && RuledBy[S.Region] == S.Index)
			{
				Distance[S.Region] = 0;
				std::queue<uint32> Walk;
				Walk.push(S.Region);
				while (!Walk.empty())
				{
					const uint32 At = Walk.front();
					Walk.pop();
					Mine.push_back(At);
					if (At >= Graph.Neighbours.size())
					{
						continue;
					}
					for (const uint16 Next : Graph.Neighbours[At])
					{
						if (Next < N && RuledBy[Next] == S.Index && Distance[Next] == Unreached)
						{
							Distance[Next] = Distance[At] + 1u;
							Walk.push(Next);
						}
					}
				}
			}
			std::sort(Mine.begin(), Mine.end());

			// 2. What it costs to carry a word that far, paid out of the treasury.
			uint64 Upkeep = 0;
			for (const uint32 R : Mine)
			{
				Upkeep += uint64{Distance[R]} * Rules.UpkeepPerHop;
			}
			Treasury* Hoard = W.Components().GetPool(Laws.Hoard).TryGet(S.Handle);
			uint64 Short = Upkeep;
			if (Upkeep != 0 && Hoard != nullptr)
			{
				const uint64 Paid = std::min<uint64>(Hoard->Amount[G_GRAIN], Upkeep);
				Hoard->Amount[G_GRAIN] -= static_cast<uint32>(Paid);
				Far->Spent += Paid;
				Short = Upkeep - Paid;
			}
			Far->Unpaid = Short;
			if (Short != 0)
			{
				Context.Events->Publish(
					Context.Tick, UpkeepUnpaidEvent,
					PolityPayload{S.Index, 0, 0, Short > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(Short)},
					W.Entities().GetId(S.Handle));
			}

			// 3. Write the hold of every region it rules, and let go what is
			//    under the floor. The seat is never let go: a polity without a
			//    seat is 07.01's business, not this system's.
			// What the polity cannot pay for, and what it has not settled at home,
			// both come off the hold of every region alike.
			const PolityLine* Trouble = HasLine ? W.Components().GetPool(Line).TryGet(S.Handle) : nullptr;
			const uint32 Loss = (Short != 0 ? Rules.UnpaidHoldLoss : 0u) + (Trouble != nullptr ? Trouble->Unrest : 0u);
			for (uint32 R = 1; R < N; ++R)
			{
				if (RuledBy[R] != S.Index || RegionHandles[R].IsNull())
				{
					continue;
				}
				const uint32 Hops = Distance[R];
				uint32 Hold = 0;
				if (Hops != Unreached)
				{
					const uint64 Fall = uint64{Hops} * Rules.HoldLostPerHop + Loss;
					Hold = Fall >= Rules.HoldAtSeat ? 0u : Rules.HoldAtSeat - static_cast<uint32>(Fall);
				}
				RegionAuthority Now;
				Now.Polity = S.Index;
				Now.Distance = Hops == Unreached ? 0u : Hops;
				Now.Hold = Hold;
				RegionAuthority* Was = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
				if (Was == nullptr)
				{
					W.Components().GetPool(Reaches.Authority).Add(RegionHandles[R], Now);
				}
				else
				{
					*Was = Now;
				}
				if (Hold >= Rules.HoldFloor || R == S.Region)
				{
					continue;
				}
				RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
				if (Rule != nullptr)
				{
					Rule->Polity = 0;
					Rule->Since = Context.Tick;
				}
				RuledBy[R] = 0;
				++Far->Slipped;
				// Whoever takes the ground away cancels the demand on it. The law
				// system runs before this one, so without this the region would be
				// assessed for a whole year on behalf of a master it no longer has.
				// What it already owes it still owes: the arrears wait for whoever
				// comes next.
				Economy::RegionDues* Owed = W.Components().GetPool(Laws.Dues).TryGet(RegionHandles[R]);
				if (Owed != nullptr)
				{
					Owed->PerMille = 0;
				}
				RegionAuthority* Gone = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
				if (Gone != nullptr)
				{
					Gone->Polity = 0;
					Gone->Hold = 0;
					Gone->Distance = 0;
				}
				Context.Events->Publish(Context.Tick, RegionSlippedEvent, PolityPayload{S.Index, R, 0, Hold},
										W.Entities().GetId(RegionHandles[R]));
			}

			// 4. What the treasury buys: how far a new region may be taken.
			Treasury* Purse = Hoard;
			const uint64 Held = Purse != nullptr ? Purse->Amount[G_GRAIN] : 0u;
			const uint64 Carries = Rules.ReachPerGrain == 0 ? 1u : 1u + Held / Rules.ReachPerGrain;
			Far->Reach = static_cast<uint32>(std::min<uint64>(Carries, std::max<uint32>(1u, Rules.ReachCeiling)));

			// 5. Take what it can walk to and pay for, in region order, so that
			//    two worlds of one seed take the same ground in the same year.
			if (Purse == nullptr || S.Region >= N || RuledBy[S.Region] != S.Index)
			{
				continue;
			}
			// The edge is every unruled neighbour of ground it holds, each at the
			// shortest walk from the seat, in region order.
			std::vector<uint32> Edge;
			for (uint32 R = 1; R < N; ++R)
			{
				if (RuledBy[R] != S.Index || Distance[R] == Unreached || R >= Graph.Neighbours.size())
				{
					continue;
				}
				// Empty ground is claimed from the seat, so the seat's reach
				// binds it. Ground put in play is taken at the border, where the
				// polity already stands, so the reach does not: a war is not
				// fought from the capital.
				const bool WithinReach = Distance[R] + 1u <= Far->Reach;
				for (const uint16 Next : Graph.Neighbours[R])
				{
					if (Next == 0 || Next >= N || RegionHandles[Next].IsNull())
					{
						continue;
					}
					if (RuledBy[Next] != 0)
					{
						// Ground somebody holds is takeable only where a higher
						// layer has put it in play against them (07.06).
						const RegionInPlay* Play =
							HasPlay ? W.Components().GetPool(InPlay_).TryGet(RegionHandles[Next]) : nullptr;
						if (Play == nullptr || Play->By != S.Index)
						{
							continue;
						}
					}
					else if (!WithinReach)
					{
						continue;
					}
					const uint32 Hops = Distance[R] + 1u;
					if (Distance[Next] == Unreached || Hops < Distance[Next])
					{
						Distance[Next] = Hops;
						Edge.push_back(Next);
					}
				}
			}
			std::sort(Edge.begin(), Edge.end());
			Edge.erase(std::unique(Edge.begin(), Edge.end()), Edge.end());
			for (const uint32 R : Edge)
			{
				const uint32 Held_ = RuledBy[R];
				const uint32 Price = Held_ != 0 ? Rules.AnnexCost : Rules.ClaimCost;
				if (Purse->Amount[G_GRAIN] < Price)
				{
					continue; // dearer ground waits for a fuller treasury
				}
				Purse->Amount[G_GRAIN] -= Price;
				Far->Spent += Price;
				++Far->Claimed;
				RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
				if (Rule == nullptr)
				{
					RegionRule Fresh;
					Fresh.Polity = S.Index;
					Fresh.Since = Context.Tick;
					W.Components().GetPool(Polities.Rule).Add(RegionHandles[R], Fresh);
				}
				else
				{
					Rule->Polity = S.Index;
					Rule->Since = Context.Tick;
				}
				RuledBy[R] = S.Index;
				// Held from this tick, and the region says so from its own side:
				// a region that is ruled always carries the authority of whoever
				// rules it, with no year in between.
				const uint64 Fall = uint64{Distance[R]} * Rules.HoldLostPerHop;
				RegionAuthority Taken;
				Taken.Polity = S.Index;
				Taken.Distance = Distance[R];
				Taken.Hold = Fall >= Rules.HoldAtSeat ? 0u : Rules.HoldAtSeat - static_cast<uint32>(Fall);
				RegionAuthority* Was = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
				if (Was == nullptr)
				{
					W.Components().GetPool(Reaches.Authority).Add(RegionHandles[R], Taken);
				}
				else
				{
					*Was = Taken;
				}
				Context.Events->Publish(Context.Tick, Held_ != 0 ? RegionAnnexedEvent : RegionTakenEvent,
										PolityPayload{S.Index, R, Held_, Distance[R]},
										W.Entities().GetId(RegionHandles[R]));
			}
		}

		// A region nobody rules carries nobody's authority.
		for (uint32 R = 1; R < N; ++R)
		{
			if (RuledBy[R] != 0 || RegionHandles[R].IsNull())
			{
				continue;
			}
			RegionAuthority* A = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
			if (A != nullptr && A->Polity != 0)
			{
				A->Polity = 0;
				A->Hold = 0;
				A->Distance = 0;
			}
		}
	}

	const RegionAuthority* AuthorityOf(const World& W, const History::PreHistoryTypes& Types, const ReachTypes& Reaches,
									   uint32 Region)
	{
		const RegionAuthority* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Reaches.Authority).TryGet(H);
					}
				});
		return Found;
	}

	const PolityReach* ReachOf(const World& W, const PolityTypes& Polities, const ReachTypes& Reaches, uint32 Polity)
	{
		const PolityReach* Found = nullptr;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle H, const PolityInfo& P)
				{
					if (P.Index == Polity && Found == nullptr)
					{
						Found = W.Components().GetPool(Reaches.Reach).TryGet(H);
					}
				});
		return Found;
	}

	ReachStats MeasureReach(const World& W, const History::PreHistoryTypes& Types, const PolityTypes& Polities,
							const ReachTypes& Reaches, const ReachRules& Rules)
	{
		ReachStats S;
		struct Row
		{
			uint32 Index = 0;
			PolityReach Reach;
			bool Standing = false;
		};
		std::vector<Row> Rows;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle H, const PolityInfo& P)
				{
					const PolityReach* R = W.Components().GetPool(Reaches.Reach).TryGet(H);
					if (R != nullptr)
					{
						Rows.push_back(Row{P.Index, *R, P.Dissolved == 0});
					}
				});
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Index < B.Index; });

		std::vector<uint32> Standing;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle, const PolityInfo& P)
				{
					if (P.Dissolved == 0)
					{
						Standing.push_back(P.Index);
					}
				});
		std::sort(Standing.begin(), Standing.end());

		struct Land
		{
			uint32 Region = 0;
			RegionAuthority Authority;
			uint32 Rule = 0;
			bool HasAuthority = false;
		};
		std::vector<Land> Lands;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionAuthority* A = W.Components().GetPool(Reaches.Authority).TryGet(H);
					const RegionRule* U = W.Components().GetPool(Polities.Rule).TryGet(H);
					Lands.push_back(Land{R.Index, A != nullptr ? *A : RegionAuthority{}, U != nullptr ? U->Polity : 0u,
										 A != nullptr});
				});
		std::sort(Lands.begin(), Lands.end(), [](const Land& A, const Land& B) { return A.Region < B.Region; });

		Hash64 D = HashString("Reach");
		for (const Row& R : Rows)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R.Reach), sizeof(R.Reach)));
			S.Spent += R.Reach.Spent;
			S.Unpaid += R.Reach.Unpaid;
		}
		for (const Land& L : Lands)
		{
			if (L.HasAuthority)
			{
				D = HashCombine(D, HashUInt64(L.Region));
				D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&L.Authority), sizeof(L.Authority)));
			}
			if (L.Authority.Polity == 0)
			{
				// A region that is ruled must carry the authority of whoever rules it.
				S.Bad += L.Rule != 0 && L.HasAuthority ? 1u : 0u;
				continue;
			}
			++S.Held;
			S.Firm += L.Authority.Hold > Rules.HoldAtSeat / 2u ? 1u : 0u;
			S.Far = std::max(S.Far, L.Authority.Distance);
			S.Bad += L.Authority.Hold > Rules.HoldAtSeat ? 1u : 0u;
			S.Bad += L.Authority.Polity != L.Rule ? 1u : 0u;
			S.Bad += std::binary_search(Standing.begin(), Standing.end(), L.Authority.Polity) ? 0u : 1u;
		}
		for (const Event& E : W.Log().All())
		{
			S.Taken += E.Is(RegionTakenEvent) ? 1u : 0u;
			S.Annexed += E.Is(RegionAnnexedEvent) ? 1u : 0u;
			S.Slipped += E.Is(RegionSlippedEvent) ? 1u : 0u;
			S.Unfunded += E.Is(UpkeepUnpaidEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Politics
