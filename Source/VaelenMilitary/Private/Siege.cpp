// VAELEN - VaelenMilitary
// Phase 08.04: siege.
//
// STATUS: VALIDATED (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military

#include "Vaelen/Military/Siege.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Military
{
	SiegeTypes SiegeTypes::Declare(World& W)
	{
		SiegeTypes T;
		T.Siege = W.Types().Register<SiegeInfo>("SiegeInfo");
		W.Components().CreatePool(T.Siege);
		return T;
	}

	void SiegeSystem::Tick(TickContext& Context)
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

		// Which region is whose seat, and who rules what.
		std::vector<uint32> SeatOf(N, 0u); // region -> the polity seated there
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle, const Politics::PolityInfo& P)
				{
					if (P.Dissolved == 0 && P.Seat != 0 && P.Seat < N &&
						(SeatOf[P.Seat] == 0 || P.Index < SeatOf[P.Seat]))
					{
						SeatOf[P.Seat] = P.Index;
					}
				});

		std::vector<std::pair<uint32, uint32>> Foes;
		WarPairs(W, Relations, Foes);
		auto IsFoe = [&](uint32 Mine, uint32 Theirs)
		{ return Mine != 0 && Theirs != 0 && std::binary_search(Foes.begin(), Foes.end(), std::pair{Mine, Theirs}); };

		// Hosts standing, by the ground they stand on, in army index order.
		struct Host
		{
			uint32 Index = 0;
			uint32 Polity = 0;
			uint32 Region = 0;
			uint32 Strength = 0;
		};
		std::vector<Host> Standing;
		W.Components()
			.GetPool(Armies.Army)
			.ForEach(
				[&](EntityHandle, const ArmyInfo& A)
				{
					if (A.Disbanded == 0 && A.Strength != 0 && A.Region != 0 && A.Region < N)
					{
						Standing.push_back(Host{A.Index, A.Polity, A.Region, A.Strength});
					}
				});
		std::sort(Standing.begin(), Standing.end(), [](const Host& A, const Host& B) { return A.Index < B.Index; });

		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			SiegeInfo* Walls = W.Components().GetPool(Sieges.Siege).TryGet(RegionHandles[R]);
			const uint32 Seated = SeatOf[R];

			// The host before it: the first, in army index order, of a polity at
			// war with the one seated here and strong enough to shut it in.
			const Host* Besieging = nullptr;
			if (Seated != 0)
			{
				for (const Host& H : Standing)
				{
					if (H.Region == R && H.Polity != Seated && IsFoe(H.Polity, Seated) &&
						H.Strength >= Rules.MenToInvest)
					{
						Besieging = &H;
						break;
					}
				}
			}

			if (Besieging == nullptr)
			{
				// Nobody is sitting before it. A siege that was being pressed is
				// lifted, and the wall is mended a little.
				if (Walls == nullptr)
				{
					continue;
				}
				if (Walls->Besieger != 0)
				{
					Context.Events->Publish(Context.Tick, SiegeLiftedEvent,
											Politics::PolityPayload{Walls->Besieger, R, Walls->Army, Walls->Wall},
											W.Entities().GetId(RegionHandles[R]));
					Walls->Besieger = 0;
					Walls->Army = 0;
					Walls->Years = 0;
					Walls->Laid = 0;
				}
				Walls->Wall = std::min(Rules.WallAtFull, Walls->Wall + Rules.MendPerYear);
				continue;
			}

			if (Walls == nullptr)
			{
				W.Components()
					.GetPool(Sieges.Siege)
					.Add(RegionHandles[R], SiegeInfo{0u, 0u, Seated, 0u, Rules.WallAtFull, 0u, 0u});
				Walls = W.Components().GetPool(Sieges.Siege).TryGet(RegionHandles[R]);
				if (Walls == nullptr)
				{
					continue;
				}
			}
			// A fresh besieger begins its own count, but inherits the work: a
			// wall that has been breached does not stand itself up again for
			// the next army that comes.
			if (Walls->Besieger != Besieging->Polity)
			{
				Walls->Besieger = Besieging->Polity;
				Walls->Army = Besieging->Index;
				Walls->Years = 0;
				Walls->Laid = Context.Tick;
				Context.Events->Publish(Context.Tick, SiegeLaidEvent,
										Politics::PolityPayload{Besieging->Polity, R, Besieging->Index, Walls->Wall},
										W.Entities().GetId(RegionHandles[R]));
			}
			Walls->Army = Besieging->Index;
			Walls->Held = Seated;
			++Walls->Years;
			if (Walls->Years <= Rules.YearsBeforeBreaching)
			{
				continue; // the year it arrives, nothing comes down
			}
			const uint32 Down = Besieging->Strength * Rules.WallPerHundredMen / 100u;
			Walls->Wall = Walls->Wall > Down ? Walls->Wall - Down : 0u;
			if (Walls->Wall != 0)
			{
				continue;
			}

			// The seat is stormed. Belonging is written on the region itself
			// (07.01), and the polity that held it learns next year, from the
			// rule that a polity without its seat is not a polity.
			Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			if (Rule != nullptr)
			{
				Rule->Polity = Besieging->Polity;
				Rule->Since = Context.Tick;
			}
			Politics::RegionAuthority* Grip = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
			if (Grip != nullptr)
			{
				Grip->Polity = Besieging->Polity;
				Grip->Distance = 0;
				Grip->Hold = 0; // a stormed seat obeys nobody yet; 07.03 earns it back
			}
			Context.Events->Publish(Context.Tick, SeatTakenEvent,
									Politics::PolityPayload{Besieging->Polity, R, Besieging->Index, Seated},
									W.Entities().GetId(RegionHandles[R]));
			// A seat that has changed hands is nobody's aim any more: 08.02 chose it
			// when an enemy ruled it, and an enemy no longer does. Cleared here
			// rather than tolerated, because an aim that is not enemy ground is not
			// an aim.
			W.Components()
				.GetPool(Armies.Army)
				.ForEach(
					[&](EntityHandle Host, const ArmyInfo& A)
					{
						if (A.Disbanded != 0)
						{
							return;
						}
						MarchOrder* Under = W.Components().GetPool(Marches.Order).TryGet(Host);
						if (Under != nullptr && Under->Aim == R)
						{
							Under->Aim = 0;
							Under->Hops = 0;
							Under->Arrived = 0;
						}
					});
			++Walls->Taken;
			Walls->Besieger = 0;
			Walls->Army = 0;
			Walls->Years = 0;
			Walls->Laid = 0;
			Walls->Held = Besieging->Polity;
			Walls->Wall = Rules.WallAtFull > Rules.MendPerYear ? Rules.MendPerYear : Rules.WallAtFull;
		}
	}

	const SiegeInfo* SiegeOf(const World& W, const History::PreHistoryTypes& Types, const SiegeTypes& Sieges,
							 uint32 Region)
	{
		const SiegeInfo* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Sieges.Siege).TryGet(H);
					}
				});
		return Found;
	}

	SiegeStats MeasureSieges(const World& W, const History::PreHistoryTypes& Types, const SiegeTypes& Sieges,
							 const SiegeRules& Rules)
	{
		SiegeStats S;
		std::vector<std::pair<uint32, SiegeInfo>> All;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const SiegeInfo* Walls = W.Components().GetPool(Sieges.Siege).TryGet(H);
					if (Walls != nullptr)
					{
						All.push_back({R.Index, *Walls});
					}
				});
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });

		Hash64 D = HashString("Sieges");
		for (const auto& [Region, Walls] : All)
		{
			D = HashCombine(D, HashUInt64(Region));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Walls), sizeof(Walls)));
			S.Stormed += Walls.Taken;
			S.Breached += Walls.Wall < Rules.WallAtFull ? 1u : 0u;
			// A wall never stands taller than a whole one.
			S.Bad += Walls.Wall <= Rules.WallAtFull ? 0u : 1u;
			if (Walls.Besieger == 0)
			{
				// Nobody is sitting before it, so nothing is being pressed.
				S.Bad += Walls.Army != 0 || Walls.Years != 0 || Walls.Laid != 0 ? 1u : 0u;
				continue;
			}
			++S.Pressed;
			// A siege is laid by somebody else, with a host, at a time.
			S.Bad += Walls.Besieger != Walls.Held && Walls.Army != 0 && Walls.Laid != 0 && Walls.Years != 0 ? 0u : 1u;
		}
		for (const Event& E : W.Log().All())
		{
			S.Laid += E.Is(SiegeLaidEvent) ? 1u : 0u;
			S.Lifted += E.Is(SiegeLiftedEvent) ? 1u : 0u;
			S.Taken += E.Is(SeatTakenEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Military
