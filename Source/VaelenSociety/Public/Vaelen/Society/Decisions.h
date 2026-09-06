// VAELEN - VaelenSociety
// Phase 05.05: organisations acting - what a council, a temple, a guild and a
// warband decide every year, with events and causes.
//
// STATUS: VALIDATED (Phase 05) - unit/integration tests in Tests/Society
//
// A council lays in grain after a drought, and the region's stores soften
// the next drought's cut (the need system observes them). A temple preaches
// and converts a share of the region's people of other faiths - persons
// first, so the counts follow. A guild trains its members in their craft.
// A warband plans a raid on the most peopled neighbour every few years,
// published as an event for the wars of Phase 08. Every decision is an
// event about the organisation, with the drought as cause where one is.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/System.h"
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
	enum class DecisionKind : uint32
	{
		StoreGrain = 0, ///< council
		Preach,			///< temple
		Train,			///< guild
		Raid,			///< warband
		Count
	};
	VAELEN_SOCIETY_API const char* DecisionKindName(DecisionKind K) noexcept;

	struct DecisionTypes
	{
		ComponentType<Population::RegionStores> Stores; ///< the need system observes it
		static DecisionTypes Declare(World& W);
	};

	struct DecisionRules
	{
		uint32 StoreAfterDroughtYears = 3; ///< a council keeps grain this long after a drought struck
		uint32 StorePerMille = 400;		   ///< share of a drought's cut the stores absorb
		uint32 PreachPerMille = 20;		   ///< of the region's people of other faiths converted a year
		uint32 PreachMax = 64;			   ///< persons converted a year at most
		uint32 TrainGain = 6;			   ///< craft points a year for a guild's members
		uint32 RaidEveryYears = 5;		   ///< a warband plans a raid this often
		uint32 RaidStrengthPerMember = 10;
	};

	struct DecisionPayload
	{
		uint32 Organization = 0;
		uint32 Kind = 0;   ///< DecisionKind
		uint32 Region = 0; ///< the seat
		uint32 Value = 0;  ///< grain per mille, persons converted, members trained, raid strength
	};
	struct RaidPayload
	{
		uint32 Organization = 0;
		uint32 Region = 0; ///< from
		uint32 Target = 0; ///< the neighbour
		uint32 Strength = 0;
	};
	inline constexpr EventType<DecisionPayload> DecisionMadeEvent = MakeEventType<DecisionPayload>("DecisionMade");
	inline constexpr EventType<RaidPayload> RaidPlannedEvent = MakeEventType<RaidPayload>("RaidPlanned");

	/// Yearly, for every living organisation seated in a detailed region.
	class VAELEN_SOCIETY_API DecisionSystem final : public ISystem
	{
	public:
		DecisionSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					   Population::TraitTypes InTraits, OrganizationTypes InOrganizations, DecisionTypes InDecisions,
					   DecisionRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Traits(InTraits), Organizations(InOrganizations),
			  Decisions(InDecisions), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Decisions"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Organizations"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::TraitTypes Traits;
		OrganizationTypes Organizations;
		DecisionTypes Decisions;
		DecisionRules Rules;
		Hash64 GraphDigest = 0;		 ///< derived cache, not state
		WorldGen::RegionGraph Graph; ///< derived cache, not state
	};

	/// The stores of a region (nullptr when none).
	VAELEN_SOCIETY_API const Population::RegionStores* StoresOf(const World& W, const History::PreHistoryTypes& Types,
																const DecisionTypes& Decisions, uint32 Region);

	struct DecisionStats
	{
		uint32 Decisions = 0; ///< from the log
		uint32 PerKind[static_cast<uint32>(DecisionKind::Count)] = {};
		uint32 Caused = 0; ///< decisions with a cause id
		uint32 Raids = 0;
		uint32 Converted = 0; ///< persons converted by preaching, from the log
		uint32 RegionsWithGrain = 0;
		Hash64 Digest = 0; ///< every region's stores in region order
	};
	VAELEN_SOCIETY_API DecisionStats MeasureDecisions(const World& W, const History::PreHistoryTypes& Types,
													  const DecisionTypes& Decisions);
} // namespace Vaelen::Society
