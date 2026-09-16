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
	///
	/// It is an OBSERVED type and not one of LodTypes, which matters more than it
	/// looks: registering a component type inside a lower module's Declare adds
	/// it to the type registry of every world that declares that module, which
	/// moves the state digest of every one of them. Doing it the other way round
	/// broke the frozen digests of six closed phases at once, and the CI matrix
	/// caught it. Whoever needs the hook declares it and hands it over.
	struct PersonHeld
	{
		uint32 Why = 0; ///< free for the holder to say why; 0 is fine
		uint32 Reserved = 0;
	};
	static_assert(sizeof(PersonHeld) == 8, "PersonHeld must stay padding free");

	struct LodTypes
	{
		ComponentType<LodState> State;
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
		/// A region the world keeps at the fine grain whatever else it wants:
		/// promoted before anything and never demoted while the rules say so.
		/// 0 is none, which is every world before Phase 11.
		///
		/// A colony is not a region that happens to be interesting this decade;
		/// it is the place the game is played, and it has to be detailed on the
		/// first tick and the hundred-thousandth alike. Wanting is a request the
		/// bridge weighs against MaxDetailed and against what else is wanted -
		/// holding is not. It lives in the RULES rather than in LodState because
		/// LodState is a component and every world that has one puts it in its
		/// state digest: adding a field there would move the frozen digest of
		/// every phase that ever declared 04.06, which is what ADR-0090 is about.
		uint32 Held = 0;

		/// When true, LodSystem does the CROSSINGS only, and something else
		/// decides what is detailed - a DetailSystem, below, running on a day
		/// instead of on a year.
		///
		/// Phase 15 task 15.02, and it defaults to false because of what the
		/// cadence is worth. LodSystem runs at SimLod::World: LodSchedule gives
		/// that level a period of 8640 ticks and AELVOR's calendar is one tick
		/// to the hour, so the whole bridge - demotions, promotions, crossings -
		/// fires ONCE PER SIMULATED YEAR. That is right for the crossings, whose
		/// rates are shares of a crowd that leaves in a year, and it is useless
		/// for the question "may I walk there": a region asked for in the spring
		/// is not walkable until the following spring. ADR-0139 appears to work
		/// immediately only because Aelvor::Begin ends exactly on a year
		/// boundary, so the next firing lands on the first day turn after a
		/// taking.
		///
		/// Splitting them is therefore not a tidy-up, it is the phase's
		/// premise - and it changes what is detailed WHEN, which changes
		/// everything downstream of it. So it is a rule and not a rewrite: a
		/// world that does not ask takes the path it has taken since 04.06, and
		/// the frozen digests of fourteen closed phases do not move. It lives in
		/// the RULES rather than in LodState for the reason the field above
		/// gives.
		bool DecideElsewhere = false;
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
	/// The bridge wanted to give a region up and did not, because somebody held
	/// is standing in it. LodPayload::Persons carries the heads that were NOT
	/// folded away, which is the number a demotion would have destroyed.
	inline constexpr EventType<LodPayload> RegionPinnedEvent = MakeEventType<LodPayload>("RegionPinned");
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

	/// Declares the hold type. Called by whoever holds people - the Player of
	/// Phase 10 - and by nobody else, so that a world with no reason to hold
	/// anybody has exactly the components it had before this existed.
	VAELEN_POPULATION_API ComponentType<PersonHeld> DeclareHold(World& W);
	/// Holds a living person in the fine grain: the crossings will not take
	/// them. False for an unknown or dead person, or one already held.
	VAELEN_POPULATION_API bool HoldPerson(World& W, const PersonTypes& Persons, ComponentType<PersonHeld> Held,
										  uint32 Person, uint32 Why = 0);
	/// Lets them go again. False when they were not held.
	VAELEN_POPULATION_API bool FreePerson(World& W, const PersonTypes& Persons, ComponentType<PersonHeld> Held,
										  uint32 Person);
	/// Whether the crossings will leave this person where they are.
	/// True when a LIVING person carrying PersonHeld stands in this region.
	///
	/// Phase 15 task 15.01. DemoteRegion destroys every person of a region, the
	/// dead included, and had no idea that one of them might be the person
	/// somebody is playing - while the crossings of this file have honoured
	/// PersonHeld since 04.06. Nothing released detail until 15.03, so no
	/// demotion ever reached a held person and the hole was only ever one fix
	/// away from being fatal. This is the question both layers ask.
	///
	/// It looks the type up BY NAME rather than taking it as an argument, and
	/// that is the point: PersonHeld is an observed type (see above - putting it
	/// in LodTypes would move the state digest of every world that declares
	/// 04.06), so there is no member to consult and no caller to trust. A world
	/// that never declared one answers false and behaves exactly as it did. A
	/// caller that would rather not pass the type cannot thereby skip the check.
	VAELEN_POPULATION_API bool HoldsSomebody(const World& W, const PersonTypes& Persons, uint32 Region);

	VAELEN_POPULATION_API bool IsHeld(const World& W, const PersonTypes& Persons, ComponentType<PersonHeld> Held,
									  uint32 Person);

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

	/// Daily: demotions and promotions, and nothing else.
	///
	/// Phase 15 task 15.02. This is the half of the bridge that answers "what is
	/// the world paying attention to", and it is the half that cannot wait a
	/// year. It runs at SimLod::Aggregate - 24 ticks, one day of AELVOR - and it
	/// touches no random stream, because deciding what to detail is not a thing
	/// chance should have a say in.
	///
	/// A world that adds one MUST set LodRules::DecideElsewhere on the LodSystem
	/// beside it, or the two will both promote and both demote. Nothing enforces
	/// that here; Run::Aelvor does it in one place, which is what that class is
	/// for.
	class VAELEN_POPULATION_API DetailSystem final : public ISystem
	{
	public:
		DetailSystem(World& InWorld, const History::PreHistoryTypes& InTypes, PersonTypes InPersons, LodTypes InLod,
					 LodRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Lod(InLod), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Detail"; }
		SimLod GetLod() const noexcept override { return SimLod::Aggregate; }
		std::vector<std::string_view> GetDependencies() const override { return {"Lives"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		LodTypes Lod;
		LodRules Rules;
	};

	/// Yearly: demotions, promotions, then the crossings - unless
	/// LodRules::DecideElsewhere, in which case the crossings only.
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
		/// Optional: people the crossings must leave where they are (Phase 10).
		/// Without it the bridge behaves exactly as it did before the hold
		/// existed, which is what keeps six closed phases frozen.
		void ObserveHeld(ComponentType<PersonHeld> InHeld) noexcept
		{
			Held = InHeld;
			HasHeld = true;
		}
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		LodTypes Lod;
		LodRules Rules;
		ComponentType<PersonHeld> Held;
		bool HasHeld = false;
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
