// VAELEN - VaelenPopulation
// Phase 18.05: who is cold - a person's chill in a component of its own,
// declared only in a climate world; the cause; the winter's two events.
//
// STATUS: PROTOTYPE (Phase 18) - unit/deterministic tests in Tests/Population
//
// ZERO MEANS WARM (ADR-0153). PersonNeeds is a Phase 04 VALIDATED struct whose
// eight bytes join the state digest of every world that declares the module,
// so the chill is not carved into its reserved word: it is a component of its
// own, declared by WarmthTypes only when the climate is asked for - after
// Polity and before Colony in every wiring - and observed by NeedSystem through
// ObserveWinter as RegionRation is. A world not asked for a climate declares
// nothing here, and its digests are the digests it had by construction, not
// by the promise to keep a byte at zero.
//
// The chill is a burden: what a winter put on a person (18.06 by the year,
// 18.08 by the day), judged once a year by NeedSystem beside hunger and
// plague, and spent by the year as the ration spends hunger. The two events
// are declared here and published by Economy's WinterSystem (18.06), as Sim
// declares DisasterStruckEvent for everybody who publishes one.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/Event.h"

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	/// Component on a person entity while it lives in a detailed region of a
	/// climate world. Zero is warm.
	struct PersonWarmth
	{
		uint8 Chill = 0;	 ///< 0 warm .. 255 frozen through; above WarmthRules::ChillLine the year hurts
		uint8 ColdYears = 0; ///< years in a row judged cold (saturates)
		uint8 Clad = 0;		 ///< named for cloth; nothing writes it yet
		uint8 Reserved = 0;
		uint32 Reserved2 = 0;
	};
	static_assert(sizeof(PersonWarmth) == 8, "PersonWarmth must stay padding free");

	struct WarmthTypes
	{
		ComponentType<PersonWarmth> Warmth;
		static VAELEN_POPULATION_API WarmthTypes Declare(World& W);
	};

	struct WarmthRules
	{
		uint32 ChillLine = 100;		///< a chill above this hurts at the year's judgement
		uint32 ColdDamage = 40;		///< health lost in a cold year, plus a draw below the excess
		uint32 ChillRecovery = 200; ///< chill the year takes off everyone it judged (floor 0)
	};

	/// What a winter did to a region: the payload of Winter (the one just
	/// lain) and WinterForeseen (the one coming, no deaths yet).
	struct WinterPayload
	{
		uint32 Region = 0;
		uint32 Severity = 0; ///< 0..3 by ClimateRules::SeverityDegreeDays
		uint32 ColdSum = 0;	 ///< degree-days below the cold line
		uint32 Deaths = 0;	 ///< coarse deaths of the cold; 0 for the one foreseen
	};
	inline constexpr EventType<WinterPayload> WinterEvent = MakeEventType<WinterPayload>("Winter");
	inline constexpr EventType<WinterPayload> WinterForeseenEvent = MakeEventType<WinterPayload>("WinterForeseen");

	/// Chills one living person: raises Chill by at most Amount, saturating at
	/// frozen through. Returns what was actually added: 0 for an unknown
	/// person, a dead one, or one carrying no warmth - a coarse region's people
	/// carry none, a person of a region promoted since the last year's turn has
	/// none yet, and a world without a climate declares none. Defined beside
	/// FeedPerson in Needs.cpp, on the same lookup.
	VAELEN_POPULATION_API uint32 ChillPerson(World& W, const PersonTypes& Persons, const WarmthTypes& Warmth,
											 uint32 Person, uint32 Amount);
	/// Warms one living person: takes at most Amount off Chill, floored at
	/// warm. Returns what was taken, under the same rules.
	VAELEN_POPULATION_API uint32 WarmPerson(World& W, const PersonTypes& Persons, const WarmthTypes& Warmth,
											uint32 Person, uint32 Amount);

	struct WarmthStats
	{
		uint32 WithWarmth = 0; ///< living persons carrying warmth
		uint32 Cold = 0;	   ///< of them, above the line right now
		uint64 ChillSum = 0;
		uint32 ColdDeaths = 0; ///< from the log: PersonDied with DeathCause::Cold
	};
	/// Region 0 measures the world.
	VAELEN_POPULATION_API WarmthStats MeasureWarmth(const World& W, const PersonTypes& Persons,
													const WarmthTypes& Warmth, const WarmthRules& Rules, uint32 Region);
} // namespace Vaelen::Population
