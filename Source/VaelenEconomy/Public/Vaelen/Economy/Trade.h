// VAELEN - VaelenEconomy
// Phase 06.04: trade and routes - routes opened between neighbouring markets
// where prices differ enough, goods carried yearly from the cheap side to the
// dear one, routes closed when idle, settlements marked where the traffic is.
//
// STATUS: VALIDATED (Phase 06) - deterministic/long-duration tests in Tests/Economy
//
// A route is an entity of kind Route between two adjacent regions, opened the
// year a good is dear enough on one side and in surplus on the other, and
// closed after years of carrying nothing. Every year, after the markets have
// priced, every open route carries a share of the cheaper side's surplus of
// every good, up to what the dearer side wants and a yearly limit, from the
// one common stock to the other; goods move, nothing is paid yet (wealth and
// payment are 06.05). A settlement is an entity of kind Settlement on a region
// whose routes carried enough in a year, abandoned after years without any.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/EconomyApi.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
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
	/// Component of a route entity (ids of kind Route).
	struct RouteInfo
	{
		uint32 Index = 0;	 ///< 1-based, in order of opening
		uint32 From = 0;	 ///< the lower region index
		uint32 To = 0;		 ///< the higher
		uint16 Idle = 0;	 ///< years in a row carrying nothing
		uint16 Openings = 0; ///< times this road was opened (1 the year it was built)
		uint64 Opened = 0;
		uint64 Closed = 0;	 ///< tick, 0 while open
		uint64 Carried = 0;	 ///< units, both ways, over its life
		Hash64 Identity = 0; ///< from the world seed
	};
	static_assert(sizeof(RouteInfo) == 48, "RouteInfo must stay padding free");

	/// Component of a settlement entity (ids of kind Settlement).
	struct SettlementInfo
	{
		uint32 Index = 0; ///< 1-based, in order of founding
		uint32 Region = 0;
		uint32 Routes = 0;	///< open routes touching the region, last year
		uint32 Traffic = 0; ///< units carried on them, last year
		uint64 Founded = 0;
		uint64 Abandoned = 0; ///< tick, 0 while alive
		uint32 Quiet = 0;	  ///< years in a row without traffic
		uint32 Reserved = 0;
		Hash64 Identity = 0;
	};
	static_assert(sizeof(SettlementInfo) == 48, "SettlementInfo must stay padding free");

	/// What a made road adds to what a route can carry, on the route entity:
	/// written by a later module (Phase 09 infrastructure), read by the trade
	/// system when told to observe the type. A road does not create trade; it
	/// means more of the trade that already wanted to happen gets across.
	struct RouteEase
	{
		uint32 CarryPerMille = 0; ///< added to both the share carried and the yearly cap
		uint32 Reserved = 0;
	};
	static_assert(sizeof(RouteEase) == 8, "RouteEase must stay padding free");

	struct TradeTypes
	{
		ComponentType<RouteInfo> Route;
		ComponentType<SettlementInfo> Settlement;
		static VAELEN_ECONOMY_API TradeTypes Declare(World& W);
	};

	struct TradeRules
	{
		uint32 OpenGapPerMille = 1000;		///< a route opens when a good is this much dearer on one side (twice)
		uint32 CarryPerMille = 250;			///< of the seller's surplus of a good carried a year
		uint32 CarryMax = 1000;				///< units of a good a route carries a year at most
		uint32 MaxRoutesPerRegion = 4;		///< open routes touching a region
		uint32 CloseAfterIdleYears = 5;		///< a route carrying nothing this long closes
		uint32 SettleFromTraffic = 50;		///< units a year through a region's routes found a settlement
		uint32 AbandonAfterQuietYears = 10; ///< a settlement without traffic this long is abandoned
	};

	struct TradePayload
	{
		uint32 Route = 0; ///< route or settlement index
		uint32 From = 0;  ///< region (a settlement's region for its events)
		uint32 To = 0;
		uint32 Amount = 0; ///< units carried, or the traffic that founded a settlement
	};
	inline constexpr EventType<TradePayload> RouteOpenedEvent = MakeEventType<TradePayload>("RouteOpened");
	inline constexpr EventType<TradePayload> RouteClosedEvent = MakeEventType<TradePayload>("RouteClosed");
	inline constexpr EventType<TradePayload> GoodsCarriedEvent = MakeEventType<TradePayload>("GoodsCarried");
	inline constexpr EventType<TradePayload> SettlementFoundedEvent = MakeEventType<TradePayload>("SettlementFounded");
	inline constexpr EventType<TradePayload> SettlementAbandonedEvent =
		MakeEventType<TradePayload>("SettlementAbandoned");

	/// Yearly, after Markets: carry on the open routes, close the idle, open the
	/// warranted, mark the settlements.
	class VAELEN_ECONOMY_API TradeSystem final : public ISystem
	{
	public:
		TradeSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
					Population::FamilyTypes InFamilies, EconomyTypes InEconomy, MarketTypes InMarkets,
					TradeTypes InTrade, ProductionRules InConsumption, MarketRules InPrices,
					TradeRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Families(InFamilies), Economy(InEconomy),
			  Markets(InMarkets), Trade(InTrade), Consumption(InConsumption), Prices(InPrices), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Trade"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Markets"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: the roads made on the routes (09.04). A route with a road on
		/// it carries more; a route without one carries exactly what it always did.
		void ObserveRoads(ComponentType<RouteEase> InEase) noexcept
		{
			Ease = InEase;
			HasEase = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		EconomyTypes Economy;
		MarketTypes Markets;
		TradeTypes Trade;
		ProductionRules Consumption;
		MarketRules Prices;
		TradeRules Rules;
		ComponentType<RouteEase> Ease;
		bool HasEase = false;
		Hash64 GraphDigest = 0;		 ///< derived cache, not state
		WorldGen::RegionGraph Graph; ///< derived cache, not state
	};

	/// The open routes touching a region, by index.
	VAELEN_ECONOMY_API void RoutesOf(const World& W, const TradeTypes& Trade, uint32 Region,
									 std::vector<RouteInfo>& Out);
	/// The open route between two regions (nullptr when none).
	VAELEN_ECONOMY_API const RouteInfo* RouteBetween(const World& W, const TradeTypes& Trade, uint32 A, uint32 B);
	/// The living settlement of a region (nullptr when none).
	VAELEN_ECONOMY_API const SettlementInfo* SettlementOf(const World& W, const TradeTypes& Trade, uint32 Region);

	struct TradeStats
	{
		uint32 RoutesOpen = 0;
		uint32 RoutesClosed = 0;
		uint32 MostTraffic = 0; ///< the busiest living settlement's traffic
		uint32 Carries = 0;		///< GoodsCarried events, from the log
		uint64 Carried = 0;		///< units, from the log
		uint32 Settlements = 0; ///< alive
		uint32 Abandoned = 0;
		uint32 Bad = 0;	   ///< open routes not between adjacent regions, opened twice, or on a region past its number
		Hash64 Digest = 0; ///< every route in index order, then every settlement
	};
	VAELEN_ECONOMY_API TradeStats MeasureTrade(const World& W, const History::PreHistoryTypes& Types,
											   const TradeTypes& Trade, const TradeRules& Rules);
} // namespace Vaelen::Economy
