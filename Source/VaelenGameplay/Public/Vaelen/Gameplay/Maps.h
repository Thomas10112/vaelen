// VAELEN - VaelenGameplay
// Phase 12 task 12.04: maps.
//
// A map is the one document the world can check. 12.03's documents say what
// their writer thought of somebody, and nothing can ever say whether that was
// right - an opinion has no truth to be measured against. A map says something
// about GROUND, and the ground is right there: 02.06 built the region graph and
// `AreAdjacent` will answer for any claim anybody makes.
//
// So this is where the two things Phase 12 has been separating - what is true,
// and what somebody believes - can finally be held side by side and compared.
// A person who walked from one region to the next knows a thing. A person who
// read it off a page knows the same thing exactly as well, and has no way at
// all to tell whether the page was right.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/edge tests in Tests/Gameplay/Test_Maps.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Gameplay/GameplayApi.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Regions.h"

namespace Vaelen
{
	class World;
}

namespace Vaelen::Gameplay
{
	/// How much ground one person can hold in their head. The same handful
	/// everything else in this phase uses, and for the same reason.
	inline constexpr usize MostGround = 8;
	/// How many claims one map carries.
	inline constexpr usize MostClaims = 8;

	/// Component on a person: the ground they can name, and which of it they
	/// have actually stood on. A person who read a region off a page names it
	/// exactly as readily as one who walked there, and that is the point.
	struct PersonGround
	{
		uint16 Known[MostGround] = {}; ///< region indices
		uint32 Walked = 0;			   ///< bit i set when Known[i] was stood on rather than read
		uint32 Count = 0;
		uint64 Since = 0;
	};
	static_assert(sizeof(PersonGround) == 32, "PersonGround must stay padding free");

	/// Component of a map entity (ids of kind Map): claims that two regions
	/// touch. Each is either true of the world or it is not, and the world will
	/// say which.
	struct MapInfo
	{
		uint32 Index = 0;
		uint32 Writer = 0;
		uint32 Holder = 0;
		uint32 Claims = 0;
		uint16 From[MostClaims] = {};
		uint16 To[MostClaims] = {};
		uint32 Forged = 0; ///< claims put on it by somebody who had not walked them
		uint32 Reserved = 0;
		uint64 Written = 0;
		uint64 Lost = 0;
		Hash64 Identity = 0;
	};
	static_assert(sizeof(MapInfo) == 80, "MapInfo must stay padding free");

	struct MapTypes
	{
		ComponentType<PersonGround> Ground;
		ComponentType<MapInfo> Map;

		static VAELEN_GAMEPLAY_API MapTypes Declare(World& W);
	};

	inline constexpr EventType<Player::ActPayload> MapWrittenEvent = MakeEventType<Player::ActPayload>("MapWritten");
	inline constexpr EventType<Player::ActPayload> MapReadEvent = MakeEventType<Player::ActPayload>("MapRead");

	/// Notes that a person is standing where they are standing. Called for the
	/// people of ground that is simulated person by person; a person who never
	/// leaves their region ends up knowing exactly one.
	VAELEN_GAMEPLAY_API bool NoteGround(World& W, const Population::PersonTypes& Persons, const MapTypes& Maps,
										uint32 Person, SimTick Now);
	/// The ground a person can name (nullptr when they have never been noted).
	VAELEN_GAMEPLAY_API const PersonGround* GroundOf(const World& W, const Population::PersonTypes& Persons,
													 const MapTypes& Maps, uint32 Person);
	/// True when they can name it at all, walked or read.
	VAELEN_GAMEPLAY_API bool CanName(const PersonGround& G, uint32 Region) noexcept;
	/// True when they have stood on it.
	VAELEN_GAMEPLAY_API bool HasWalked(const PersonGround& G, uint32 Region) noexcept;

	/// Draws what a person can name: every pair of regions they have WALKED,
	/// claimed to touch. A map of walked ground is always true of the world,
	/// which is what makes a forged one worth anything.
	VAELEN_GAMEPLAY_API uint32 WriteMap(World& W, const Population::PersonTypes& Persons, const MapTypes& Maps,
										uint32 Writer, SimTick Now);
	/// Puts a claim on a map that nobody walked. Returns false when the map is
	/// full or lost. This is the only way a map becomes wrong, and it is
	/// deliberate: ground does not move, so a map is wrong because somebody drew
	/// it wrong.
	VAELEN_GAMEPLAY_API bool ForgeClaim(World& W, const MapTypes& Maps, uint32 Map, uint32 From, uint32 To);
	/// Somebody reads one. Every region on it becomes ground they can NAME and
	/// have not walked - including the ground that is not there.
	VAELEN_GAMEPLAY_API bool ReadMap(World& W, const Population::PersonTypes& Persons, const MapTypes& Maps, uint32 Map,
									 uint32 Reader, SimTick Now);

	VAELEN_GAMEPLAY_API const MapInfo* MapOf(const World& W, const MapTypes& Maps, uint32 Map);

	struct MapCheck
	{
		uint32 Claims = 0;
		uint32 True_ = 0;  ///< claims the region graph agrees with
		uint32 False_ = 0; ///< claims it does not
	};
	/// The world reading a map back. This is the thing no other document allows.
	VAELEN_GAMEPLAY_API MapCheck CheckMap(const World& W, const WorldGen::RegionGraph& Graph, const MapTypes& Maps,
										  uint32 Map);

	struct MapStats
	{
		uint32 Maps = 0;
		uint32 Claims = 0;
		uint32 Forged = 0;
		uint32 KnowGround = 0; ///< people who can name any ground at all
		uint32 Walked = 0;	   ///< of what they name, what they stood on
		uint32 Read = 0;	   ///< and what they only read
		Hash64 Digest = 0;
	};
	VAELEN_GAMEPLAY_API MapStats MeasureMaps(const World& W, const Population::PersonTypes& Persons,
											 const MapTypes& Maps);
} // namespace Vaelen::Gameplay
