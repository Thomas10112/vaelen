// VAELEN - VaelenMilitary
// Phase 08.02: marching.
//
// STATUS: PROTOTYPE (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military

#include "Vaelen/Military/March.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Military
{
	namespace
	{
		constexpr uint32 G_GRAIN = static_cast<uint32>(Economy::Good::Grain);
		constexpr uint32 Unreached = 0xffffffffu;

		/// Every ordered pair of polities at war, sorted, for a binary search.
		void GatherFoes(const World& W, const Politics::DiplomacyTypes& Relations,
						std::vector<std::pair<uint32, uint32>>& Out)
		{
			Out.clear();
			W.Components()
				.GetPool(Relations.Relation_)
				.ForEach(
					[&](EntityHandle, const Politics::Relation& Bond)
					{
						if (Bond.Stance_ != static_cast<uint32>(Politics::Stance::War))
						{
							return;
						}
						Out.push_back({Bond.A, Bond.B});
						Out.push_back({Bond.B, Bond.A});
					});
			std::sort(Out.begin(), Out.end());
		}
	} // namespace

	MarchTypes MarchTypes::Declare(World& W)
	{
		MarchTypes T;
		T.Order = W.Types().Register<MarchOrder>("MarchOrder");
		T.Forage = W.Types().Register<RegionForage>("RegionForage");
		W.Components().CreatePool(T.Order);
		W.Components().CreatePool(T.Forage);
		return T;
	}

	void MarchSystem::Tick(TickContext& Context)
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
		// rebuilt only when the count of regions does.
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
			const Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
		}

		std::vector<std::pair<uint32, uint32>> Foes;
		GatherFoes(W, Relations, Foes);
		auto IsFoe = [&](uint32 Mine, uint32 Theirs)
		{ return Mine != 0 && Theirs != 0 && std::binary_search(Foes.begin(), Foes.end(), std::pair{Mine, Theirs}); };

		struct Standing
		{
			EntityHandle Handle;
			uint32 Index = 0;
		};
		std::vector<Standing> Order;
		W.Components()
			.GetPool(Armies.Army)
			.ForEach([&](EntityHandle H, const ArmyInfo& A) { Order.push_back(Standing{H, A.Index}); });
		std::sort(Order.begin(), Order.end(), [](const Standing& A, const Standing& B) { return A.Index < B.Index; });

		std::vector<uint32> Distance(N, Unreached);
		std::vector<uint32> Queue;
		std::vector<uint8> Stood(N, 0u);

		for (const Standing& It : Order)
		{
			ArmyInfo* A = W.Components().GetPool(Armies.Army).TryGet(It.Handle);
			if (A == nullptr)
			{
				continue;
			}
			MarchOrder* Sealed = W.Components().GetPool(Marches.Order).TryGet(It.Handle);
			if (Sealed == nullptr)
			{
				W.Components().GetPool(Marches.Order).Add(It.Handle, MarchOrder{});
				Sealed = W.Components().GetPool(Marches.Order).TryGet(It.Handle);
			}
			if (Sealed == nullptr)
			{
				continue;
			}
			// A host that went home is under no order; what it walked stays on
			// the record, because it is the only place that walking is written.
			if (A->Disbanded != 0 || A->Strength == 0)
			{
				Sealed->Aim = 0;
				Sealed->Hops = 0;
				Sealed->Arrived = 0;
				continue;
			}
			const uint32 From = A->Region;
			if (From == 0 || From >= N || RegionHandles[From].IsNull())
			{
				Sealed->Aim = 0;
				Sealed->Hops = 0;
				Sealed->Arrived = 0;
				continue;
			}

			// Hops from where it stands to everywhere it can walk.
			std::fill(Distance.begin(), Distance.end(), Unreached);
			Queue.clear();
			Distance[From] = 0;
			Queue.push_back(From);
			for (usize Head = 0; Head < Queue.size(); ++Head)
			{
				const uint32 At = Queue[Head];
				if (Distance[At] >= Rules.AimWithin || At >= Graph.Neighbours.size())
				{
					continue;
				}
				for (const uint16 Next : Graph.Neighbours[At])
				{
					if (Next == 0 || Next >= N || RegionHandles[Next].IsNull() || Distance[Next] != Unreached)
					{
						continue;
					}
					Distance[Next] = Distance[At] + 1u;
					Queue.push_back(Next);
				}
			}

			// The aim is the nearest enemy ground; ties go to the lower region.
			uint32 Aim = 0;
			uint32 Best = Unreached;
			for (uint32 R = 1; R < N; ++R)
			{
				if (Distance[R] == Unreached || Distance[R] >= Best || !IsFoe(A->Polity, RuledBy[R]))
				{
					continue;
				}
				Best = Distance[R];
				Aim = R;
			}

			uint32 Walked = 0;
			uint32 To = From;
			if (Aim != 0 && Best != 0)
			{
				// Walk the shortest way back from the aim, then take the first
				// HopsPerYear steps of it forwards. Ties go to the lower region,
				// so the road an army takes is the same on every run.
				std::vector<uint32> Back;
				Back.push_back(Aim);
				uint32 At = Aim;
				while (At != From)
				{
					uint32 Step = 0;
					if (At < Graph.Neighbours.size())
					{
						for (const uint16 Prev : Graph.Neighbours[At])
						{
							if (Prev < N && Distance[Prev] + 1u == Distance[At] && (Step == 0 || Prev < Step))
							{
								Step = Prev;
							}
						}
					}
					if (Step == 0)
					{
						Back.clear(); // no way back: the graph moved under us
						break;
					}
					At = Step;
					Back.push_back(At);
				}
				if (!Back.empty())
				{
					const usize Steps = std::min<usize>(Rules.HopsPerYear, Back.size() - 1u);
					To = Back[Back.size() - 1u - Steps];
					Walked = static_cast<uint32>(Steps);
				}
			}

			const bool WasThere = Sealed->Arrived != 0 && Sealed->Aim == Aim;
			A->Region = To;
			Sealed->Aim = Aim;
			Sealed->Hops = Aim != 0 && Best != Unreached ? Best - Walked : 0u;
			Sealed->Walked += Walked;
			Sealed->Arrived = Aim != 0 && To == Aim ? 1u : 0u;
			if (Walked != 0)
			{
				Context.Events->Publish(Context.Tick, ArmyMarchedEvent,
										Politics::PolityPayload{A->Polity, To, A->Index, Walked},
										W.Entities().GetId(It.Handle));
			}
			if (Sealed->Arrived != 0 && !WasThere)
			{
				Context.Events->Publish(Context.Tick, ArmyArrivedEvent,
										Politics::PolityPayload{A->Polity, To, A->Index, Sealed->Walked},
										W.Entities().GetId(It.Handle));
			}

			// It eats off the ground it ends the year on.
			const EntityHandle Ground = RegionHandles[To];
			uint32 Took = 0;
			Economy::RegionStock* Stock = W.Components().GetPool(Economy.Region).TryGet(Ground);
			if (Stock != nullptr)
			{
				const uint64 Want = uint64{A->Strength} * Rules.ForagePerManPerYear;
				Took = static_cast<uint32>(std::min<uint64>(Want, Stock->Amount[G_GRAIN]));
				Stock->Amount[G_GRAIN] -= Took;
			}
			RegionForage* Eaten = W.Components().GetPool(Marches.Forage).TryGet(Ground);
			if (Eaten == nullptr)
			{
				W.Components().GetPool(Marches.Forage).Add(Ground, RegionForage{Took, 1u});
			}
			else if (Stood[To] != 0)
			{
				Eaten->Taken += Took; // a second host on the same ground the same year
			}
			else
			{
				Eaten->Taken = Took;
				++Eaten->Years;
			}
			Stood[To] = 1u;
			if (Took != 0)
			{
				Context.Events->Publish(Context.Tick, ArmyForagedEvent,
										Politics::PolityPayload{A->Polity, To, A->Index, Took},
										W.Entities().GetId(It.Handle));
			}

			// A host on somebody else's ground loosens their grip on it. It does
			// not take the region - 08.04 does that - but 07.03 knows what a
			// grip that keeps loosening comes to.
			if (RuledBy[To] != 0 && RuledBy[To] != A->Polity && IsFoe(A->Polity, RuledBy[To]))
			{
				Politics::RegionAuthority* Grip = W.Components().GetPool(Reaches.Authority).TryGet(Ground);
				if (Grip != nullptr)
				{
					Grip->Hold = Grip->Hold > Rules.HostileHoldPerMille ? Grip->Hold - Rules.HostileHoldPerMille : 0u;
				}
			}
		}

		// Ground nobody stood on this year is not being eaten.
		for (uint32 R = 1; R < N; ++R)
		{
			if (Stood[R] != 0 || RegionHandles[R].IsNull())
			{
				continue;
			}
			RegionForage* Eaten = W.Components().GetPool(Marches.Forage).TryGet(RegionHandles[R]);
			if (Eaten != nullptr)
			{
				Eaten->Taken = 0;
				Eaten->Years = 0;
			}
		}
	}

	const MarchOrder* OrderOf(const World& W, const ArmyTypes& Armies, const MarchTypes& Marches, uint32 Army)
	{
		const MarchOrder* Found = nullptr;
		W.Components()
			.GetPool(Armies.Army)
			.ForEach(
				[&](EntityHandle H, const ArmyInfo& A)
				{
					if (A.Index == Army && Found == nullptr)
					{
						Found = W.Components().GetPool(Marches.Order).TryGet(H);
					}
				});
		return Found;
	}

	const RegionForage* ForageOf(const World& W, const History::PreHistoryTypes& Types, const MarchTypes& Marches,
								 uint32 Region)
	{
		const RegionForage* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Marches.Forage).TryGet(H);
					}
				});
		return Found;
	}

	MarchStats MeasureMarches(const World& W, const History::PreHistoryTypes& Types,
							  const Politics::PolityTypes& Polities, const Politics::DiplomacyTypes& Relations,
							  const ArmyTypes& Armies, const MarchTypes& Marches, const MarchRules& Rules)
	{
		MarchStats S;
		(void)Rules;
		std::vector<uint32> RuledBy;
		std::vector<std::pair<uint32, RegionForage>> Eaten;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(H);
					if (R.Index >= RuledBy.size())
					{
						RuledBy.resize(usize{R.Index} + 1u, 0u);
					}
					RuledBy[R.Index] = Rule != nullptr ? Rule->Polity : 0u;
					const RegionForage* F = W.Components().GetPool(Marches.Forage).TryGet(H);
					if (F != nullptr)
					{
						Eaten.push_back({R.Index, *F});
					}
				});
		std::sort(Eaten.begin(), Eaten.end(), [](const auto& A, const auto& B) { return A.first < B.first; });

		std::vector<std::pair<uint32, uint32>> Foes;
		GatherFoes(W, Relations, Foes);
		auto IsFoe = [&](uint32 Mine, uint32 Theirs)
		{ return Mine != 0 && Theirs != 0 && std::binary_search(Foes.begin(), Foes.end(), std::pair{Mine, Theirs}); };

		struct Pair
		{
			ArmyInfo Host;
			MarchOrder Under;
			bool Sealed = false;
		};
		std::vector<Pair> All;
		W.Components()
			.GetPool(Armies.Army)
			.ForEach(
				[&](EntityHandle H, const ArmyInfo& A)
				{
					const MarchOrder* O = W.Components().GetPool(Marches.Order).TryGet(H);
					All.push_back(Pair{A, O != nullptr ? *O : MarchOrder{}, O != nullptr});
				});
		std::sort(All.begin(), All.end(), [](const Pair& A, const Pair& B) { return A.Host.Index < B.Host.Index; });

		Hash64 D = HashString("Marches");
		for (const Pair& P : All)
		{
			if (!P.Sealed)
			{
				continue;
			}
			D = HashCombine(D, HashUInt64(P.Host.Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&P.Under), sizeof(P.Under)));
			S.Walked += P.Under.Walked;
			if (P.Host.Disbanded != 0)
			{
				// A host that went home is under no order.
				S.Bad += P.Under.Aim != 0 || P.Under.Arrived != 0 ? 1u : 0u;
				continue;
			}
			const bool RealGround = P.Host.Region != 0 && P.Host.Region < RuledBy.size();
			S.Bad += RealGround ? 0u : 1u;
			if (RealGround && RuledBy[P.Host.Region] != P.Host.Polity)
			{
				++S.Abroad;
			}
			if (P.Under.Aim == 0)
			{
				++S.Idle;
				S.Bad += P.Under.Hops != 0 || P.Under.Arrived != 0 ? 1u : 0u;
				continue;
			}
			++S.Marching;
			// The aim is ground held by somebody this polity is at war with.
			S.Bad += P.Under.Aim < RuledBy.size() && IsFoe(P.Host.Polity, RuledBy[P.Under.Aim]) ? 0u : 1u;
			if (P.Under.Arrived != 0)
			{
				++S.Arrived;
				// Arrived means standing on it, with nothing left to walk.
				S.Bad += P.Host.Region == P.Under.Aim && P.Under.Hops == 0 ? 0u : 1u;
			}
			else
			{
				// Not arrived means there is still ground between the two.
				S.Bad += P.Under.Hops != 0 && P.Host.Region != P.Under.Aim ? 0u : 1u;
			}
		}
		for (const auto& [Region, Forage] : Eaten)
		{
			D = HashCombine(D, HashUInt64(Region));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Forage), sizeof(Forage)));
			if (Forage.Years == 0)
			{
				// Nobody stood on it this year, so nothing was taken off it.
				S.Bad += Forage.Taken != 0 ? 1u : 0u;
				continue;
			}
			++S.Foraged;
			S.Taken += Forage.Taken;
		}
		for (const Event& E : W.Log().All())
		{
			S.Marches += E.Is(ArmyMarchedEvent) ? 1u : 0u;
			S.Forages += E.Is(ArmyForagedEvent) ? 1u : 0u;
			S.Arrivals += E.Is(ArmyArrivedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Military
