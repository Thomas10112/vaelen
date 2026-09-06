// VAELEN - VaelenPopulation
// Phase 04.03: families and lineage.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population
//
// Persons marry inside their region and culture (and faith, when the rules say
// so), a married man without a family founds one and his wife joins it, and
// children are born into their mother's family with her husband as father.
// Every family is an entity (ids of kind Family) that keeps its head, its
// home and the tick it went extinct. Lineage is read from the persons
// themselves: ancestors, descendants, siblings and kinship are pure queries
// over the mother and father links. Every marriage and founding is an event.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	/// Component of a family entity (ids of kind Family).
	struct FamilyInfo
	{
		uint32 Index = 0;	   ///< 1-based, in order of founding
		uint32 Culture = 0;	   ///< culture of the founder
		uint32 Region = 0;	   ///< home region (where it was founded)
		uint32 Head = 0;	   ///< person index of the head (0 once extinct)
		uint32 Founder = 0;	   ///< person index of the founder
		uint32 Generation = 0; ///< generations since the founder among the living
		uint64 Founded = 0;	   ///< tick
		uint64 Extinct = 0;	   ///< tick when the last member died, 0 while alive
		Hash64 Identity = 0;
	};
	static_assert(sizeof(FamilyInfo) == 48, "FamilyInfo must stay padding free");

	struct FamilyTypes
	{
		ComponentType<FamilyInfo> Family;
		static FamilyTypes Declare(World& W);
	};

	struct FamilyRules
	{
		uint32 MarryFrom = 18;			///< youngest bride or groom (years)
		uint32 MarryTo = 50;			///< nobody marries from this age
		uint32 MarriagesPerMille = 350; ///< yearly chance for an unmarried adult to seek a spouse
		uint32 MaxAgeGap = 15;			///< years between spouses
		uint32 FaithMatters = 1;		///< spouses share a faith (or both have none)
		uint32 FoundOnMarriage = 1;		///< a married man without a family founds one
	};

	/// Marriage norms of a culture, on the culture entity: written by a later
	/// module (Phase 05 norms), read by the family system when told to observe
	/// the type. Cultures without the component follow the FamilyRules.
	struct MarriageNorms
	{
		uint32 MarryFrom = 18;
		uint32 MarryTo = 50;
		uint32 MaxAgeGap = 15;
		uint32 FaithMatters = 1;
		uint32 MarriagesPerMille = 350;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(MarriageNorms) == 24, "MarriageNorms must stay padding free");

	struct MarriagePayload
	{
		uint32 Person = 0; ///< the groom
		uint32 Spouse = 0; ///< the bride
		uint32 Region = 0;
		uint32 Family = 0; ///< family the couple lives in (0 until founded)
	};
	struct FamilyPayload
	{
		uint32 Family = 0;
		uint32 Region = 0;
		uint32 Head = 0;
		uint32 Culture = 0;
	};
	inline constexpr EventType<MarriagePayload> PersonMarriedEvent = MakeEventType<MarriagePayload>("PersonMarried");
	inline constexpr EventType<FamilyPayload> FamilyFoundedEvent = MakeEventType<FamilyPayload>("FamilyFounded");
	inline constexpr EventType<FamilyPayload> FamilyExtinctEvent = MakeEventType<FamilyPayload>("FamilyExtinct");

	/// Yearly, for every detailed region: widows and widowers are released,
	/// unmarried adults marry inside the region, culture and faith, married men
	/// found families and wives join them, heads are replaced when they die
	/// and families without a living member go extinct.
	class VAELEN_POPULATION_API FamilySystem final : public ISystem
	{
	public:
		FamilySystem(World& InWorld, const History::PreHistoryTypes& InTypes, PersonTypes InPersons,
					 FamilyTypes InFamilies, FamilyRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Families"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Lives"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// Runs after another yearly system too (Needs, Lod), so that the heads
		/// it replaces and the spouses it releases include that system's deaths
		/// and departures of the same tick. The system must exist in the world.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: marriages follow the MarriageNorms of the groom's culture
		/// where the culture entity carries one (Phase 05 norms).
		void ObserveNorms(ComponentType<MarriageNorms> InNorms) noexcept
		{
			Norms = InNorms;
			HasNorms = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		FamilyTypes Families;
		FamilyRules Rules;
		ComponentType<MarriageNorms> Norms;
		bool HasNorms = false;
	};

	// ── Lineage queries (pure) ────────────────────────────────────────────────

	/// The person with an index, or null.
	VAELEN_POPULATION_API const PersonInfo* FindPerson(const World& W, const PersonTypes& Persons, uint32 Index);
	/// Ancestors up to Depth generations (parents first), each once, in
	/// index order within a generation.
	VAELEN_POPULATION_API void Ancestors(const World& W, const PersonTypes& Persons, uint32 Person, uint32 Depth,
										 std::vector<uint32>& Out);
	/// Descendants up to Depth generations (children first).
	VAELEN_POPULATION_API void Descendants(const World& W, const PersonTypes& Persons, uint32 Person, uint32 Depth,
										   std::vector<uint32>& Out);
	/// Persons sharing at least one known parent, in index order.
	VAELEN_POPULATION_API void Siblings(const World& W, const PersonTypes& Persons, uint32 Person,
										std::vector<uint32>& Out);
	/// True when the two share an ancestor within Depth generations (or one
	/// descends from the other).
	VAELEN_POPULATION_API bool AreKin(const World& W, const PersonTypes& Persons, uint32 A, uint32 B, uint32 Depth);
	/// Living members of a family, in index order.
	VAELEN_POPULATION_API void FamilyMembers(const World& W, const PersonTypes& Persons, uint32 Family,
											 std::vector<uint32>& Out);

	struct FamilyStats
	{
		uint32 Families = 0;
		uint32 Extinct = 0;
		uint32 Married = 0;	  ///< living persons with a spouse
		uint32 Adults = 0;	  ///< living persons of marrying age
		uint32 InAFamily = 0; ///< living persons with a family
		uint32 Largest = 0;	  ///< living members of the largest family
		uint32 Broken = 0;	  ///< spouse links that do not point back, or to the dead
	};
	VAELEN_POPULATION_API FamilyStats MeasureFamilies(const World& W, const PersonTypes& Persons,
													  const FamilyTypes& Families, const FamilyRules& Rules,
													  uint64 Tick);
} // namespace Vaelen::Population
