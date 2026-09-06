// VAELEN - VaelenPopulation
// Phase 04.02: ageing, mortality, fertility and the reconciliation of the grains.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population
//
// In a detailed region the persons are the truth: every year the LifeSystem
// ages them, lets some die by age band, lets couples have children, and then
// writes the living back into the region's coarse counts (people per culture,
// believers per faith). The coarse systems of Phase 03 skip detailed regions
// (they observe the RegionLod marker), so nothing moves those counts but the
// persons. Every birth and death is an event about the person.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	struct LifeRules
	{
		static constexpr uint32 Bands = 9;
		/// Upper age (exclusive, years) of each band; the last is open.
		uint32 BandEnd[Bands] = {1, 5, 15, 30, 45, 60, 75, 90, 0xffffffffu};
		/// Yearly deaths per mille in each band.
		uint32 DeathsPerMille[Bands] = {60, 15, 3, 4, 8, 18, 40, 150, 500};
		uint32 FertileFrom = 16;		   ///< a woman may bear from this age (years)
		uint32 FertileTo = 40;			   ///< and below this one
		uint32 FatherFrom = 16;			   ///< a father from this age
		uint32 FatherTo = 55;			   ///< and below this one
		uint32 BirthsPerMille = 480;	   ///< yearly chance per fertile woman at full room
		uint32 MinimumBirthsPerMille = 50; ///< chance kept when the region is at capacity
		uint32 FemalePerMille = 500;	   ///< sex of a newborn
	};

	struct PersonPayload
	{
		uint32 Person = 0; ///< person index
		uint32 Region = 0;
		uint32 AgeYears = 0;
		uint32 Other = 0; ///< mother (birth) or the disaster kind + 1 (death), 0 = natural
	};
	inline constexpr EventType<PersonPayload> PersonBornEvent = MakeEventType<PersonPayload>("PersonBorn");
	inline constexpr EventType<PersonPayload> PersonDiedEvent = MakeEventType<PersonPayload>("PersonDied");

	/// Age in whole years at a tick (0 when born later than the tick).
	VAELEN_POPULATION_API uint32 AgeYears(const PersonInfo& P, uint64 Tick) noexcept;
	/// Band of an age under the rules.
	VAELEN_POPULATION_API uint32 BandOf(uint32 Age, const LifeRules& Rules) noexcept;

	/// Writes the living persons of a detailed region into its coarse counts
	/// (people per culture, believers per faith). Returns false when the region
	/// is not detailed. Capacity is untouched.
	VAELEN_POPULATION_API bool ReconcileRegion(World& W, const History::PreHistoryTypes& Types,
											   const PersonTypes& Persons, uint32 Region);

	/// Yearly, for every detailed region: deaths by age band, births to couples
	/// scaled by the room left in the region, then reconciliation.
	class VAELEN_POPULATION_API LifeSystem final : public ISystem
	{
	public:
		LifeSystem(World& InWorld, const History::PreHistoryTypes& InTypes, PersonTypes InPersons,
				   LifeRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Lives"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Population"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		LifeRules Rules;
	};

	struct LifeStats
	{
		uint32 Alive = 0;
		uint32 Dead = 0;
		uint32 Children = 0; ///< under 15
		uint32 Elders = 0;	 ///< 60 and over
		uint32 Oldest = 0;	 ///< years
		uint64 AgeSum = 0;	 ///< years, over the living
		uint32 BornHere = 0; ///< living persons with a known mother
	};
	VAELEN_POPULATION_API LifeStats MeasureLives(const World& W, const PersonTypes& Persons, uint32 Region,
												 uint64 Tick);
} // namespace Vaelen::Population
