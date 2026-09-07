// VAELEN - VaelenEconomy
// Phase 06.03: markets and prices - every region a market, integer prices
// moved by what the region holds over what it wants, within a floor and a
// ceiling, and the value of a stock at those prices.
//
// STATUS: VALIDATED (Phase 06) - unit/distribution/deterministic tests in Tests/Economy
//
// A market is a component on the region: one integer price per good. Every
// year, after production, a region wants so many years of every good by the
// same consumption rules the production uses (its people's grain, timber,
// salt, cloth, tools, the ore its tools take, a little luxury); its price for
// a good is the base price times what it wants over what it holds - common
// stock and houses together - clamped between a quarter and eight times the
// base. A price that moved by a quarter or more is an event, with the year's
// harvest as cause for grain. Prices exist in both grains alike and are
// what 06.04 trade and 06.05 wealth read.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyApi.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
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
	/// Component on a region entity: its prices, by Good.
	struct RegionMarket
	{
		uint32 Price[8] = {}; ///< [GoodCount] used, the rest reserved
	};
	static_assert(sizeof(RegionMarket) == 32, "RegionMarket must stay padding free");

	struct MarketTypes
	{
		ComponentType<RegionMarket> Market;
		static VAELEN_ECONOMY_API MarketTypes Declare(World& W);
	};

	struct MarketRules
	{
		uint32 BasePrice[GoodCount] = {10, 40, 60, 20,
									   8,  30, 200}; ///< grain, cloth, tools, ore, timber, salt, luxuries
		uint32 FloorPerMille = 250;					 ///< of the base
		uint32 CeilingPerMille = 8000;
		uint32 GrainYearsWanted = 2;   ///< a market wants this many years of grain in the region
		uint32 GoodsYearsWanted = 3;   ///< and of every other used good
		uint32 LuxuryPerPersons = 200; ///< one luxury wanted per this many people
		uint32 ChangePerMille = 250;   ///< a price moved by this much is an event
	};

	/// A price moved (Region, 0, Good, new price); the year's harvest is the cause for grain.
	inline constexpr EventType<StockPayload> PriceChangedEvent = MakeEventType<StockPayload>("PriceChanged");

	/// Yearly, after Production: the prices of every region holding a stock.
	class VAELEN_ECONOMY_API MarketSystem final : public ISystem
	{
	public:
		MarketSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					 Population::FamilyTypes InFamilies, EconomyTypes InEconomy, MarketTypes InMarkets,
					 ProductionRules InConsumption, MarketRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Economy(InEconomy),
			  Markets(InMarkets), Consumption(InConsumption), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Markets"; }
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
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		EconomyTypes Economy;
		MarketTypes Markets;
		ProductionRules Consumption;
		MarketRules Rules;
	};

	/// What a region wants to hold of a good, by the rules, for so many people.
	VAELEN_ECONOMY_API uint64 WantedStock(const ProductionRules& Consumption, const MarketRules& Rules, Good G,
										  uint64 People) noexcept;
	/// The price of a good from what is wanted over what is held, within the bounds.
	VAELEN_ECONOMY_API uint32 PriceFor(const MarketRules& Rules, Good G, uint64 Wanted, uint64 Held) noexcept;
	/// The market of a region (nullptr before its first prices or for an unknown region).
	VAELEN_ECONOMY_API const RegionMarket* MarketOf(const World& W, const History::PreHistoryTypes& Types,
													const MarketTypes& Markets, uint32 Region);
	/// The value of a stock at a market's prices.
	VAELEN_ECONOMY_API uint64 ValueOf(const uint32 Amount[GoodCount], const RegionMarket& Market) noexcept;

	struct MarketStats
	{
		uint32 Markets = 0;				  ///< regions with prices, region-filtered
		uint32 Lowest[GoodCount] = {};	  ///< the lowest price of each good
		uint32 Highest[GoodCount] = {};	  ///< the highest
		uint32 AtFloor[GoodCount] = {};	  ///< markets at the floor
		uint32 AtCeiling[GoodCount] = {}; ///< markets at the ceiling
		uint32 Changes = 0;				  ///< price events, from the log
		uint32 Caused = 0;				  ///< with a cause
		Hash64 Digest = 0;				  ///< every market in region order
	};
	VAELEN_ECONOMY_API MarketStats MeasureMarkets(const World& W, const History::PreHistoryTypes& Types,
												  const MarketTypes& Markets, uint32 Region);
} // namespace Vaelen::Economy
