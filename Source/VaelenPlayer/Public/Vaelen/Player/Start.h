// VAELEN - VaelenPlayer
// Phase 10.02: the enslaved start - a place in a world, not a character sheet.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player
//
// The player begins bound. That is not a story the game tells the player; it is
// a fact about somebody the world already made. 05.04 binds people every year -
// for debt, at birth, by capture, by being on the wrong side of a promotion -
// and 10.02 takes one of them and hands them to whoever is playing.
//
// So this task does not create a life. It LOOKS FOR ONE, and the search can
// fail: a world whose detailed regions hold nobody bound offers nobody, and
// this returns zero and writes nothing. Inventing a bound person to be would be
// writing state into the world from outside it, which is the one thing Phase 10
// is built not to do (ADR-0082).
//
// Where it looks is the ground a mining colony would stand on - a region with
// ore under it - because that is where Phase 11 puts the colony, and a start
// that has to move house when Phase 11 lands is not a start.
//
// What it records is what the life WAS at its first moment: the region, the
// bond and who held it, the family, the standing 05.02 had already given them,
// and their age. A record, never a rule: nothing reads PlayerStart to decide
// anything, and the world runs the same whether it is there or not.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerApi.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Standing.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Player
{
	/// Component on the played person: what the life was at its first moment.
	struct PlayerStart
	{
		uint32 Person = 0;
		uint32 Region = 0;
		uint32 Bond = 0;	 ///< Society::BondKind at the start
		uint32 Holder = 0;	 ///< person index of who held them, 0 = the region itself
		uint32 Family = 0;	 ///< family index, 0 = none
		uint32 Standing = 0; ///< the score 05.02 had already given them
		uint32 Age = 0;		 ///< years lived when the life was taken up
		uint32 OnOre = 0;	 ///< 1 when the ground had ore under it, 0 when the world offered none
		uint64 Began = 0;	 ///< tick
	};
	static_assert(sizeof(PlayerStart) == 40, "PlayerStart must stay padding free");

	struct StartTypes
	{
		ComponentType<PlayerStart> Start;
		static VAELEN_PLAYER_API StartTypes Declare(World& W);
	};

	struct StartRules
	{
		uint32 FromAge = 16; ///< old enough to work, young enough to have a life ahead
		uint32 ToAge = 40;
		uint32 WantBound = 1; ///< the start is a bound life; 0 takes whoever is there
		uint32 PreferOre = 1; ///< ground with ore under it first, where a mining colony would stand
	};

	/// Finds a life the world already made and hands it to the player: a living
	/// adult of a region simulated person by person, bound, on ground with ore
	/// under it where the world offers one there. Returns the person index, or 0
	/// when the world offers nobody who fits - and then nothing at all is written.
	///
	/// The ore is a PREFERENCE and not a requirement, and the record says which
	/// it got. The ground a mining colony would stand on and the ground the most
	/// people live on are not the same ground - at AELVOR 128 the two busiest
	/// regions have no ore under them at all - and a start that refused to
	/// happen over that would be a start that never happens.
	VAELEN_PLAYER_API uint32 BeginEnslaved(World& W, const History::PreHistoryTypes& Types,
										   const Population::PersonTypes& Persons, const Society::BondageTypes& Bondage,
										   const Society::StandingTypes& Standing, const PlayerTypes& Player,
										   const StartTypes& Start, const StartRules& Rules, SimTick Now,
										   const Population::LodTypes* Lod = nullptr);
	/// Ends the start, so that another life may be taken up. False when none was.
	///
	/// The record says what the life WAS at its first moment, so it belongs to
	/// the life and goes when the life does; what happened in it stays in the
	/// world's history, which is where it belongs. A world with mortality in it
	/// will not let one person be played for forty years, and the Phase 10 gate
	/// takes up another when one ends.
	VAELEN_PLAYER_API bool EndStart(World& W, const StartTypes& Start);
	/// What the life was at its first moment (nullptr when no life was taken up).
	VAELEN_PLAYER_API const PlayerStart* StartOf(const World& W, const StartTypes& Start);

	struct StartStats
	{
		uint32 Started = 0;	   ///< starts recorded; more than one is incoherent
		uint32 StillBound = 0; ///< of those, on somebody the world still has bound
		uint32 WithFamily = 0;
		uint32 Bad = 0; ///< incoherent: see MeasureStart
		Hash64 Digest = 0;
	};

	/// Counts the starts and checks what one must keep: one to a world, on the
	/// person the mark points at, saying what that person's first moment was.
	VAELEN_PLAYER_API StartStats MeasureStart(const World& W, const Population::PersonTypes& Persons,
											  const Society::BondageTypes& Bondage, const PlayerTypes& Player,
											  const StartTypes& Start);
} // namespace Vaelen::Player
