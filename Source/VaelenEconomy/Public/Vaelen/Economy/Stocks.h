// VAELEN - VaelenEconomy
// Phase 06.01: goods and stocks - kinds of goods, what a region holds in
// common and what a house holds, conserved across the grains.
//
// STATUS: VALIDATED (Phase 06) - unit/deterministic/edge tests in Tests/Economy
//
// Goods are kinds in a table, never entities. A region holds goods in common
// (RegionStock, its whole stock while coarse); a house of a detailed region
// holds its own (HouseStock). The land endows every region once, from its
// capacity and its deposits. When a region is detailed, a share of its common
// stock is split among its living houses by their members, the rest stays in
// common; when it is demoted, or a house goes extinct, the house's goods fold
// back into the common stock. Nothing is made or lost here (06.02 produces
// and consumes): the sum of a region's common stock and its houses' is the
// same before and after every promotion and demotion. Every change is an
// event about the region or the house.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyApi.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Persons.h"
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
	/// Kinds of goods. Counted, never crafted (Phase 09 makes things).
	enum class Good : uint8
	{
		Grain = 0,
		Cloth,
		Tools,
		Ore,
		Timber,
		Salt,
		Luxuries,
		Count
	};
	inline constexpr uint32 GoodCount = static_cast<uint32>(Good::Count);
	VAELEN_ECONOMY_API const char* GoodName(Good G) noexcept;

	/// Component on a region entity: the goods held in common, by Good.
	struct RegionStock
	{
		uint32 Amount[8] = {}; ///< [GoodCount] used, the rest reserved
	};
	static_assert(sizeof(RegionStock) == 32, "RegionStock must stay padding free");

	/// Component on a region entity, written by a higher layer (a polity's law,
	/// Phase 07) and read by the production system where it is observed: the
	/// share of every harvest that is owed away, and what has been assessed and
	/// not yet taken. Nothing in the economy knows who demands it or who takes
	/// it - the grain stays in the region's stock until a collector comes.
	struct RegionDues
	{
		uint32 PerMille = 0; ///< of every harvest, assessed the year it is reaped
		uint32 Owed = 0;	 ///< assessed and not yet taken
	};
	static_assert(sizeof(RegionDues) == 8, "RegionDues must stay padding free");

	/// Component on a family entity while its home region is detailed.
	struct HouseStock
	{
		uint32 Amount[8] = {};
	};
	static_assert(sizeof(HouseStock) == 32, "HouseStock must stay padding free");

	/// Component on a family entity: the house its goods pass to when it dies
	/// out. Written by a later system (06.05 wealth) from the culture's descent
	/// custom; the stock system honours it when told to observe the type, and
	/// without an observer an extinct house's goods return to the common stock.
	struct HouseHeir
	{
		uint32 Family = 0; ///< family index, 0 = none known
		uint32 Reserved = 0;
	};
	static_assert(sizeof(HouseHeir) == 8, "HouseHeir must stay padding free");

	struct EconomyTypes
	{
		ComponentType<RegionStock> Region;
		ComponentType<HouseStock> House;
		static VAELEN_ECONOMY_API EconomyTypes Declare(World& W);
	};

	struct EconomyRules
	{
		uint32 EndowGrainPerCapacity = 500;			 ///< grain per unit of capacity, per mille
		uint32 EndowTimberPerRichness = 1;			 ///< timber per point of richness of the region's timber deposits
		uint32 EndowOrePerRichness = 1;				 ///< ore per point of richness of its iron and copper deposits
		uint32 EndowSaltPerRichness = 1;			 ///< salt per point of richness of its salt deposits
		uint32 EndowLuxuryPerRichnessPerMille = 100; ///< luxuries per point of richness of its gold deposits
		uint32 HouseSharePerMille = 800; ///< share of the common stock split among the houses at a promotion
	};

	struct StockPayload
	{
		uint32 Region = 0;
		uint32 House = 0;  ///< family index (0 = the common stock), or a count of houses for Split and Folded
		uint32 Good = 0;   ///< Good, or GoodCount when every good is meant
		uint32 Amount = 0; ///< units
	};
	/// The land endowed a region (Amount = units of every good).
	inline constexpr EventType<StockPayload> StockEndowedEvent = MakeEventType<StockPayload>("StockEndowed");
	/// A promotion split the common stock among the houses (House = houses, Amount = units given).
	inline constexpr EventType<StockPayload> StockSplitEvent = MakeEventType<StockPayload>("StockSplit");
	/// A demotion folded the houses' goods into the common stock (House = houses, Amount = units).
	inline constexpr EventType<StockPayload> StockFoldedEvent = MakeEventType<StockPayload>("StockFolded");
	/// An extinct house returned its goods to the common stock (Amount = units).
	inline constexpr EventType<StockPayload> StockReturnedEvent = MakeEventType<StockPayload>("StockReturned");
	/// An extinct house's goods passed to its heir (House = the heir, Amount = units).
	inline constexpr EventType<StockPayload> StockInheritedEvent = MakeEventType<StockPayload>("StockInherited");
	/// Units added to, or taken from, a stock by AddStock.
	inline constexpr EventType<StockPayload> StockAddedEvent = MakeEventType<StockPayload>("StockAdded");
	inline constexpr EventType<StockPayload> StockTakenEvent = MakeEventType<StockPayload>("StockTaken");

	/// Yearly, after Families (and Lod, see RunAfter): endowments, folds and
	/// returns, then the split of newly detailed regions.
	class VAELEN_ECONOMY_API StockSystem final : public ISystem
	{
	public:
		StockSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					Population::FamilyTypes InFamilies, EconomyTypes InEconomy, EconomyRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Economy(InEconomy),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Stocks"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Families"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		/// Runs after another yearly system too (Lod), so that a promotion's
		/// houses are endowed in the same tick. The system must exist.
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: an extinct house's goods pass to the heir named on it
		/// (Phase 06 wealth) instead of returning to the common stock.
		void ObserveHeirs(ComponentType<HouseHeir> InHeirs) noexcept
		{
			Heirs = InHeirs;
			HasHeirs = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		EconomyTypes Economy;
		EconomyRules Rules;
		ComponentType<HouseHeir> Heirs;
		bool HasHeirs = false;
	};

	/// The common stock of a region (nullptr before its endowment or for an unknown region).
	VAELEN_ECONOMY_API const RegionStock* StockOf(const World& W, const History::PreHistoryTypes& Types,
												  const EconomyTypes& Economy, uint32 Region);
	/// The stock of a house (nullptr while its region is coarse, or for an unknown house).
	VAELEN_ECONOMY_API const HouseStock* HouseStockOf(const World& W, const Population::FamilyTypes& Families,
													  const EconomyTypes& Economy, uint32 Family);
	/// The whole stock of a region, common and houses, by Good (Out[GoodCount]).
	VAELEN_ECONOMY_API void TotalStock(const World& W, const History::PreHistoryTypes& Types,
									   const Population::FamilyTypes& Families, const EconomyTypes& Economy,
									   uint32 Region, uint32 Out[GoodCount]);
	/// Adds units (Delta > 0) to, or takes them (Delta < 0) from, the common
	/// stock of a region (House = 0) or a house's, clamped at zero. Returns the
	/// units actually moved, 0 for an unknown region, house or good; publishes
	/// StockAdded or StockTaken with the cause when anything moved.
	VAELEN_ECONOMY_API uint32 AddStock(World& W, const History::PreHistoryTypes& Types,
									   const Population::FamilyTypes& Families, const EconomyTypes& Economy,
									   uint32 Region, uint32 House, Good G, int32 Delta, SimTick Tick,
									   PersistentId Cause = {});

	struct StockStats
	{
		uint32 RegionsWithStock = 0;
		uint32 HousesWithStock = 0;
		uint32 Stale = 0;			   ///< house stocks on extinct houses or in coarse regions
		uint32 Total[GoodCount] = {};  ///< common and houses, region-filtered
		uint32 Common[GoodCount] = {}; ///< common only
		uint32 Endowed = 0;			   ///< events, from the log
		uint32 Splits = 0;
		uint32 Folds = 0;
		uint32 Returns = 0;
		uint32 Inheritances = 0;
		uint32 Added = 0;
		uint32 Taken = 0;
		Hash64 Digest = 0; ///< every common stock in region order, then every house stock in family order
	};
	VAELEN_ECONOMY_API StockStats MeasureStocks(const World& W, const History::PreHistoryTypes& Types,
												const Population::PersonTypes& Persons,
												const Population::FamilyTypes& Families, const EconomyTypes& Economy,
												uint32 Region);
} // namespace Vaelen::Economy
