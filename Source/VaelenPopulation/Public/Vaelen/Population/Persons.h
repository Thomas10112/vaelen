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
		Gone = 2, ///< left the detailed grain for a coarse region (04.06); kept for history
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
		uint8 Reserved[2] = {};
		uint32 Spouse = 0; ///< person index of the living spouse, 0 = unmarried or widowed
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

	/// Singleton component: the next person index to hand out, and the only place
	/// one is ever taken from.
	///
	/// A person index must never be handed out twice in the life of a world,
	/// because things outside the population remember one: a council's head
	/// (05.01), a polity's ruler (07.01), a line's claimant (07.04), a faction's
	/// (07.05). Allocating one past the highest person alive looks equivalent and
	/// is not: demoting a region destroys every person in it (Population LOD),
	/// which lowers that highest, and the next promotion or birth hands the same
	/// indices out again - to strangers, who inherit every claim the dead had. The
	/// counter only ever goes up, and it is carried in a snapshot like any other
	/// state.
	struct PersonCounter
	{
		uint32 Next = 1; ///< the next index to hand out; 0 is never a person
		uint32 Reserved = 0;
	};
	static_assert(sizeof(PersonCounter) == 8, "PersonCounter must stay padding free");

	struct PersonTypes
	{
		ComponentType<PersonInfo> Person;
		ComponentType<RegionDetail> Detail;
		ComponentType<PersonCounter> Counter;  ///< one per world, made when the first person is
		ComponentType<History::RegionLod> Lod; ///< the marker the coarse systems observe
		/// Declares the three types. The coarse systems of a PreHistory learn the
		/// marker through Attach; without it they keep moving detailed regions.
		static VAELEN_POPULATION_API PersonTypes Declare(World& W);
		/// Declare and Attach in one call.
		static VAELEN_POPULATION_API PersonTypes Declare(World& W, History::PreHistory& Ages);
		/// Tells the population, migration and disaster systems of the
		/// pre-history to leave detailed regions alone.
		void Attach(History::PreHistory& Ages) const noexcept;
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
	/// Takes Count consecutive person indices and returns the first of them,
	/// making the world's counter on the first call. Every person ever made takes
	/// its index from here; see PersonCounter for why nothing else may.
	VAELEN_POPULATION_API uint32 TakePersonIndices(World& W, const PersonTypes& Persons, uint32 Count);

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
	/// What an audit of the two grains found. Phase 15 task 15.04.
	///
	/// RECOMPUTED, never accumulated: every field below is counted from the
	/// world as it stands when Audit is called. A counter that is incremented
	/// as things happen agrees with itself by construction and is therefore no
	/// instrument at all - it would have counted the crossing that lost a
	/// believer as a crossing that went fine.
	///
	/// It declares no component and writes nothing, so calling it moves no
	/// digest and a test may call it on any world of any closed phase.
	struct AuditReport
	{
		uint32 Regions = 0;	 ///< regions carrying a population count
		uint32 Detailed = 0; ///< of those, simulated person by person
		/// Detailed regions whose living persons do not agree with their coarse
		/// counts (IsConsistent, region by region), and the first of them.
		uint32 Disagreeing = 0;
		uint32 FirstDisagreeing = 0;
		uint64 CoarseHeads = 0; ///< sum of RegionPopulation::Total over the world
		uint64 FineHeads = 0;	///< living persons counted one by one
		/// Regions whose culture slots do not sum to Total. Recount() exists to
		/// make this 0 and nothing else should ever make it otherwise.
		uint32 SlotSumWrong = 0;
		/// Regions where the believers outnumber the people. Believers FEWER
		/// than people is normal - somebody may hold no religion - so the
		/// reverse is the only direction that is always a fault.
		uint32 FaithsOverHeads = 0;
		/// Regions with every slot taken. Not faults: the PRECONDITION of the
		/// defect 15.05 fixes, because RegionFaith::Add refuses a fifth faith
		/// and the crossing that calls it ignores the refusal. A world where
		/// this is 0 cannot lose a believer that way, and a world where it
		/// climbs is a world where the fix matters more.
		uint32 FaithCeiling = 0;
		uint32 CultureCeiling = 0;
	};
	/// Counts both grains of the whole world and compares them where both
	/// exist. See AuditReport for what each number means and, as importantly,
	/// what it does not mean.
	///
	/// WHAT IT CANNOT SEE, said plainly because an instrument that is trusted
	/// past its range is worse than none: a believer lost into a COARSE region
	/// leaves no trace to recompute from. There are no persons there to count,
	/// so Total - sum(Adherents) cannot be told apart from the irreligious. The
	/// loss is observable at the moment it happens and not afterwards, which is
	/// why 15.05 is a fix in the crossing and not a second audit.
	VAELEN_POPULATION_API AuditReport Audit(const World& W, const History::PreHistoryTypes& Types,
											const PersonTypes& Persons);

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
		uint32 Gone = 0;		  ///< left for a coarse region
		uint32 Inconsistent = 0;  ///< detailed regions whose persons disagree with the counts
		Hash64 PersonsDigest = 0; ///< FNV over every PersonInfo in index order
	};
	VAELEN_POPULATION_API DetailStats MeasureDetail(const World& W, const History::PreHistoryTypes& Types,
													const PersonTypes& Persons);
} // namespace Vaelen::Population
