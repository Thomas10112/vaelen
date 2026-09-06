// VAELEN - VaelenSociety
// Phase 05.04: bondage and slavery as institutions - states on persons,
// entries by debt and birth, exits by manumission, flight, the holder's
// death and death, shares kept per region.
//
// STATUS: VALIDATED (Phase 05) - unit/integration/edge tests in Tests/Society
//
// Where a culture allows it, a common adult may fall into bondage for debt,
// held by one of the region's elite; bondage unredeemed for years hardens
// into slavery, and the child of an enslaved mother is born enslaved where
// the culture allows birth bondage. The bonded and the enslaved are freed by
// manumission, by flight, the bonded by their holder's death, all by their
// own death. Every entry and exit is an event about the person, with the
// birth or the holder's death as its cause where one exists. Each region
// keeps its strata as counts, written from the persons while it is detailed
// and kept while it is coarse.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/BondState.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/SocietyApi.h"
#include "Vaelen/Society/Standing.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Society
{
	VAELEN_SOCIETY_API const char* BondKindName(BondKind K) noexcept;
	VAELEN_SOCIETY_API const char* BondEntryName(BondEntry E) noexcept;
	VAELEN_SOCIETY_API const char* BondExitName(BondExit E) noexcept;

	/// Component on a region entity: its strata as counts.
	struct RegionStrata
	{
		uint32 Free = 0;
		uint32 Bonded = 0;
		uint32 Enslaved = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(RegionStrata) == 16, "RegionStrata must stay padding free");

	struct BondageTypes
	{
		ComponentType<BondState> Bond;
		ComponentType<RegionStrata> Strata;
		static BondageTypes Declare(World& W);
	};

	struct BondageRules
	{
		uint32 DebtPerMille = 15; ///< yearly chance for a common adult of a culture allowing debt bondage
		uint32 DebtFromAge = 16;
		uint32 HardenAfterYears = 15;	 ///< bondage unredeemed this long becomes slavery
		uint32 ManumissionPerMille = 25; ///< yearly chance of freedom for the bonded and the enslaved
		uint32 FlightPerMille = 8;		 ///< yearly chance of flight
		uint32 BirthFollowsMother = 1;	 ///< the child of an enslaved mother is enslaved (where allowed)
		uint32 MaxHeldPerHolder = 12;	 ///< a holder takes no more
	};

	struct BondPayload
	{
		uint32 Person = 0;
		uint32 Region = 0;
		uint32 Kind = 0;   ///< BondKind
		uint32 Reason = 0; ///< BondEntry or BondExit
	};
	inline constexpr EventType<BondPayload> BondEnteredEvent = MakeEventType<BondPayload>("BondEntered");
	inline constexpr EventType<BondPayload> BondLeftEvent = MakeEventType<BondPayload>("BondLeft");

	/// Yearly, for every detailed region: exits, hardenings, entries, then the strata.
	class VAELEN_SOCIETY_API BondageSystem final : public ISystem
	{
	public:
		BondageSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					  NormTypes InNorms, StandingTypes InStanding, BondageTypes InBondage,
					  BondageRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Norms(InNorms), Standing(InStanding),
			  Bonds(InBondage), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Bondage"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Standing", "Norms"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// Runs after another yearly system too (Lod), so that a promotion's
		/// persons are bound in the same tick. The system must exist.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		NormTypes Norms;
		StandingTypes Standing;
		BondageTypes Bonds;
		BondageRules Rules;
	};

	/// The bond of a person (nullptr when free or unknown).
	VAELEN_SOCIETY_API const BondState* BondOf(const World& W, const Population::PersonTypes& Persons,
											   const BondageTypes& Types, uint32 Person);
	/// The strata of a region (nullptr when never written).
	VAELEN_SOCIETY_API const RegionStrata* StrataOf(const World& W, const History::PreHistoryTypes& Types,
													const BondageTypes& Types_, uint32 Region);

	struct BondageStats
	{
		uint32 Bonded = 0; ///< living persons (region-filtered)
		uint32 Enslaved = 0;
		uint32 Stale = 0;		///< bonds on the dead or the gone
		uint32 HolderLost = 0;	///< bonds whose holder is dead, gone or not of the region
		uint32 Entered[5] = {}; ///< by BondEntry, from the log
		uint32 Left[6] = {};	///< by BondExit, from the log
		uint32 Caused = 0;		///< entries and exits with a cause id
		Hash64 Digest = 0;		///< every bond in person index order, then every strata in region order
	};
	VAELEN_SOCIETY_API BondageStats MeasureBondage(const World& W, const History::PreHistoryTypes& Types,
												   const Population::PersonTypes& Persons, const BondageTypes& Types_,
												   uint32 Region);
} // namespace Vaelen::Society
