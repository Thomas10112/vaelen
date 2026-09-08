// VAELEN - VaelenInfrastructure
// Phase 09.01: buildings - the things people raise out of goods and labour,
// what they cost, and how sound they still are.
//
// STATUS: PROTOTYPE (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure
//
// A building is an entity of kind Building standing in a region. It is raised
// out of what the region holds IN COMMON, never out of a house's own goods: a
// granary is not one family's. So a region builds when its people are many
// enough to want the thing, when the common stock holds the timber, the tools
// and the grain the builders eat, and when there are hands to spare from the
// fields - and a detailed region builds out of what a council put by (05.05),
// which is the same rule read from the other end.
//
// A region keeps at most one work of each kind and makes it bigger as it grows,
// so a granary of three is one building the region enlarged twice, not three
// buildings. Nothing here does anything yet: 09.02 gives every kind its effect
// on the layers below, and 09.05 lets what is not kept fall down. This task
// only makes the thing exist, cost what it costs, and be counted.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Infrastructure/InfrastructureApi.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Families.h"
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

namespace Vaelen::Infrastructure
{
	/// Kinds of work a region raises. Kinds are a table, never entities - the
	/// building is the entity.
	enum class Work : uint8
	{
		Granary = 0, ///< holds grain against a bad year (09.02)
		Mill,		 ///< lifts what the fields give (09.02)
		Smithy,		 ///< lifts what the craft gives (09.02)
		Wall,		 ///< raises what a siege must break (09.02, over 08.04)
		Count
	};
	inline constexpr uint32 WorkCount = static_cast<uint32>(Work::Count);
	VAELEN_INFRASTRUCTURE_API const char* WorkName(Work W) noexcept;

	/// Component of a building entity (ids of kind Building).
	struct BuildingInfo
	{
		uint32 Index = 0;	 ///< 1-based, in order of raising
		uint32 Kind = 0;	 ///< Work
		uint32 Region = 0;	 ///< where it stands
		uint32 Polity = 0;	 ///< who ruled that ground when it went up, 0 = nobody did
		uint32 Size = 0;	 ///< how much of it there is; a granary of 3 holds three times
		uint32 Repair = 0;	 ///< per mille, 1000 = sound; 09.05 lets it fall
		uint32 Timber = 0;	 ///< units of timber it has taken, over every enlargement
		uint32 Tools = 0;	 ///< units of tools
		uint32 Grain = 0;	 ///< grain the builders ate
		uint32 Hands = 0;	 ///< man-years of labour it has taken
		uint64 Raised = 0;	 ///< tick
		uint64 Fell = 0;	 ///< tick it stopped standing, 0 while it stands
		Hash64 Identity = 0; ///< from the world seed, for names
	};
	static_assert(sizeof(BuildingInfo) == 64, "BuildingInfo must stay padding free");

	/// Component on a region entity: what stands on it. A summary, so that the
	/// layers below can ask "how big is the granary here" without walking every
	/// building in the world; the buildings themselves stay the truth and
	/// MeasureBuildings checks the two agree.
	struct RegionWorks
	{
		uint32 Standing = 0; ///< buildings still up
		uint32 Ruins = 0;	 ///< buildings that fell and are still on the ground
		uint32 Kept[8] = {}; ///< [WorkCount] summed size still sound, by kind (size x repair)
	};
	static_assert(sizeof(RegionWorks) == 40, "RegionWorks must stay padding free");

	struct InfrastructureTypes
	{
		ComponentType<BuildingInfo> Building;
		ComponentType<RegionWorks> Works;
		static VAELEN_INFRASTRUCTURE_API InfrastructureTypes Declare(World& W);
	};

	struct BuildingRules
	{
		uint32 PeopleToBuild = 200; ///< under this many living, a region raises nothing
		uint32 PeoplePerSize = 500; ///< a region wants one more of a thing per this many people
		uint32 MostOfAKind = 4;		///< and never more than this
		uint32 TimberPerSize = 40;	///< what one more of a thing costs, out of the common stock
		uint32 ToolsPerSize = 8;
		uint32 GrainPerSize = 30;		  ///< what the builders eat while they raise it
		uint32 HandsPerSize = 20;		  ///< man-years; the region must have them to spare
		uint32 WorkerSharePerMille = 700; ///< of the people, who work at all (the reading of 06.02)
		uint32 SparePerMille = 100;		  ///< of the workers, who can leave the fields for a year
		uint32 WallsAtSeatsOnly = 1;	  ///< a wall is raised where a polity sits, nowhere else
		uint32 RaisedPerRegionPerYear = 1;
	};

	struct WorksPayload
	{
		uint32 Region = 0;
		uint32 Building = 0; ///< building index
		uint32 Kind = 0;	 ///< Work
		uint32 Amount = 0;	 ///< the size it now has
	};
	/// A region raised something it did not have.
	inline constexpr EventType<WorksPayload> BuildingRaisedEvent = MakeEventType<WorksPayload>("BuildingRaised");
	/// A region made something it already had bigger.
	inline constexpr EventType<WorksPayload> BuildingEnlargedEvent = MakeEventType<WorksPayload>("BuildingEnlarged");

	/// Yearly, after Production: what a region wants, what it can pay for, and
	/// the one thing a year it raises or enlarges.
	class VAELEN_INFRASTRUCTURE_API BuildingSystem final : public ISystem
	{
	public:
		BuildingSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::FamilyTypes InFamilies,
					   Economy::EconomyTypes InEconomy, Politics::PolityTypes InPolities, InfrastructureTypes InWorks,
					   BuildingRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Families(InFamilies), Economy(InEconomy), Polities(InPolities),
			  Works(InWorks), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Buildings"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Production"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// Runs after another yearly system too. The system must exist.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::FamilyTypes Families;
		Economy::EconomyTypes Economy;
		Politics::PolityTypes Polities;
		InfrastructureTypes Works;
		BuildingRules Rules;
	};

	/// The building of that index (nullptr for an unknown one).
	VAELEN_INFRASTRUCTURE_API const BuildingInfo* BuildingOf(const World& W, const InfrastructureTypes& Works,
															 uint32 Building);
	/// Every building of a region, standing or fallen, in index order.
	VAELEN_INFRASTRUCTURE_API void BuildingsIn(const World& W, const InfrastructureTypes& Works, uint32 Region,
											   std::vector<uint32>& Out);
	/// What stands on a region (nullptr before it ever built anything).
	VAELEN_INFRASTRUCTURE_API const RegionWorks* WorksOf(const World& W, const History::PreHistoryTypes& Types,
														 const InfrastructureTypes& Works, uint32 Region);
	/// The sound size of one kind of work in a region - the one number every
	/// effect of 09.02 is a function of. 0 where nothing of the kind stands.
	VAELEN_INFRASTRUCTURE_API uint32 KeptSize(const World& W, const History::PreHistoryTypes& Types,
											  const InfrastructureTypes& Works, uint32 Region, Work Kind);

	struct BuildingStats
	{
		uint32 Standing = 0;		 ///< buildings still up
		uint32 Ruined = 0;			 ///< buildings that fell
		uint32 Regions = 0;			 ///< regions with anything standing
		uint32 Of[WorkCount] = {};	 ///< standing buildings, by kind
		uint32 Size[WorkCount] = {}; ///< summed size standing, by kind
		uint32 Timber = 0;			 ///< what all of it cost, standing and fallen
		uint32 Tools = 0;
		uint32 Grain = 0;
		uint32 Hands = 0;
		uint32 Raised = 0;	 ///< events, from the log
		uint32 Enlarged = 0; ///< events
		uint32 Bad = 0;		 ///< incoherent: see MeasureBuildings
		Hash64 Digest = 0;	 ///< every building, in index order
	};

	/// Counts every building and checks the invariants a building must keep:
	/// an index used once, a known region, a known kind, a standing building
	/// with a size and a repair no better than sound, one work of a kind per
	/// region, and a RegionWorks that agrees with the buildings on it.
	VAELEN_INFRASTRUCTURE_API BuildingStats MeasureBuildings(const World& W, const History::PreHistoryTypes& Types,
															 const InfrastructureTypes& Works,
															 const BuildingRules& Rules);
} // namespace Vaelen::Infrastructure
