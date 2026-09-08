// VAELEN - VaelenEconomy
// Phase 06.02: production and consumption - the yearly harvest from the land,
// the people and their farming, cut by the droughts; grain eaten by the
// people, spoiled in store; timber, ore and salt from the deposits; cloth and
// tools from the people's craft; the ration the need system observes.
//
// STATUS: VALIDATED (Phase 06) - unit/integration/deterministic tests in Tests/Economy
//
// Every year, for every region holding a stock: the workers (a share of the
// people on the land within the capacity, or the persons of age of a detailed
// region by their farming) harvest grain, cut by this year's drought; a
// detailed region's harvest goes to the workers' houses (a share to the common
// stock where a council keeps grain), a coarse region's to the common stock;
// the grain in store spoils a little; every person eats, a house from its own
// stock then from the common one; what the region could not feed becomes its
// ration, written for the need system (04.04) so that hunger follows the grain.
// The deposits yield timber, ore and salt to the common stock, the people burn
// timber and use salt, their craft makes cloth and tools (tools from ore), and
// cloth and tools wear. Luxuries are neither made nor used yet (06.04 trade).
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyApi.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
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

namespace Vaelen::Economy
{
	/// What the works of a region add to what it makes, on the region entity:
	/// written by a later module (Phase 09 infrastructure), read by the
	/// production system when told to observe the type. A mill does not make
	/// grain; it means the same fields and the same hands give more of it.
	struct RegionWorkshops
	{
		uint32 FieldsPerMille = 0; ///< added to the harvest
		uint32 CraftPerMille = 0;  ///< added to the cloth and the tools
	};
	static_assert(sizeof(RegionWorkshops) == 8, "RegionWorkshops must stay padding free");

	struct ProductionTypes
	{
		ComponentType<Population::RegionRation> Ration; ///< the need system observes it
		static VAELEN_ECONOMY_API ProductionTypes Declare(World& W);
	};

	struct ProductionRules
	{
		uint32 GrainPerPerson = 4; ///< a person's yearly need
		uint32 HarvestPerWorker =
			7; ///< a worker's yearly harvest at ordinary farming (seven tenths work: a fifth of surplus)
		uint32 WorkerSharePerMille = 700;  ///< of the people on the land, in a coarse region
		uint32 WorkerFromAge = 12;		   ///< a person of a detailed region works from this age
		uint32 FarmingFloorPerMille = 850; ///< a worker's yield: floor plus span by farming skill
		uint32 FarmingSpanPerMille = 300;
		uint32 GrainSpoilPerMille = 100;				///< of the grain in every stock, yearly
		uint32 DroughtCutPerMille[3] = {300, 600, 900}; ///< harvest lost by severity 1..3
		uint32 ExtractPerMille = 50;					///< of a deposit's richness a year (timber, ore, salt)
		uint32 ExtractFromPeople = 20;					///< a region extracts once this many live there
		uint32 TimberPerPersons = 10;					///< one timber a year burnt per this many people
		uint32 SaltPerPersons = 50;
		uint32 ClothPerPersons = 20; ///< one cloth a year made per this many people, by their craft
		uint32 ClothWearPerPersons = 25;
		uint32 ToolsPerPersons = 40; ///< one tool a year made per this many people, from one ore
		uint32 ToolsWearPerPersons = 50;
		uint32 CraftFrom = 32;			 ///< a crafter's skill line, in a detailed region
		uint32 CraftFloorPerMille = 850; ///< craft output: floor plus span by the crafters' mean skill
		uint32 CraftSpanPerMille = 300;
	};

	/// A region's harvest (Region, 0, Grain, units), with the drought as cause when it cut.
	inline constexpr EventType<StockPayload> HarvestEvent = MakeEventType<StockPayload>("Harvest");
	/// What a region could not feed (Region, 0, Grain, units short).
	inline constexpr EventType<StockPayload> ShortfallEvent = MakeEventType<StockPayload>("Shortfall");

	/// Yearly, after Stocks: harvest, spoilage, meals, the ration, the other goods.
	class VAELEN_ECONOMY_API ProductionSystem final : public ISystem
	{
	public:
		ProductionSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
						 Population::FamilyTypes InFamilies, EconomyTypes InEconomy, ProductionTypes InProduction,
						 ProductionRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Economy(InEconomy),
			  Production(InProduction), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Production"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Stocks"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: the persons' farming and craft skills (04.05) shape a detailed region's output.
		void ObserveTraits(ComponentType<Population::PersonTraits> InTraits) noexcept
		{
			Traits = InTraits;
			HasTraits = true;
		}
		/// Optional: where a council keeps grain (05.05), that share of a detailed
		/// region's harvest goes to the common stock instead of the houses.
		void ObserveStores(ComponentType<Population::RegionStores> InStores) noexcept
		{
			Stores = InStores;
			HasStores = true;
		}
		/// Optional: what a region has built (09.02). A mill lifts the harvest and
		/// a smithy the craft, by what stands and how sound it is.
		void ObserveWorkshops(ComponentType<RegionWorkshops> InShops) noexcept
		{
			Shops = InShops;
			HasShops = true;
		}
		/// Optional: what a region owes away (07.02). The share is assessed on the
		/// year's harvest, the grain stays in the stock until a collector takes it.
		void ObserveDues(ComponentType<RegionDues> InDues) noexcept
		{
			Dues = InDues;
			HasDues = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		EconomyTypes Economy;
		ProductionTypes Production;
		ProductionRules Rules;
		ComponentType<Population::PersonTraits> Traits;
		bool HasTraits = false;
		ComponentType<Population::RegionStores> Stores;
		bool HasStores = false;
		ComponentType<RegionDues> Dues;
		bool HasDues = false;
		ComponentType<RegionWorkshops> Shops;
		bool HasShops = false;
	};

	/// The ration of a region (nullptr before the first harvest or for an unknown region).
	VAELEN_ECONOMY_API const Population::RegionRation* RationOf(const World& W, const History::PreHistoryTypes& Types,
																const ProductionTypes& Production, uint32 Region);

	struct ProductionStats
	{
		uint32 Harvests = 0;	 ///< harvest events, region-filtered
		uint64 Grain = 0;		 ///< units harvested
		uint32 Cut = 0;			 ///< harvests with a drought as cause
		uint32 Shortfalls = 0;	 ///< shortfall events
		uint64 Short = 0;		 ///< units short
		uint32 Rationed = 0;	 ///< regions with a ration under a full one
		uint32 RationMin = 1000; ///< the lowest ration
		Hash64 Digest = 0;		 ///< every ration in region order
	};
	VAELEN_ECONOMY_API ProductionStats MeasureProduction(const World& W, const History::PreHistoryTypes& Types,
														 const ProductionTypes& Production, uint32 Region);
} // namespace Vaelen::Economy
