// VAELEN - VaelenPopulation
// Phase 04.05: traits, skills and names of persons.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population
//
// Every person carries six traits drawn from its identity and, for a child,
// pulled toward its parents; four skills that grow through youth and adult
// life under the traits, with a head start from the parents' trades, and fade
// in old age; and a name built in the person's language by the naming rules
// of Phase 03. Traits and names are given once and never change; skills move
// once a year in detailed regions.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/PopulationApi.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Population
{
	enum class Trait : uint32
	{
		Vigour = 0,
		Wit,
		Will,
		Charm,
		Boldness,
		Piety,
		Count
	};
	enum class Skill : uint32
	{
		Farming = 0,
		Craft,
		Fighting,
		Lore,
		Count
	};
	VAELEN_POPULATION_API const char* TraitName(Trait T) noexcept;
	VAELEN_POPULATION_API const char* SkillName(Skill S) noexcept;

	/// Component on a person entity.
	struct PersonTraits
	{
		uint8 Traits[static_cast<uint32>(Trait::Count)] = {}; ///< 0..255, 128 is ordinary
		uint8 Skills[static_cast<uint32>(Skill::Count)] = {}; ///< 0..255, 0 untrained
		uint8 Named = 0;									  ///< 1 once a name was given (or refused)
		uint8 Reserved = 0;
		uint32 Reserved2 = 0;
	};
	static_assert(sizeof(PersonTraits) == 16, "PersonTraits must stay padding free");

	struct TraitTypes
	{
		ComponentType<PersonTraits> Traits;
		static TraitTypes Declare(World& W);
	};

	struct TraitRules
	{
		uint32 HeritabilityPerMille = 500; ///< share of the parents' mean in a child's trait
		uint32 SkillFrom = 8;			   ///< skills grow from this age
		uint32 SkillTo = 45;			   ///< and stop growing at this age
		uint32 SkillGrowth = 6;			   ///< yearly draw below this, scaled by the trait behind the skill
		uint32 ApprenticeTo = 15;		   ///< until this age a parent's skill adds to the growth
		uint32 DeclineFrom = 60;		   ///< skills lose one point a year from this age
		uint32 NamePersons = 1;			   ///< give every person a name in its language
	};

	/// Traits drawn from an identity alone (a materialised person).
	VAELEN_POPULATION_API PersonTraits TraitsFromIdentity(Hash64 Identity) noexcept;
	/// Traits of a child: its own draw pulled toward the mean of its parents.
	VAELEN_POPULATION_API PersonTraits TraitsFromParents(Hash64 Identity, const PersonTraits& Mother,
														 const PersonTraits& Father, const TraitRules& Rules) noexcept;
	/// The trait that drives a skill's growth.
	VAELEN_POPULATION_API Trait TraitBehind(Skill S) noexcept;

	/// Yearly: traits and a name for every person that lacks them, then a
	/// year of skill for the living of the detailed regions.
	class VAELEN_POPULATION_API TraitSystem final : public ISystem
	{
	public:
		TraitSystem(World& InWorld, const History::PreHistoryTypes& InTypes, PersonTypes InPersons, TraitTypes InTraits,
					TraitRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Traits(InTraits), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Traits"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Lives"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		PersonTypes Persons;
		TraitTypes Traits;
		TraitRules Rules;
	};

	/// Name of a person (empty text when unnamed or unknown).
	VAELEN_POPULATION_API History::NameText PersonName(const World& W, const History::LanguageTypes& Languages,
													   const PersonTypes& Persons, uint32 Person);

	struct TraitStats
	{
		uint32 WithTraits = 0; ///< living persons with traits (region-filtered)
		uint32 Named = 0;
		uint32 Unnamed = 0;
		uint64 TraitSum[static_cast<uint32>(Trait::Count)] = {};
		uint64 SkillSum[static_cast<uint32>(Skill::Count)] = {};
		uint8 TraitMin = 255;
		uint8 TraitMax = 0;
		uint8 SkillMax = 0;
		Hash64 Digest = 0; ///< traits and skills of every person, in index order
	};
	VAELEN_POPULATION_API TraitStats MeasureTraits(const World& W, const History::PreHistoryTypes& Types,
												   const PersonTypes& Persons, const TraitTypes& Traits, uint32 Region);
} // namespace Vaelen::Population
