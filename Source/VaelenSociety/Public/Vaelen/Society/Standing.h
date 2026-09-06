// VAELEN - VaelenSociety
// Phase 05.02: standing - the rank of a person from house, age, traits,
// skills and offices, and the elite of a region.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic tests in Tests/Society
//
// Standing is computed, never stored as truth: every year, in every detailed
// region, each living person gets a score from the size of its house and its
// headship, its age band, its charm and will, its best skill and the offices
// it holds (a seat, a head's seat), and the region's living adults are ranked
// by that score into three tiers - the elite (the top share), the notables
// (the next share) and the common. The score, rank and tier live in a small
// component so that later systems and the chronicle can read them; the rule
// table says what counts.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/BondState.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/SocietyApi.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Society
{
	enum class Tier : uint8
	{
		Common = 0,
		Notable = 1,
		Elite = 2,
	};

	/// Offices held, as bits.
	enum class Office : uint8
	{
		HeadOfHouse = 1,
		Seat = 2,		///< member of an organisation
		HeadOfSeat = 4, ///< head of an organisation
	};

	/// Component on a living person of a detailed region.
	struct PersonStanding
	{
		uint32 Score = 0;  ///< the sum the rules give
		uint8 Rank = 0;	   ///< 0..255 within the region's living adults, 255 the highest
		uint8 Tier_ = 0;   ///< Tier
		uint8 Offices = 0; ///< Office bits
		uint8 Reserved = 0;
	};
	static_assert(sizeof(PersonStanding) == 8, "PersonStanding must stay padding free");

	struct StandingTypes
	{
		ComponentType<PersonStanding> Standing;
		static StandingTypes Declare(World& W);
	};

	struct StandingRules
	{
		uint32 AdultFrom = 16;			 ///< ranked from this age
		uint32 HousePointsPerMember = 4; ///< per living member of the house (capped)
		uint32 HouseMembersCap = 40;
		uint32 HeadOfHousePoints = 60;
		uint32 SeatPoints = 80;					   ///< a seat in an organisation
		uint32 HeadOfSeatPoints = 120;			   ///< the head of one
		uint32 AgeBandPoints[4] = {0, 20, 40, 30}; ///< 16-30, 30-45, 45-60, 60+
		uint32 TraitPointsPerMille = 250;		   ///< of (charm + will) / 2
		uint32 SkillPointsPerMille = 300;		   ///< of the best skill
		uint32 ElitePerMille = 50;				   ///< top share of the ranked
		uint32 NotablePerMille = 150;			   ///< next share
	};

	/// The score of one person under the rules (pure).
	VAELEN_SOCIETY_API uint32 StandingScore(const Population::PersonInfo& P, const Population::PersonTraits* T,
											uint32 HouseMembers, uint8 Offices, uint64 Tick,
											const StandingRules& Rules) noexcept;

	/// Yearly, for every detailed region: scores, ranks and tiers of the living adults.
	class VAELEN_SOCIETY_API StandingSystem final : public ISystem
	{
	public:
		StandingSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					   Population::FamilyTypes InFamilies, Population::TraitTypes InTraits,
					   OrganizationTypes InOrganizations, StandingTypes InStanding, StandingRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Traits(InTraits),
			  Organizations(InOrganizations), Standing(InStanding), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Standing"; }
		/// Optional: the bound (05.04) are not ranked; bondage removes one from standing.
		void ObserveBonds(ComponentType<BondState> InBonds) noexcept
		{
			Bonds = InBonds;
			HasBonds = true;
		}
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Organizations"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		Population::TraitTypes Traits;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		StandingRules Rules;
		ComponentType<BondState> Bonds;
		bool HasBonds = false;
	};

	/// The elite of a region: living adults by rank descending (then index), at most MaxCount (0 = all of the elite
	/// tier).
	VAELEN_SOCIETY_API void EliteOf(const World& W, const Population::PersonTypes& Persons, const StandingTypes& Types,
									uint32 Region, std::vector<uint32>& Out, uint32 MaxCount = 0);
	/// The standing of a person (nullptr when unranked).
	VAELEN_SOCIETY_API const PersonStanding* StandingOf(const World& W, const Population::PersonTypes& Persons,
														const StandingTypes& Types, uint32 Person);

	struct StandingStats
	{
		uint32 Ranked = 0; ///< living adults with a standing (region-filtered)
		uint32 PerTier[3] = {};
		uint64 ScoreSum[3] = {};
		uint32 WithOffice = 0;
		uint32 Stale = 0;  ///< standings on the dead, the gone or the young
		Hash64 Digest = 0; ///< every standing in person index order
	};
	VAELEN_SOCIETY_API StandingStats MeasureStanding(const World& W, const Population::PersonTypes& Persons,
													 const StandingTypes& Types, uint32 Region);
} // namespace Vaelen::Society
