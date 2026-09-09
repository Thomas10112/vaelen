// VAELEN - VaelenPlayer
// Phase 10.01: the player as a marker on one person the world already had.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player
//
// The player is not a new kind of thing. Nine phases have built a world of
// people who are born, eat, work, are bound and freed, marry, hold office,
// march and die, and the player is ONE OF THEM: a mark on an existing person of
// a detailed region, nothing more.
//
// This task adds the mark and nothing else. It does not tick, it does not
// publish, it does not read anything at all, and that is the point: the whole
// architectural claim of Phase 10 is that a world with a player in it and the
// same world without one are the same world, tick for tick, until the player
// actually does something. 10.04 gives them a way to do something, and it goes
// through a system inside the simulation like everything else.
//
// A world can hold one mark at a time. Marking is a thing done to a person who
// exists, is alive, and lives in a region simulated person by person - a coarse
// region has no persons to be, only a count of them.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Player/PlayerApi.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Player
{
	/// Component on a person entity: the mark that says this is the one played.
	struct PlayerMark
	{
		uint32 Person = 0; ///< the person index, so the answer survives a snapshot
		uint32 Reserved = 0;
		uint64 Since = 0;	 ///< tick the mark was set
		Hash64 Identity = 0; ///< from the world seed
	};
	static_assert(sizeof(PlayerMark) == 24, "PlayerMark must stay padding free");

	struct PlayerTypes
	{
		ComponentType<PlayerMark> Mark;
		/// The hold of 04.06: people the crossings must leave where they are.
		/// Declared HERE, by the module with a reason to hold somebody, because
		/// a component type registered inside a lower module's Declare joins the
		/// type registry of every world that declares that module and moves its
		/// state digest - which broke the frozen digests of six closed phases
		/// when this was tried the other way round.
		ComponentType<Population::PersonHeld> Held;
		static VAELEN_PLAYER_API PlayerTypes Declare(World& W);
	};

	/// Marks an existing living person of a detailed region as the played one.
	/// False when the person is unknown, dead, or when somebody is already
	/// marked: a world holds one player at a time, and taking a second would
	/// leave the first behind with no way to say which was meant.
	///
	/// It also holds them in the fine grain (04.06), which a played person
	/// needs: the crossings send unmarried adults of a crowded region to a
	/// neighbour and turn them into counts, and the player is exactly the
	/// profile they pick. A gate that ran forty years without this found its
	/// person emigrated in the third year. The bridge honours the hold only
	/// when it was told to observe it (LodSystem::ObserveHeld).
	VAELEN_PLAYER_API bool TakePlayer(World& W, const Population::PersonTypes& Persons, const PlayerTypes& Player,
									  uint32 Person, SimTick Now);
	/// Removes the mark, and the hold with it. The world runs on exactly as it did.
	VAELEN_PLAYER_API bool ReleasePlayer(World& W, const PlayerTypes& Player,
										 const Population::PersonTypes* Persons = nullptr);
	/// The person index being played, 0 when nobody is.
	VAELEN_PLAYER_API uint32 PlayerPerson(const World& W, const PlayerTypes& Player);
	/// The mark itself (nullptr when nobody is played).
	VAELEN_PLAYER_API const PlayerMark* PlayerOf(const World& W, const PlayerTypes& Player);

	struct PlayerStats
	{
		uint32 Marks = 0;  ///< marks in the world; more than one is incoherent
		uint32 Alive = 0;  ///< of those, on a person still living
		uint32 Region = 0; ///< where the played person is, 0 when nobody is played
		uint32 Bad = 0;	   ///< incoherent: see MeasurePlayer
		Hash64 Digest = 0;
	};

	/// Counts the marks and checks what one must keep: one to a world, on a
	/// person that exists, whose index agrees with the person it sits on.
	VAELEN_PLAYER_API PlayerStats MeasurePlayer(const World& W, const Population::PersonTypes& Persons,
												const PlayerTypes& Player);
} // namespace Vaelen::Player
