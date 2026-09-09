// VAELEN - VaelenPlayer
// Phase 10.05: what the player can do - seven verbs, none of which writes
// anything itself.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player
//
// 10.04 gave intent a shape and a system inside the simulation to act on it,
// and left the acting empty on purpose: an intent cost its hours and changed
// nothing. This fills it in, under the rule the phase was written to - a doing
// changes the world ONLY through the system that already owns that change.
//
//   work   -> Economy::AddStock (06.01), which is what owns units of a good,
//             and Population::HungerPerson/TirePerson (04.04) for what the day
//             costs the body - so that eating and resting are worth doing
//   eat    -> AddStock for the grain, Population::FeedPerson (04.04) for the meal
//   rest   -> Population::RestPerson (04.04), the field 04.04 reserved for this
//   move   -> Population::MovePerson (04.06), which owns a person's region and
//             reconciles both grains; only to a region a person could walk to
//   speak  -> nothing but the act itself; 10.06 reads the acts to build what the
//             people around the player make of them, and nothing else should
//   give   -> AddStock twice, out of one house and into another
//   take   -> AddStock twice, the other way; what it costs in standing is 10.06's
//
// Nothing here reaches into a component another module owns. That is not
// fastidiousness: it is the only reason a life can be replayed. A verb that
// wrote a region's stock directly would have to be replayed by re-running the
// verb rather than the simulation, and the two would drift the first time the
// economy changed.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/PlayerApi.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"

namespace Vaelen
{
	class World;
}

namespace Vaelen::Player
{
	struct DoingRules
	{
		uint32 WorkGood = static_cast<uint32>(Economy::Good::Grain); ///< what a day's work brings in
		uint32 WorkYield = 2;										 ///< units of it
		uint32 WorkHunger = 12;										 ///< food a day of work burns (of 255)
		uint32 WorkTire = 15;										 ///< rest a day of work spends (of 255)
		uint32 EatGrain = 1;										 ///< grain a meal costs
		uint32 EatFood = 60;										 ///< food a meal restores (of 255)
		uint32 RestGain = 40;										 ///< rest a rest restores (of 255)
		uint32 GiveMost = 20;										 ///< units one giving can move
		uint32 TakeMost = 5;										 ///< units one taking can move
	};

	/// The seven verbs, each one a call into the module that owns that change.
	/// Held by the caller and handed to PlayerOrderSystem with ObserveDoing; a
	/// world without one runs exactly as 10.04 left it.
	class VAELEN_PLAYER_API Doings final : public IDoing
	{
	public:
		Doings(const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
			   Population::FamilyTypes InFamilies, Population::NeedTypes InNeeds, Economy::EconomyTypes InEconomy,
			   DoingRules InRules) noexcept
			: Types(InTypes), Persons(InPersons), Families(InFamilies), Needs(InNeeds), Economy(InEconomy),
			  Rules(InRules)
		{
		}
		Refusal Allows(const World& W, uint32 Person, const PlayerCommand& Command) const override;
		void Do(World& W, uint32 Person, const PlayerCommand& Command, SimTick Now, PersistentId Cause) override;

		/// What was actually done, for the tests and for 10.07: counted per kind.
		const uint32* Done() const noexcept { return Tally; }
		uint32 DoneOf(Intent Kind) const noexcept
		{
			return static_cast<usize>(Kind) < IntentCount ? Tally[static_cast<usize>(Kind)] : 0u;
		}

	private:
		/// The house a person keeps their goods in, 0 when they have none (then
		/// the region's common stock is theirs to work with).
		uint32 HouseOf(const World& W, uint32 Person) const;
		/// The region a living person is in, 0 when there is no such person.
		uint32 RegionOf(const World& W, uint32 Person) const;
		/// Units of the work good a person can lay hands on.
		uint32 HasGoods(const World& W, uint32 Person) const;

		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		Population::NeedTypes Needs;
		Economy::EconomyTypes Economy;
		DoingRules Rules;
		mutable WorldGen::RegionGraphCache Ways; ///< who a region's neighbours are (02.06)
		uint32 Tally[IntentCount] = {};
	};

	struct DoingStats
	{
		uint32 Acts = 0;	///< PlayerActed events in the log
		uint32 Refused = 0; ///< PlayerRefused events in the log
		uint32 Worked = 0;	///< of the acts, by kind
		uint32 Ate = 0;
		uint32 Rested = 0;
		uint32 Moved = 0; ///< PersonMoved events caused by an act
		uint32 Gave = 0;
		uint32 Took = 0;
		uint32 Caused = 0; ///< events whose cause is one of the acts
	};

	/// Reads the log: what the played person did, and how much of the world
	/// moved because of it. Everything here comes from events, so it says what
	/// the world remembers rather than what the player module believes.
	VAELEN_PLAYER_API DoingStats MeasureDoings(const World& W);
} // namespace Vaelen::Player
