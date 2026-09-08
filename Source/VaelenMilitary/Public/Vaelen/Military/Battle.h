// VAELEN - VaelenMilitary
// Phase 08.03: battle - what happens when two hosts stand on the same ground.
//
// STATUS: VALIDATED (Phase 08) - unit/integration/deterministic/edge tests in Tests/Military
//
// 08.02 puts hosts in motion; sooner or later two of them, of polities at war,
// end a year in the same region. A battle is what settles that, and it settles
// it in one year: there is no manoeuvring, no second round, no reinforcement.
// Three things decide it and nothing else - how many men each side has, whose
// ground it is, and a draw from a stream fixed by the world seed. The ground
// counts because 07.03 already measures how firmly a polity holds a region,
// and a host fighting where the people obey its own ruler is not fighting the
// same battle as one deep in a stranger's province.
//
// What the loser loses is men, and the men leave the levies of the regions
// that gave them, so the count of men away stays exactly the count of men
// under arms (08.01). Whether they fell or scattered is 08.06's to say. A host
// left far weaker than the one that beat it breaks outright and is gone; one
// that merely lost falls back on the nearest ground its own polity rules, and
// loses its marching orders, because a beaten army is under no orders until it
// is given new ones.
//
// A battle does not take a region. Ground changes hands at a seat, before
// walls, in 08.04.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/March.h"
#include "Vaelen/Military/MilitaryApi.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Military
{
	/// Component of a battle entity (ids of kind Battle). A battle is written
	/// once and never changed: it is a thing that happened.
	struct BattleInfo
	{
		uint32 Index = 0;		 ///< 1-based, in the order fought
		uint32 Region = 0;		 ///< the ground
		uint32 Attacker = 0;	 ///< the polity that came to it
		uint32 Defender = 0;	 ///< the polity that held it, or was there first
		uint32 AttackerArmy = 0; ///< army indices
		uint32 DefenderArmy = 0;
		uint32 AttackerMen = 0; ///< men before the fighting
		uint32 DefenderMen = 0;
		uint32 AttackerLost = 0;
		uint32 DefenderLost = 0;
		uint32 Winner = 0; ///< Attacker or Defender, never neither
		uint32 Ground = 0; ///< per mille the defender's ground was worth
		uint64 Fought = 0; ///< tick
		Hash64 Identity = 0;
	};
	static_assert(sizeof(BattleInfo) == 64, "BattleInfo must stay padding free");

	struct BattleTypes
	{
		ComponentType<BattleInfo> Battle;
		static VAELEN_MILITARY_API BattleTypes Declare(World& W);
	};

	struct BattleRules
	{
		uint32 GroundPerMille = 300;	 ///< at a full hold, what fighting on your own ground is worth
		uint32 LuckPerMille = 150;		 ///< the widest a draw from the stream can swing a side
		uint32 LoserLostPerMille = 350;	 ///< of the beaten host's men
		uint32 WinnerLostPerMille = 120; ///< of the winner's
		uint32 BreakUnderPerMille = 350; ///< a loser left under this share of the winner is gone
	};

	/// Two hosts settled it (the winner, the region, the battle, men the loser lost).
	inline constexpr EventType<Politics::PolityPayload> BattleFoughtEvent =
		MakeEventType<Politics::PolityPayload>("BattleFought");
	/// A beaten host was left too weak to be a host at all (the polity, the region, the army, men lost in all).
	inline constexpr EventType<Politics::PolityPayload> ArmyBrokenEvent =
		MakeEventType<Politics::PolityPayload>("ArmyBroken");
	/// A beaten host fell back (the polity, the region it fell back to, the army, men left).
	inline constexpr EventType<Politics::PolityPayload> ArmyRetreatedEvent =
		MakeEventType<Politics::PolityPayload>("ArmyRetreated");

	/// Yearly, after Marching: in every region where two hosts of polities at
	/// war stand, settle it once - strength, ground and the stream - take the
	/// losses out of both sides' levies, and break or send back the loser.
	class VAELEN_MILITARY_API BattleSystem final : public ISystem
	{
	public:
		BattleSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Politics::PolityTypes InPolities,
					 Politics::ReachTypes InReaches, Politics::DiplomacyTypes InRelations, ArmyTypes InArmies,
					 MarchTypes InMarches, BattleTypes InBattles, BattleRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Polities(InPolities), Reaches(InReaches), Relations(InRelations),
			  Armies(InArmies), Marches(InMarches), Battles(InBattles), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Battles"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Marching"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Politics::PolityTypes Polities;
		Politics::ReachTypes Reaches;
		Politics::DiplomacyTypes Relations;
		ArmyTypes Armies;
		MarchTypes Marches;
		BattleTypes Battles;
		BattleRules Rules;
		WorldGen::RegionGraphCache Roads; ///< rebuilt when the map it was built from is replaced
	};

	/// A battle by index (nullptr when unknown).
	VAELEN_MILITARY_API const BattleInfo* BattleOf(const World& W, const BattleTypes& Battles, uint32 Battle);
	/// Every battle fought in a region, in index order.
	VAELEN_MILITARY_API void BattlesIn(const World& W, const BattleTypes& Battles, uint32 Region,
									   std::vector<uint32>& Out);

	struct BattleStats
	{
		uint32 Fought = 0;	 ///< battles on record
		uint32 Defended = 0; ///< of those, won by the side whose ground it was
		uint32 Broken = 0;	 ///< hosts left too weak to be hosts
		uint64 Fallen = 0;	 ///< men lost, both sides, over every battle
		uint32 Battles_ = 0; ///< events, from the log
		uint32 Breakings = 0;
		uint32 Retreats = 0;
		uint32 Bad = 0;	   ///< a battle of a polity with itself, on ground that is not there, with a winner
						   ///< that fought on neither side, or with a side losing more men than it brought
		Hash64 Digest = 0; ///< every battle in index order
	};
	VAELEN_MILITARY_API BattleStats MeasureBattles(const World& W, const History::PreHistoryTypes& Types,
												   const BattleTypes& Battles, const BattleRules& Rules);
} // namespace Vaelen::Military
