// VAELEN - VaelenMilitary
// Phase 08.05: war as a thing with a beginning and an end.
//
// STATUS: VALIDATED (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military

#include "Vaelen/Military/War.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Military
{
	namespace
	{
		constexpr uint64 WarSalt = 0x57415221ull; // "WAR!"
		constexpr uint32 WearCeiling = 1000;

		uint32 Wear(uint32 Was, uint32 More)
		{
			const uint64 Sum = uint64{Was} + More;
			return Sum > WearCeiling ? WearCeiling : static_cast<uint32>(Sum);
		}
	} // namespace

	WarTypes WarTypes::Declare(World& W)
	{
		WarTypes T;
		T.War = W.Types().Register<WarInfo>("WarInfo");
		W.Components().CreatePool(T.War);
		return T;
	}

	void WarSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;

		// Who still stands, so that a war can end because a side does not.
		std::vector<uint32> Standing;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle, const Politics::PolityInfo& P)
				{
					if (P.Dissolved == 0)
					{
						Standing.push_back(P.Index);
					}
				});
		std::sort(Standing.begin(), Standing.end());
		auto Stands = [&](uint32 P) { return std::binary_search(Standing.begin(), Standing.end(), P); };

		// The wars on record, by the pair they are between.
		struct Open
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 A = 0;
			uint32 B = 0;
		};
		std::vector<Open> Running;
		uint32 Highest = 0;
		W.Components().GetPool(Wars.War).ForEach(
			[&](EntityHandle H, const WarInfo& Fight)
			{
				Highest = std::max(Highest, Fight.Index);
				if (Fight.Ended == 0)
				{
					Running.push_back(Open{H, Fight.Index, Fight.A, Fight.B});
				}
			});
		std::sort(Running.begin(), Running.end(), [](const Open& X, const Open& Y) { return X.Index < Y.Index; });
		auto Fighting = [&](uint32 A, uint32 B) -> const Open*
		{
			for (const Open& O : Running)
			{
				if (O.A == A && O.B == B)
				{
					return &O;
				}
			}
			return nullptr;
		};

		// What the last year cost, side by side: the fallen of every battle
		// fought in it, and every capital that changed hands.
		const uint64 Since = Context.Tick > History::TicksPerYear ? Context.Tick - History::TicksPerYear : 0u;
		std::vector<std::pair<uint32, uint32>> Fell; // polity, men
		W.Components()
			.GetPool(Battles.Battle)
			.ForEach(
				[&](EntityHandle, const BattleInfo& Field)
				{
					if (Field.Fought <= Since)
					{
						return;
					}
					Fell.push_back({Field.Attacker, Field.AttackerLost});
					Fell.push_back({Field.Defender, Field.DefenderLost});
				});
		std::vector<uint32> LostSeat; // polities that lost a capital in the year
		for (const Event& E : W.Log().All())
		{
			if (E.Tick > Since && E.Is(SeatTakenEvent))
			{
				LostSeat.push_back(E.Get<Politics::PolityPayload>().Value);
			}
		}
		auto FallenOf = [&](uint32 Polity)
		{
			uint64 Men = 0;
			for (const auto& [Side, Lost] : Fell)
			{
				Men += Side == Polity ? Lost : 0u;
			}
			return Men;
		};
		auto SeatsLostBy = [&](uint32 Polity)
		{ return static_cast<uint32>(std::count(LostSeat.begin(), LostSeat.end(), Polity)); };

		// 1. Every relation that has turned to war opens one, and every war
		//    that is running holds its relation at war: a war is not called off
		//    by a good harvest.
		W.Components()
			.GetPool(Relations.Relation_)
			.ForEach(
				[&](EntityHandle, Politics::Relation& Bond)
				{
					const bool AtWar = Bond.Stance_ == static_cast<uint32>(Politics::Stance::War);
					const Open* Already = Fighting(Bond.A, Bond.B);
					if (Already != nullptr)
					{
						Bond.Stance_ = static_cast<uint32>(Politics::Stance::War);
						return;
					}
					if (!AtWar)
					{
						return;
					}
					// A relation outlives the polities in it, as a record (07.06), and
					// 07.06 only ever revisits pairs whose ground still touches. A war
					// stance left standing over a polity that is gone would be read as a
					// live war for ever after, and the survivor would keep a host under
					// arms and feed it against nobody. Write it back to peace here: the
					// war is the thing, and there is no war without two sides.
					if (!Stands(Bond.A) || !Stands(Bond.B))
					{
						Bond.Stance_ = static_cast<uint32>(Politics::Stance::Peace);
						Bond.Warmth = std::max(Bond.Warmth, Rules.PeaceWarmth);
						Bond.Turned = Context.Tick;
						return;
					}
					WarInfo Fresh;
					Fresh.Index = ++Highest;
					Fresh.A = Bond.A;
					Fresh.B = Bond.B;
					Fresh.Began = Context.Tick;
					Fresh.Identity = Noise::LatticeHash(W.Config().Seed ^ WarSalt, static_cast<int32>(Fresh.Index),
														static_cast<int32>(Bond.A));
					const EntityHandle H = W.CreateEntity(IdKind::War);
					W.Components().GetPool(Wars.War).Add(H, Fresh);
					Running.push_back(Open{H, Fresh.Index, Fresh.A, Fresh.B});
					Context.Events->Publish(Context.Tick, WarBeganEvent,
											Politics::PolityPayload{Fresh.A, 0u, Fresh.Index, Fresh.B},
											W.Entities().GetId(H));
				});
		std::sort(Running.begin(), Running.end(), [](const Open& X, const Open& Y) { return X.Index < Y.Index; });

		// 2. Wear both sides down, and end what has been borne long enough.
		for (const Open& O : Running)
		{
			WarInfo* Fight = W.Components().GetPool(Wars.War).TryGet(O.Handle);
			if (Fight == nullptr || Fight->Ended != 0)
			{
				continue;
			}
			++Fight->Years;
			const uint32 FellA = static_cast<uint32>(FallenOf(Fight->A));
			const uint32 FellB = static_cast<uint32>(FallenOf(Fight->B));
			const uint32 SeatsA = SeatsLostBy(Fight->A);
			const uint32 SeatsB = SeatsLostBy(Fight->B);
			Fight->FallenA += FellA;
			Fight->FallenB += FellB;
			Fight->Seats += SeatsA + SeatsB;
			Fight->WearA = Wear(Fight->WearA, Rules.WearPerYear + FellA * Rules.WearPerHundredFallen / 100u +
												  SeatsA * Rules.WearPerSeatLost);
			Fight->WearB = Wear(Fight->WearB, Rules.WearPerYear + FellB * Rules.WearPerHundredFallen / 100u +
												  SeatsB * Rules.WearPerSeatLost);

			// A war ends when a side is gone, or when what it has cost is more
			// than a side will go on paying.
			const bool AGone = !Stands(Fight->A);
			const bool BGone = !Stands(Fight->B);
			uint32 Winner = 0;
			bool Over = false;
			if (AGone || BGone)
			{
				Over = true;
				Winner = AGone && BGone ? 0u : (AGone ? Fight->B : Fight->A);
			}
			else if (Fight->Years >= Rules.LeastYears)
			{
				const bool AForfeits = Fight->WearA >= Rules.ForfeitAt;
				const bool BForfeits = Fight->WearB >= Rules.ForfeitAt;
				if (AForfeits != BForfeits)
				{
					Over = true;
					Winner = AForfeits ? Fight->B : Fight->A;
				}
				else if (AForfeits && BForfeits)
				{
					Over = true; // both broken: nobody won it
				}
				else if (Fight->WearA >= Rules.WillingAt && Fight->WearB >= Rules.WillingAt)
				{
					Over = true; // both willing to stop, and neither has won
				}
			}
			if (!Over)
			{
				continue;
			}
			Fight->Ended = Context.Tick;
			Fight->Winner = Winner;
			Context.Events->Publish(Context.Tick, WarEndedEvent,
									Politics::PolityPayload{Winner, 0u, Fight->Index, Fight->Years},
									W.Entities().GetId(O.Handle));

			// The terms are what has already happened: whoever holds ground at
			// the end keeps it. What the war put in play stops being in play,
			// and the stance is written back to a peace that will hold for a
			// while and then not.
			const uint32 SideA = Fight->A;
			const uint32 SideB = Fight->B;
			W.Components()
				.GetPool(Relations.Relation_)
				.ForEach(
					[&](EntityHandle, Politics::Relation& Bond)
					{
						if (Bond.A != SideA || Bond.B != SideB)
						{
							return;
						}
						Bond.Stance_ = static_cast<uint32>(Politics::Stance::Peace);
						Bond.Warmth = std::max(Bond.Warmth, Rules.PeaceWarmth);
						Bond.Turned = Context.Tick;
					});
			W.Components()
				.GetPool(Relations.Contested)
				.ForEach(
					[&](EntityHandle, Politics::RegionInPlay& Play)
					{
						if (Play.By == SideA || Play.By == SideB)
						{
							Play.By = 0;
						}
					});
			// And the marching ends with it. A host still under an order to walk on
			// ground that is no longer enemy ground is an order nobody gave: 08.01
			// sends it home on its next tick, and until then it is under nothing.
			W.Components()
				.GetPool(Armies.Army)
				.ForEach(
					[&](EntityHandle H, const ArmyInfo& Host)
					{
						if (Host.Disbanded != 0 || (Host.Polity != SideA && Host.Polity != SideB))
						{
							return;
						}
						MarchOrder* Under = W.Components().GetPool(Marches.Order).TryGet(H);
						if (Under != nullptr)
						{
							Under->Aim = 0;
							Under->Hops = 0;
							Under->Arrived = 0;
						}
					});
		}
	}

	const WarInfo* WarOf(const World& W, const WarTypes& Wars, uint32 War)
	{
		const WarInfo* Found = nullptr;
		W.Components().GetPool(Wars.War).ForEach(
			[&](EntityHandle H, const WarInfo& Fight)
			{
				if (Fight.Index == War && Found == nullptr)
				{
					Found = W.Components().GetPool(Wars.War).TryGet(H);
				}
			});
		return Found;
	}

	const WarInfo* WarBetween(const World& W, const WarTypes& Wars, uint32 A, uint32 B)
	{
		if (A == B || A == 0 || B == 0)
		{
			return nullptr;
		}
		if (A > B)
		{
			std::swap(A, B);
		}
		const WarInfo* Found = nullptr;
		W.Components().GetPool(Wars.War).ForEach(
			[&](EntityHandle H, const WarInfo& Fight)
			{
				if (Fight.A == A && Fight.B == B && Fight.Ended == 0 && Found == nullptr)
				{
					Found = W.Components().GetPool(Wars.War).TryGet(H);
				}
			});
		return Found;
	}

	WarStats MeasureWars(const World& W, const WarTypes& Wars, const WarRules& Rules)
	{
		WarStats S;
		(void)Rules;
		std::vector<WarInfo> All;
		W.Components().GetPool(Wars.War).ForEach([&](EntityHandle, const WarInfo& F) { All.push_back(F); });
		std::sort(All.begin(), All.end(), [](const WarInfo& X, const WarInfo& Y) { return X.Index < Y.Index; });

		Hash64 D = HashString("Wars");
		for (const WarInfo& F : All)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&F), sizeof(F)));
			S.Fallen += uint64{F.FallenA} + F.FallenB;
			S.LongestYears = std::max(S.LongestYears, F.Years);
			// A war is between two powers, and it began before it ended.
			S.Bad += F.A != 0 && F.B != 0 && F.A < F.B ? 0u : 1u;
			S.Bad += F.Began != 0 ? 0u : 1u;
			S.Bad += F.Identity != 0 ? 0u : 1u;
			if (F.Ended == 0)
			{
				++S.Running;
				// A war still being fought has no winner and no end.
				S.Bad += F.Winner != 0 ? 1u : 0u;
				continue;
			}
			++S.Over;
			S.Bad += F.Ended >= F.Began ? 0u : 1u;
			if (F.Winner == 0)
			{
				++S.White;
				continue;
			}
			++S.Decided;
			// Whoever won it fought in it.
			S.Bad += F.Winner == F.A || F.Winner == F.B ? 0u : 1u;
		}
		for (const Event& E : W.Log().All())
		{
			S.Began += E.Is(WarBeganEvent) ? 1u : 0u;
			S.Ended += E.Is(WarEndedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Military
