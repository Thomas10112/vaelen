// VAELEN - VaelenMilitary
// Phase 08.03: battle.
//
// STATUS: VALIDATED (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military

#include "Vaelen/Military/Battle.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Military
{
	namespace
	{
		constexpr uint64 BattleSalt = 0x42544c45ull; // "BTLE"
		/// A draw from the stream, as a swing in per mille within [-Width, +Width].
		int32 Swing(uint64 Seed, uint32 Region, uint32 Army, uint32 Width)
		{
			if (Width == 0)
			{
				return 0;
			}
			const uint64 H = Noise::LatticeHash(Seed, static_cast<int32>(Region), static_cast<int32>(Army));
			return static_cast<int32>(H % (uint64{Width} * 2u + 1u)) - static_cast<int32>(Width);
		}

		/// Strength swung by the stream, never below one man's worth.
		uint64 Rolled(uint32 Men, uint32 Bonus, int32 Luck)
		{
			const int64 Scale = int64{1000} + int64{Bonus} + Luck;
			const int64 Out = int64{Men} * (Scale > 1 ? Scale : 1) / 1000;
			return Out > 0 ? static_cast<uint64>(Out) : 0u;
		}
	} // namespace

	BattleTypes BattleTypes::Declare(World& W)
	{
		BattleTypes T;
		T.Battle = W.Types().Register<BattleInfo>("BattleInfo");
		W.Components().CreatePool(T.Battle);
		return T;
	}

	void BattleSystem::Tick(TickContext& Context)
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
		// The graph is derived from the map, so the cache is keyed on the map
		// and not on the count of regions: two different maps can share a count,
		// and a snapshot loaded over a world that has already ticked would
		// otherwise leave this walking an adjacency that no longer exists.
		const WorldGen::RegionGraph& Graph = Roads.Of(W.Map(), Types.World.Regions);

		std::vector<uint32> RuledBy(N, 0u);
		std::vector<uint32> HoldOf(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
			const Politics::RegionAuthority* Grip = W.Components().GetPool(Reaches.Authority).TryGet(RegionHandles[R]);
			HoldOf[R] = Grip != nullptr && Grip->Polity == RuledBy[R] ? Grip->Hold : 0u;
		}

		std::vector<std::pair<uint32, uint32>> Foes;
		WarPairs(W, Relations, Foes);
		if (Foes.empty())
		{
			return;
		}
		auto IsFoe = [&](uint32 Mine, uint32 Theirs)
		{ return Mine != 0 && Theirs != 0 && std::binary_search(Foes.begin(), Foes.end(), std::pair{Mine, Theirs}); };

		// The hosts standing, by the ground they stand on, in army index order.
		struct Host
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Polity = 0;
			uint32 Region = 0;
		};
		std::vector<Host> Standing;
		uint32 Highest = 0;
		W.Components()
			.GetPool(Armies.Army)
			.ForEach(
				[&](EntityHandle H, const ArmyInfo& A)
				{
					if (A.Disbanded == 0 && A.Strength != 0 && A.Region != 0 && A.Region < N)
					{
						Standing.push_back(Host{H, A.Index, A.Polity, A.Region});
					}
				});
		std::sort(Standing.begin(), Standing.end(), [](const Host& A, const Host& B) { return A.Index < B.Index; });
		if (Standing.size() < 2)
		{
			return;
		}
		W.Components()
			.GetPool(Battles.Battle)
			.ForEach([&](EntityHandle, const BattleInfo& B) { Highest = std::max(Highest, B.Index); });

		/// Where a beaten host falls back to: the neighbouring region its own
		/// polity rules, else any neighbour the winner does not, else nowhere.
		auto FallBackFrom = [&](uint32 From, uint32 Mine, uint32 Theirs) -> uint32
		{
			if (From >= Graph.Neighbours.size())
			{
				return 0;
			}
			uint32 Home = 0;
			uint32 Neutral = 0;
			for (const uint16 Next : Graph.Neighbours[From])
			{
				if (Next == 0 || Next >= N || RegionHandles[Next].IsNull())
				{
					continue;
				}
				if (RuledBy[Next] == Mine && (Home == 0 || Next < Home))
				{
					Home = Next;
				}
				else if (RuledBy[Next] != Theirs && (Neutral == 0 || Next < Neutral))
				{
					Neutral = Next;
				}
			}
			return Home != 0 ? Home : Neutral;
		};

		// One battle a region a year: the first pair of enemies standing on it,
		// in army index order.
		std::vector<uint8> Settled(N, 0u);
		for (usize i = 0; i + 1 < Standing.size(); ++i)
		{
			const Host& First = Standing[i];
			if (Settled[First.Region] != 0)
			{
				continue;
			}
			usize j = i + 1;
			for (; j < Standing.size(); ++j)
			{
				if (Standing[j].Region == First.Region && IsFoe(First.Polity, Standing[j].Polity))
				{
					break;
				}
			}
			if (j >= Standing.size())
			{
				continue;
			}
			const Host& Second = Standing[j];
			const uint32 Region = First.Region;
			Settled[Region] = 1u;

			// Whose ground it is decides who defends; on nobody's ground the
			// host that was raised first is the one standing its ground.
			const bool FirstHolds = RuledBy[Region] == First.Polity;
			const Host& Def = FirstHolds || RuledBy[Region] != Second.Polity ? First : Second;
			const Host& Att = &Def == &First ? Second : First;
			ArmyInfo* DefHost = W.Components().GetPool(Armies.Army).TryGet(Def.Handle);
			ArmyInfo* AttHost = W.Components().GetPool(Armies.Army).TryGet(Att.Handle);
			if (DefHost == nullptr || AttHost == nullptr)
			{
				continue;
			}
			const uint32 DefMen = DefHost->Strength;
			const uint32 AttMen = AttHost->Strength;
			const uint32 Ground = RuledBy[Region] == Def.Polity ? Rules.GroundPerMille * HoldOf[Region] / 1000u : 0u;

			const uint64 Stream = W.Config().Seed ^ BattleSalt ^ Context.Tick;
			const uint64 DefRoll = Rolled(DefMen, Ground, Swing(Stream, Region, Def.Index, Rules.LuckPerMille));
			const uint64 AttRoll = Rolled(AttMen, 0u, Swing(Stream, Region, Att.Index, Rules.LuckPerMille));
			// A tie goes to the side that did not have to come.
			const bool AttackerWon = AttRoll > DefRoll;
			ArmyInfo* Won = AttackerWon ? AttHost : DefHost;
			ArmyInfo* Lost = AttackerWon ? DefHost : AttHost;
			const Host& WonAt = AttackerWon ? Att : Def;
			const Host& LostAt = AttackerWon ? Def : Att;

			const uint32 LoserFell =
				std::min(Lost->Strength, std::max<uint32>(1u, Lost->Strength * Rules.LoserLostPerMille / 1000u));
			const uint32 WinnerFell =
				std::min(Won->Strength, std::max<uint32>(1u, Won->Strength * Rules.WinnerLostPerMille / 1000u));
			Lost->Strength -= LoserFell;
			Won->Strength -= WinnerFell;

			BattleInfo Record;
			Record.Index = ++Highest;
			Record.Region = Region;
			Record.Attacker = Att.Polity;
			Record.Defender = Def.Polity;
			Record.AttackerArmy = Att.Index;
			Record.DefenderArmy = Def.Index;
			Record.AttackerMen = AttMen;
			Record.DefenderMen = DefMen;
			Record.AttackerLost = AttackerWon ? WinnerFell : LoserFell;
			Record.DefenderLost = AttackerWon ? LoserFell : WinnerFell;
			Record.Winner = WonAt.Polity;
			Record.Ground = Ground;
			Record.Fought = Context.Tick;
			Record.Identity = Noise::LatticeHash(W.Config().Seed ^ BattleSalt, static_cast<int32>(Record.Index),
												 static_cast<int32>(Region));
			const EntityHandle Written = W.CreateEntity(IdKind::Battle);
			W.Components().GetPool(Battles.Battle).Add(Written, Record);
			const PersistentId Fought = Context.Events->Publish(
				Context.Tick, BattleFoughtEvent,
				Politics::PolityPayload{Record.Winner, Region, Record.Index, LoserFell}, W.Entities().GetId(Written));

			// The fallen leave the levies of the regions that gave them, all of them,
			// and the record of each says which battle it was.
			VAELEN_ENSURE(ReleaseLevy(W, Types, Armies, Lost->Polity, LoserFell, LevyEnd::Fallen, Context, Fought) ==
						  LoserFell);
			VAELEN_ENSURE(ReleaseLevy(W, Types, Armies, Won->Polity, WinnerFell, LevyEnd::Fallen, Context, Fought) ==
						  WinnerFell);

			// A beaten host is under no orders until it is given new ones.
			MarchOrder* Beaten = W.Components().GetPool(Marches.Order).TryGet(LostAt.Handle);
			if (Beaten != nullptr)
			{
				Beaten->Aim = 0;
				Beaten->Hops = 0;
				Beaten->Arrived = 0;
			}
			// And so is the winner, when what it marched on was the host it has just
			// beaten rather than the ground it is standing on. The beaten host is
			// about to leave this region or stop existing; an order to march on it
			// where it no longer is is an order to march on nothing.
			MarchOrder* Standing_ = W.Components().GetPool(Marches.Order).TryGet(WonAt.Handle);
			if (Standing_ != nullptr && Standing_->Aim == Region && !IsFoe(WonAt.Polity, RuledBy[Region]))
			{
				Standing_->Aim = 0;
				Standing_->Hops = 0;
				Standing_->Arrived = 0;
			}

			const bool Shattered = uint64{Lost->Strength} * 1000u < uint64{Won->Strength} * Rules.BreakUnderPerMille;
			if (Shattered)
			{
				const uint32 Rest = Lost->Strength;
				// What is left of a broken host scatters and walks home.
				VAELEN_ENSURE(ReleaseLevy(W, Types, Armies, Lost->Polity, Rest, LevyEnd::Home, Context) == Rest);
				Lost->Strength = 0;
				Lost->Disbanded = Context.Tick;
				Context.Events->Publish(Context.Tick, ArmyBrokenEvent,
										Politics::PolityPayload{LostAt.Polity, Region, LostAt.Index, LoserFell + Rest},
										W.Entities().GetId(LostAt.Handle), Fought);
				continue;
			}
			const uint32 Back = FallBackFrom(Region, LostAt.Polity, WonAt.Polity);
			if (Back != 0)
			{
				Lost->Region = Back;
			}
			Context.Events->Publish(
				Context.Tick, ArmyRetreatedEvent,
				Politics::PolityPayload{LostAt.Polity, Back != 0 ? Back : Region, LostAt.Index, Lost->Strength},
				W.Entities().GetId(LostAt.Handle), Fought);
		}
	}

	const BattleInfo* BattleOf(const World& W, const BattleTypes& Battles, uint32 Battle)
	{
		const BattleInfo* Found = nullptr;
		W.Components()
			.GetPool(Battles.Battle)
			.ForEach(
				[&](EntityHandle H, const BattleInfo& B)
				{
					if (B.Index == Battle && Found == nullptr)
					{
						Found = W.Components().GetPool(Battles.Battle).TryGet(H);
					}
				});
		return Found;
	}

	void BattlesIn(const World& W, const BattleTypes& Battles, uint32 Region, std::vector<uint32>& Out)
	{
		Out.clear();
		if (Region == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Battles.Battle)
			.ForEach(
				[&](EntityHandle, const BattleInfo& B)
				{
					if (B.Region == Region)
					{
						Out.push_back(B.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	BattleStats MeasureBattles(const World& W, const History::PreHistoryTypes& Types, const BattleTypes& Battles,
							   const BattleRules& Rules)
	{
		BattleStats S;
		(void)Rules;
		usize Regions = 0;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach([&](EntityHandle, const WorldGen::RegionInfo& R) { Regions = std::max(Regions, usize{R.Index}); });

		std::vector<BattleInfo> All;
		W.Components().GetPool(Battles.Battle).ForEach([&](EntityHandle, const BattleInfo& B) { All.push_back(B); });
		std::sort(All.begin(), All.end(), [](const BattleInfo& A, const BattleInfo& B) { return A.Index < B.Index; });

		Hash64 D = HashString("Battles");
		for (const BattleInfo& B : All)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&B), sizeof(B)));
			++S.Fought;
			S.Fallen += uint64{B.AttackerLost} + B.DefenderLost;
			S.Defended += B.Winner == B.Defender ? 1u : 0u;
			// A battle has two sides, on ground that is there, and it is won by
			// one of the two; and no side loses more men than it brought.
			S.Bad += B.Attacker != 0 && B.Defender != 0 && B.Attacker != B.Defender ? 0u : 1u;
			S.Bad += B.Region != 0 && B.Region <= Regions ? 0u : 1u;
			S.Bad += B.Winner == B.Attacker || B.Winner == B.Defender ? 0u : 1u;
			S.Bad += B.AttackerLost <= B.AttackerMen && B.DefenderLost <= B.DefenderMen ? 0u : 1u;
			S.Bad += B.AttackerArmy != 0 && B.DefenderArmy != 0 && B.AttackerArmy != B.DefenderArmy ? 0u : 1u;
			S.Bad += B.Identity != 0 && B.Fought != 0 ? 0u : 1u;
		}
		for (const Event& E : W.Log().All())
		{
			S.Battles_ += E.Is(BattleFoughtEvent) ? 1u : 0u;
			S.Breakings += E.Is(ArmyBrokenEvent) ? 1u : 0u;
			S.Retreats += E.Is(ArmyRetreatedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Military
