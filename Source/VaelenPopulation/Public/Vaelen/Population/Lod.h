// VAELEN - VaelenPopulation
// Phase 04.06: the LOD bridge - regions promoted and demoted on request, and
// persons who cross the border between the two grains.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/long-duration tests in Tests/Population
//
// A LodState holds the regions the world wants detailed (the player's region,
// its neighbours, a chronicle's focus). The yearly LodSystem promotes what is
// wanted and demotes what is not, and lets people cross the border: a
// detailed region past its capacity sends young adults to the neighbour with
// the most room (the persons stay as history, gone from every count; the counts arrive), and a crowded coarse
// neighbour sends people into a detailed region with room (the counts leave,
// persons arrive). Every crossing is an event about the person.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/System.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	/// Singleton component: what the world wants detailed, and the bridge's tallies.
	struct LodState
	{
		static constexpr uint32 MaxWanted = 8;
		uint32 Wanted[MaxWanted] = {}; ///< region indices, 0 = free slot
		uint32 WantedCount = 0;
		uint32 Promotions = 0;
		uint32 Demotions = 0;
		uint32 Emigrants = 0;  ///< persons who left a detailed region for a coarse one
		uint32 Immigrants = 0; ///< persons who arrived from a coarse region
		uint32 Refused = 0;	   ///< promotions refused (too many persons)
	};
	static_assert(sizeof(LodState) == 4 * (LodState::MaxWanted + 6), "LodState must stay padding free");

	/// Component on a person the crossings of 04.06 must leave where they are.
	///
	/// The bridge sends unmarried adults of a crowded region to a neighbour and
	/// turns them into counts, which is right for a world and wrong for anybody
	/// the world outside the simulation is holding on to: Phase 10's player is
	/// exactly the profile it picks - alive, unmarried, of an age to travel -
	/// and a player who is emigrated stops being a person to play. Whoever has a
	/// reason to hold somebody in the fine grain marks them, and the bridge
	/// skips them. Nothing else in the project has such a reason yet.
	struct PersonHeld
	{
		uint32 Why = 0; ///< free for the holder to say why; 0 is fine
		uint32 Reserved = 0;
	};
	static_assert(sizeof(PersonHeld) == 8, "PersonHeld must stay padding free");

	struct LodTypes
	{
		ComponentType<LodState> State;
		ComponentType<PersonHeld> Held;
		static VAELEN_POPULATION_API LodTypes Declare(World& W);
	};

	struct LodRules
	{
		uint32 MaxDetailed = 4;			  ///< regions detailed at once
		MaterialiseRules Materialise;	  ///< how a promotion builds persons
		uint32 CrowdedPerMille = 750;	  ///< a region is crowded above this share of its capacity
		uint32 RoomPerMille = 650;		  ///< and has room below this share
		uint32 LeaveSharePerMille = 200;  ///< share of the crowd over the line that leaves a year
		uint32 ArriveSharePerMille = 200; ///< share of a crowded neighbour's crowd that arrives a year
		uint32 MoverFrom = 16;			  ///< movers are unmarried adults of these ages
		uint32 MoverTo = 40;
		uint32 FemalePerMille = 500;
	};

	struct LodPayload
	{
		uint32 Region = 0;
		uint32 Persons = 0;
		uint32 Promotions = 0; ///< promotions of this region so far
		uint32 Reserved = 0;
	};
	inline constexpr EventType<LodPayload> RegionPromotedEvent = MakeEventType<LodPayload>("RegionPromoted");
	inline constexpr EventType<LodPayload> RegionDemotedEvent = MakeEventType<LodPayload>("RegionDemoted");
	/// PersonPayload::Other carries the other region.
	inline constexpr EventType<PersonPayload> PersonLeftEvent = MakeEventType<PersonPayload>("PersonLeft");
	inline constexpr EventType<PersonPayload> PersonArrivedEvent = MakeEventType<PersonPayload>("PersonArrived");
	/// A person walked from one detailed region to another, and is still a
	/// person in both (unlike a crossing, which turns one into counts).
	/// PersonPayload::Other carries the region left behind.
	inline constexpr EventType<PersonPayload> PersonMovedEvent = MakeEventType<PersonPayload>("PersonMoved");

	/// The state (created on first use).
	VAELEN_POPULATION_API LodState& LodStateOf(World& W, const LodTypes& Types);
	/// Asks for a region to be detailed at the next yearly tick; false when the list is full.
	VAELEN_POPULATION_API bool RequestDetail(World& W, const LodTypes& Types, uint32 Region);
	/// Lets a region go coarse at the next yearly tick; false when it was not wanted.
	VAELEN_POPULATION_API bool ReleaseDetail(World& W, const LodTypes& Types, uint32 Region);
	VAELEN_POPULATION_API bool IsWanted(const World& W, const LodTypes& Types, uint32 Region);

	/// Holds a living person in the fine grain: the crossings will not take
	/// them. False for an unknown or dead person, or one already held.
	VAELEN_POPULATION_API bool HoldPerson(World& W, const PersonTypes& Persons, const LodTypes& Types, uint32 Person,
										  uint32 Why = 0);
	/// Lets them go again. False when they were not held.
	VAELEN_POPULATION_API bool FreePerson(World& W, const PersonTypes& Persons, const LodTypes& Types, uint32 Person);
	/// Whether the crossings will leave this person where they are.
	VAELEN_POPULATION_API bool IsHeld(const World& W, const PersonTypes& Persons, const LodTypes& Types, uint32 Person);

	/// Moves one living person from the detailed region they are in to another
	/// detailed region and reconciles the coarse counts of both, so that the two
	/// grains still agree afterwards. False when the person is unknown or not
	/// alive, when either region is not detailed, or when they are already
	/// there; nothing changes then. Publishes PersonMovedEvent with the cause.
	///
	/// This is the only way anything in the project moves a person between
	/// regions, and it lives here because 04.06 is what owns a person changing
	/// the region they are counted in. The player of Phase 10 walks through it
	/// like everything else.
	VAELEN_POPULATION_API bool MovePerson(World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
										  uint32 Person, uint32 To, SimTick Now, PersistentId Cause = {});

	/// Yearly: demotions, promotions, then the crossings.
	class VAELEN_POPULATION_API LodSystem final : public ISystem
	{
	public:
		LodSystem(World& InWorld, const History::PreHistoryTypes& InTypes, PersonTypes InPersons, LodTypes InLod,
				  LodRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Lod(InLod), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Lod"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Lives"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		LodTypes Lod;
		LodRules Rules;
		WorldGen::RegionGraphCache Roads; ///< derived, rebuilt when the map it came from is replaced
	};

	struct LodStats
	{
		uint32 Detailed = 0;
		uint32 Wanted = 0;
		uint32 Promotions = 0;
		uint32 Demotions = 0;
		uint32 Emigrants = 0;
		uint32 Immigrants = 0;
		uint32 Refused = 0;
		uint32 LeftEvents = 0; ///< from the log
		uint32 ArrivedEvents = 0;
	};
	VAELEN_POPULATION_API LodStats MeasureLod(const World& W, const History::PreHistoryTypes& Types,
											  const PersonTypes& Persons, const LodTypes& Types_);
} // namespace Vaelen::Population
