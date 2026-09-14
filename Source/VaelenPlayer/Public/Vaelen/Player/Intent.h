// VAELEN - VaelenPlayer
// Phase 14.01: the command surface as a LEAF - what an intent is, and nothing
// about the door it goes through.
//
// STATUS: PROTOTYPE (Phase 14) - probe TU Tests/Player/Probe_IntentLeaf.cpp, tests in Tests/Player/Test_Stream.cpp
//
// Commands.h declared both halves of the player's command surface in one
// header: the SHAPE of an intent (these types) and the DOOR into the simulation
// (Submit(World&...), the queue, the system). It includes Sim/System.h and
// Population/Persons.h to do so. A file that needs only the shape - a UI that
// turns a keypress into a PlayerCommand - could not include it without also
// being able to name World&. Phase 14's rule 1 is an include rule, and this is
// the header that lets a consumer keep it: includes CoreTypes.h and PlayerApi.h
// and nothing else, so that Submit is not nameable from a file that includes
// only this.
//
// Nothing moved but declarations. Commands.h includes this back, every symbol
// keeps its name and its layout, and PlayerCommand is still 24 bytes - the
// static_assert came along with it. ADR-0136.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Player/PlayerApi.h"

namespace Vaelen::Player
{
	/// What a person can want to do. The seven doings of 10.05 are named here so
	/// that a recorded stream keeps its meaning from one task to the next.
	enum class Intent : uint8
	{
		None = 0,
		Wait, ///< let the hours go by; the one intent that is complete in 10.04
		Work,
		Rest,
		Eat,
		Move,
		Speak,
		Give,
		Take,
		Count
	};
	inline constexpr usize IntentCount = static_cast<usize>(Intent::Count);
	VAELEN_PLAYER_API const char* IntentName(Intent Kind);

	/// Why the world refused an intent. Kept on the command and counted on the
	/// queue, because "nothing happened" is not an answer a player can act on.
	///
	/// Two families, and a screen must not confuse them: NoPlayer, Unknown and
	/// Full are what the DOOR (Submit) can answer; everything else is applied
	/// later by the system inside the simulation and counted on the queue.
	/// None means queued, not done.
	enum class Refusal : uint8
	{
		None = 0,
		NoPlayer, ///< nobody is played, or the played person has no queue
		Dead,	  ///< the person the intent was for is no longer alive
		Unknown,  ///< an intent this build has no name for
		Costly,	  ///< it asks for more hours than a whole day has
		Full,	  ///< the queue is already holding all it can
		Stale,	  ///< it waited so long unapplied that it is no longer meant
		Nothing,  ///< there is nothing to do it with: no grain to eat, none to give
		TooFar,	  ///< the region is not one a person can walk to from here
		NoOne,	  ///< the person it is aimed at is not here, or not alive
		Count
	};
	VAELEN_PLAYER_API const char* RefusalName(Refusal Why);

	/// One intent. Small, flat and copyable, because a recorded stream of these
	/// IS the player's half of a replay: seed plus stream gives the same life.
	struct PlayerCommand
	{
		uint8 Kind = 0;		 ///< Intent
		uint8 Why = 0;		 ///< Refusal, filled in when the world refused it
		uint16 Reserved = 0; //
		uint32 Target = 0;	 ///< what it is aimed at; the kind says what that means
		uint32 Amount = 0;	 ///< how much of it
		uint32 Hours = 0;	 ///< hours asked; 0 takes the rule's cost for the kind
		uint64 Issued = 0;	 ///< the tick it was submitted on
	};
	static_assert(sizeof(PlayerCommand) == 24, "PlayerCommand must stay padding free");
} // namespace Vaelen::Player
