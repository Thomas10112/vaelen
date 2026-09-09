// VAELEN - VaelenPlayer
// Phase 10.06: what the people around the player make of them.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player
//
// The usual way to build this is a dialogue tree with a reputation number
// hanging off it: the player picks line 2 rather than line 1, somebody's
// approval goes up four, and what the world thinks of them is a record of which
// buttons were pressed. This is not that, and cannot become that.
//
// An opinion here is read out of the event log. 10.04 and 10.05 put every act
// in it - who did it, what kind it was, who it was aimed at - and this walks
// those records and nothing else. Nobody has an opinion about an intent, a
// menu, or a thing the player meant to do; they have opinions about what was
// done to them, which is the only thing the world remembers.
//
// The standing of 05.02 enters in one place and one way: an opinion is worth
// what its holder is worth. The region's repute of the player is the opinions
// it has weighted by the RANK of who holds them, so the head of a house
// thinking well of you counts for more than a field hand doing the same. That
// is a read of 05.02 and never a write to it - the player's own standing is
// still what StandingSystem says it is, from the house, the office, the traits
// and the years, exactly as for everybody else in the world.
//
// And the world forgets. An opinion drifts back towards nothing when nothing
// more happens, at a rate a year can be measured in, so that a kindness done
// once is not a claim on somebody for ever.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerApi.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/Standing.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Player
{
	/// What one person makes of the player.
	struct Opinion
	{
		uint32 Person = 0;	 ///< who holds it
		int32 Regard = 0;	 ///< -1000 (wronged) .. 1000 (owes them everything)
		uint32 Met = 0;		 ///< times the player did something to them
		uint32 Reserved = 0; //
		uint64 Last = 0;	 ///< tick of the last of those times
	};
	static_assert(sizeof(Opinion) == 24, "Opinion must stay padding free");

	/// How many people the played person is remembered by at once. A person is
	/// known to the handful they have actually dealt with, not to a region.
	inline constexpr usize MostKnown = 8;

	/// Component on the played person: who knows them, and what the place at
	/// large makes of them.
	struct PlayerRegard
	{
		uint32 Known = 0;	   ///< how many of Who are in use
		uint32 Kindnesses = 0; ///< things done for somebody, over the life
		uint32 Wrongs = 0;	   ///< things taken from somebody
		uint32 Drift = 0;	   ///< carry towards the next day of forgetting
		int32 Repute = 0;	   ///< the opinions, weighted by the standing of who holds them
		uint32 Reserved = 0;   //
		uint64 Since = 0;	   ///< tick the first of them was formed
		Opinion Who[MostKnown];
	};
	static_assert(sizeof(PlayerRegard) == 224, "PlayerRegard must stay padding free");

	struct RegardTypes
	{
		ComponentType<PlayerRegard> Regard;
		static VAELEN_PLAYER_API RegardTypes Declare(World& W);
	};

	struct RegardRules
	{
		int32 ForSpeaking = 15;	   ///< a word is worth little, and it is worth something
		int32 ForGiving = 60;	   ///< per giving, plus what was given
		int32 PerUnitGiven = 30;   //
		int32 ForTaking = -120;	   ///< per taking, plus what was taken
		int32 PerUnitTaken = -40;  //
		int32 Most = 1000;		   ///< nobody is owed more than this
		int32 Least = -1000;	   ///< nor wronged worse
		uint32 ForgetPerYear = 90; ///< points an untouched opinion drifts back a year
		/// How much of an opinion reaches the place at large, per mille, before
		/// the standing of whoever holds it is weighed in.
		uint32 ReputeSharePerMille = 400;
	};

	/// Daily, after PlayerOrders: reads the acts of this tick out of the log and
	/// nothing else, then lets the older opinions drift. It writes one component
	/// of its own and reads 05.02; it changes nothing any other module owns.
	class VAELEN_PLAYER_API RegardSystem final : public ISystem
	{
	public:
		RegardSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					 PlayerTypes InPlayer, Society::StandingTypes InStanding, RegardTypes InRegard,
					 RegardRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Player(InPlayer), Standing(InStanding),
			  Regard(InRegard), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "PlayerRegard"; }
		SimLod GetLod() const noexcept override { return SimLod::Aggregate; }
		std::vector<std::string_view> GetDependencies() const override
		{
			// After the acts of the day exist, because they are what it reads.
			std::vector<std::string_view> Out{"PlayerOrders"};
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
		Population::PersonTypes Persons;
		PlayerTypes Player;
		Society::StandingTypes Standing;
		RegardTypes Regard;
		RegardRules Rules;
	};

	/// What the world holds about the played person (nullptr when nobody has an
	/// opinion yet).
	VAELEN_PLAYER_API const PlayerRegard* RegardOf(const World& W, const RegardTypes& Regard);
	/// What one person makes of them, 0 when they have never dealt with them.
	VAELEN_PLAYER_API int32 RegardFrom(const World& W, const RegardTypes& Regard, uint32 Person);
	/// What the place at large makes of them: the opinions, weighted by the
	/// standing of whoever holds them.
	VAELEN_PLAYER_API int32 ReputeOf(const World& W, const RegardTypes& Regard);

	struct RegardStats
	{
		uint32 Records = 0; ///< regard records in the world; more than one is incoherent
		uint32 Known = 0;	///< people with an opinion
		uint32 Friends = 0; ///< of those, thinking well
		uint32 Enemies = 0; ///< and ill
		int32 Repute = 0;	//
		uint32 Kindnesses = 0;
		uint32 Wrongs = 0;
		uint32 Bad = 0; ///< incoherent: see MeasureRegard
	};

	/// Counts the opinions and checks what they must keep: one record to a
	/// world, on the person being played, never more opinions than the table
	/// holds, never one held by nobody, and none outside the bounds the rules
	/// set.
	VAELEN_PLAYER_API RegardStats MeasureRegard(const World& W, const Population::PersonTypes& Persons,
												const PlayerTypes& Player, const RegardTypes& Regard,
												const RegardRules& Rules);
} // namespace Vaelen::Player
