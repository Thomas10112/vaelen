// VAELEN - VaelenPopulation
// Phase 04.01: persons and the two grains of population.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population
//
// Population has two grains and one truth. The coarse grain is the Phase 03
// `RegionPopulation` (integer counts per culture) and `RegionFaith` (believers
// per religion) on every region. The fine grain is one `PersonInfo` entity per
// person, and it exists only in regions that were promoted to detail. Promotion
// materialises exactly the counts (culture by culture, faith by faith, sex and
// age drawn from a hash stream of the world seed, the region and the tick), and
// demotion folds the living persons back into the counts. Both are pure
// functions of the state: two worlds promote the same region into the same
// persons, and a promote / demote round trip leaves the counts as they were.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/PreHistory.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	enum class Sex : uint8
	{
		Female = 0,
		Male = 1,
	};

	enum class LifeState : uint8
	{
		Alive = 0,
		Dead = 1,
	};

	/// Component of a person entity (ids of kind Person).
	struct PersonInfo
	{
		uint32 Index = 0;	 ///< 1-based, in order of creation in the world
		uint32 Region = 0;	 ///< region index where the person lives (or died)
		uint32 Culture = 0;	 ///< culture index (never 0)
		uint32 Religion = 0; ///< religion index, 0 = none
		uint32 Language = 0; ///< language index, 0 = none yet
		uint32 Family = 0;	 ///< family index (04.03), 0 = none
		uint32 Mother = 0;	 ///< person index, 0 = unknown (materialised)
		uint32 Father = 0;	 ///< person index, 0 = unknown (materialised)
		uint64 Born = 0;	 ///< tick of birth (may precede the world's first tick)
		uint64 Died = 0;	 ///< tick of death, 0 while alive
		Hash64 Identity = 0; ///< seed of traits and names, from the world seed
		uint8 Sex = 0;		 ///< Sex
		uint8 State = 0;	 ///< LifeState
		uint8 Reserved[6] = {};
	};
	static_assert(sizeof(PersonInfo) == 64, "PersonInfo must stay padding free");

	/// Component on a region entity while the region is detailed.
	struct RegionDetail
	{
		uint32 Region = 0;
		uint32 Persons = 0;	   ///< persons created by the promotion
		uint64 PromotedAt = 0; ///< tick
		uint32 Promotions = 0; ///< how many times this region was promoted
		uint32 Reserved = 0;
	};
	static_assert(sizeof(RegionDetail) == 24, "RegionDetail must stay padding free");

	struct PersonTypes
	{
		ComponentType<PersonInfo> Person;
		ComponentType<RegionDetail> Detail;
		static PersonTypes Declare(World& W);
	};

	struct MaterialiseRules
	{
		uint32 MaxPersonsPerRegion = 20000; ///< a promotion above this is refused
		uint32 FemalePerMille = 500;
		uint32 MaxAgeYears = 70;		///< oldest materialised person
		uint32 YoungHalfPerMille = 500; ///< share of persons under MaxAge / 3 (a young pyramid)
	};

	/// Persons created by a promotion, per culture, so the coarse counts can
	/// be compared with the fine grain.
	struct RegionCensus
	{
		uint32 Region = 0;
		uint32 Alive = 0;
		uint32 Dead = 0;
		uint32 Female = 0;
		uint32 Male = 0;
		uint32 ByCulture[History::RegionPopulation::MaxCultures] = {};
		uint32 CultureOf[History::RegionPopulation::MaxCultures] = {};
		uint32 ByFaith[History::RegionFaith::MaxFaiths] = {};
		uint32 FaithOf[History::RegionFaith::MaxFaiths] = {};
		uint32 Faithless = 0;
	};

	/// Materialises the counts of a region into person entities. Returns the
	/// persons created; 0 when the region is unknown, already detailed,
	/// unsettled or above MaxPersonsPerRegion (nothing changes then).
	VAELEN_POPULATION_API uint32 PromoteRegion(World& W, const History::PreHistoryTypes& Types,
											   const PersonTypes& Persons, const MaterialiseRules& Rules, uint32 Region,
											   SimTick Now);
	/// Folds the living persons of a detailed region back into its counts
	/// (counts per culture and believers per faith become what the persons say)
	/// and destroys every person of the region, dead ones included. Returns the
	/// persons removed; 0 when the region is not detailed.
	VAELEN_POPULATION_API uint32 DemoteRegion(World& W, const History::PreHistoryTypes& Types,
											  const PersonTypes& Persons, uint32 Region);
	/// True while the region carries a RegionDetail.
	VAELEN_POPULATION_API bool IsDetailed(const World& W, const History::PreHistoryTypes& Types,
										  const PersonTypes& Persons, uint32 Region);
	/// The fine grain of a region counted.
	VAELEN_POPULATION_API RegionCensus CountPersons(const World& W, const PersonTypes& Persons, uint32 Region);
	/// True when the living persons of a detailed region agree with its coarse
	/// counts, culture by culture and faith by faith.
	VAELEN_POPULATION_API bool IsConsistent(const World& W, const History::PreHistoryTypes& Types,
											const PersonTypes& Persons, uint32 Region);

	struct DetailStats
	{
		uint32 DetailedRegions = 0;
		uint32 Persons = 0; ///< entities with a PersonInfo
		uint32 Alive = 0;
		uint32 Dead = 0;
		uint32 Inconsistent = 0;  ///< detailed regions whose persons disagree with the counts
		Hash64 PersonsDigest = 0; ///< FNV over every PersonInfo in index order
	};
	VAELEN_POPULATION_API DetailStats MeasureDetail(const World& W, const History::PreHistoryTypes& Types,
													const PersonTypes& Persons);
} // namespace Vaelen::Population
