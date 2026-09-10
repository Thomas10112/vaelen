// VAELEN - Tests/Economy
// Phase 06.04: trade and routes - routes on price gaps carrying goods from
// the cheap side to the dear one, routes closed when idle, settlements
// founded and abandoned, five hundred years at 64 frozen.
//
// STATUS: VALIDATED (Phase 06)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.04): AELVOR 64 at
// year 120, the busiest region detailed, 500 years with every Phase 04 body
// and Phase 06 system so far.
#define VAELEN_TRADE_FROZEN_64 0x6f8855c6ff82b417ull
#define VAELEN_TRADE_CARRIED_64 133492ull
#define VAELEN_TRADE_SETTLEMENTS_64 23u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogTrade);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, bool WithTrade = true, TradeRules InTrade = TradeRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Economy = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			Stores = Instance.Types().Register<RegionStores>("RegionStores"); // a council's granary (05.05)
			Instance.Components().CreatePool(Stores);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy, Production,
														 ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, Trade,
												  ProductionRules{}, MarketRules{}, InTrade);
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveStores(Stores);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			if (WithTrade)
			{
				Instance.Systems().Add(Roads.get());
			}
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		static WorldGenConfig Square(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			return Gen;
		}
		uint32 Busiest() const
		{
			uint32 Best = 0;
			uint32 People = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && (P->Total > People || (P->Total == People && R.Index < Best)))
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		EntityHandle RegionHandle(uint32 Region) const
		{
			EntityHandle Out;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region && Out.IsNull())
						{
							Out = H;
						}
					});
			return Out;
		}
		const RegionPopulation* Counts(uint32 Region) const
		{
			const EntityHandle H = RegionHandle(Region);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
		}
		TradeStats Stats() const { return MeasureTrade(Instance, Ages.Types(), Trade, TradeRules{}); }
		MarketStats Fairs() const { return MeasureMarkets(Instance, Ages.Types(), Markets, 0); }
		/// Every good of every region's common stock set to Amount (a flood, or a drain).
		void FillEvery(int32 Delta)
		{
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle, const RegionInfo& R)
					{
						for (uint32 g = 0; g < GoodCount; ++g)
						{
							AddStock(Instance, Ages.Types(), Families, Economy, R.Index, 0, static_cast<Good>(g), Delta,
									 Instance.Now());
						}
					});
		}
		const RegionMarket* Market(uint32 Region) const { return MarketOf(Instance, Ages.Types(), Markets, Region); }
		uint32 Price(uint32 Region, Good G) const
		{
			const RegionMarket* M = Market(Region);
			return M != nullptr ? M->Price[static_cast<uint32>(G)] : 0u;
		}
		StockStats Stock(uint32 Region = 0) const
		{
			return MeasureStocks(Instance, Ages.Types(), Persons, Families, Economy, Region);
		}
		/// Units harvested in a region during the last Years years.
		uint64 Harvested(uint32 Region, uint32 Years) const
		{
			uint64 Sum = 0;
			const std::vector<Event>& All = Instance.Log().All();
			for (usize i = All.size(); i > 0; --i)
			{
				const Event& E = All[i - 1];
				if (E.Tick + uint64{TicksPerYear} * Years < Instance.Now())
				{
					break;
				}
				if (E.Is(HarvestEvent) && E.Get<StockPayload>().Region == Region)
				{
					Sum += E.Get<StockPayload>().Amount;
				}
			}
			return Sum;
		}
		bool Curse(uint32 Region, DisasterKind Kind)
		{
			bool Queued = false;
			Instance.Components()
				.GetPool(Ages.Types().Disasters.State)
				.ForEach(
					[&](EntityHandle, DisasterState& S)
					{
						if (!Queued && S.PendingCount < DisasterState::MaxPending)
						{
							S.Pending[S.PendingCount] = PendingOmen{Region, static_cast<uint32>(Kind), 1000, 0, 0};
							++S.PendingCount;
							Queued = true;
						}
					});
			return Queued;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		EconomyTypes Economy;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		ComponentType<RegionStores> Stores;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
	};
} // namespace

VAELEN_TEST(Trade, RoutesOpenOnPriceGapsAndCarryGoodsFromCheapToDear)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const TradeStats S = W.Stats();
	VAELEN_LOG_INFO(LogTrade,
					"year 300: %u routes open, %u closed, %llu units carried in %u years' carries, %u "
					"settlements (%u abandoned, the busiest at %u), %u bad",
					S.RoutesOpen, S.RoutesClosed, static_cast<unsigned long long>(S.Carried), S.Carries, S.Settlements,
					S.Abandoned, S.MostTraffic, S.Bad);
	VT_CHECK(S.RoutesOpen > 0);
	VT_CHECK(S.Carried > 0 && S.Carries > 0);
	VT_CHECK_EQ(S.Bad, 0u);
	VT_CHECK(S.Settlements > 0);
	// Every open route joins two markets; every settlement sits on a region with routes.
	uint32 Bad = 0;
	W.Instance.Components()
		.GetPool(W.Trade.Route)
		.ForEach(
			[&](EntityHandle, const RouteInfo& R)
			{
				Bad += R.Closed == 0 && (W.Market(R.From) == nullptr || W.Market(R.To) == nullptr) ? 1u : 0u;
				Bad += R.From >= R.To ? 1u : 0u;
			});
	W.Instance.Components()
		.GetPool(W.Trade.Settlement)
		.ForEach(
			[&](EntityHandle, const SettlementInfo& St)
			{
				std::vector<RouteInfo> Routes;
				RoutesOf(W.Instance, W.Trade, St.Region, Routes);
				Bad += St.Abandoned == 0 && Routes.empty() && St.Quiet == 0 ? 1u : 0u;
				Bad += St.Identity == 0 ? 1u : 0u;
			});
	VT_CHECK_EQ(Bad, 0u);
	// Trade brings ore where no deposit is: fewer markets at the ore ceiling than without it.
	Run X(AelvorSeed, false);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	const MarketStats With = W.Fairs();
	const MarketStats Without = X.Fairs();
	VAELEN_LOG_INFO(LogTrade, "ore at the ceiling: %u markets with trade, %u without", With.AtCeiling[3],
					Without.AtCeiling[3]);
	VT_CHECK(With.AtCeiling[3] < Without.AtCeiling[3]);
	VT_CHECK_EQ(X.Stats().RoutesOpen + X.Stats().RoutesClosed, 0u);
	// By hand: a region flooded with grain beside a drained neighbour, the grain crosses.
	const uint32 Region = W.Busiest();
	std::vector<RouteInfo> Routes;
	RoutesOf(W.Instance, W.Trade, Region, Routes);
	VT_REQUIRE(!Routes.empty());
	const uint32 Other = Routes[0].From == Region ? Routes[0].To : Routes[0].From;
	// (luxuries: nothing makes them, so the neighbour cannot fill its own want)
	AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Luxuries, 100000, W.Instance.Now());
	AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Other, 0, Good::Luxuries, -0x7fffffff,
			 W.Instance.Now());
	const uint32 Before =
		StockOf(W.Instance, W.Ages.Types(), W.Economy, Other)->Amount[static_cast<uint32>(Good::Luxuries)];
	const uint64 CarriedBefore = RouteBetween(W.Instance, W.Trade, Region, Other)->Carried;
	W.Ages.Run(1); // this year's prices see the flood and the drain, and the route carries on them
	const uint32 After =
		StockOf(W.Instance, W.Ages.Types(), W.Economy, Other)->Amount[static_cast<uint32>(Good::Luxuries)];
	VAELEN_LOG_INFO(LogTrade, "region %u flooded with luxuries beside %u: %u there, then %u", Region, Other, Before,
					After);
	VT_CHECK_EQ(Before, 0u);
	VT_CHECK(After > Before);
	VT_REQUIRE(RouteBetween(W.Instance, W.Trade, Region, Other) != nullptr);
	VT_CHECK(RouteBetween(W.Instance, W.Trade, Region, Other)->Carried > CarriedBefore);
	VT_CHECK(RouteBetween(W.Instance, W.Trade, Other, Region) == RouteBetween(W.Instance, W.Trade, Region, Other));
	VT_CHECK(RouteBetween(W.Instance, W.Trade, 0xfffffff0u, Region) == nullptr);
	VT_CHECK(SettlementOf(W.Instance, W.Trade, 0xfffffff0u) == nullptr);
}

VAELEN_TEST(Trade, RoutesCloseWhenIdleAndSettlementsRiseAndFall)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const TradeStats Before = W.Stats();
	VT_REQUIRE(Before.RoutesOpen > 0 && Before.Settlements > 0);
	// A world flooded with everything: no gap, no surplus wanted anywhere, the
	// routes carry nothing, close in five years, and the settlements empty in ten.
	for (uint32 Year = 1; Year <= 12; ++Year)
	{
		W.FillEvery(1000000);
		W.Ages.Run(1);
	}
	const TradeStats Flooded = W.Stats();
	VAELEN_LOG_INFO(LogTrade, "flooded: %u routes open (%u closed), %u settlements (%u abandoned)", Flooded.RoutesOpen,
					Flooded.RoutesClosed, Flooded.Settlements, Flooded.Abandoned);
	VT_CHECK_EQ(Flooded.RoutesOpen, 0u);
	VT_CHECK(Flooded.RoutesClosed >= Before.RoutesOpen);
	VT_CHECK_EQ(Flooded.Settlements, 0u);
	VT_CHECK(Flooded.Abandoned >= Before.Settlements);
	VT_CHECK_EQ(Flooded.Carried, Before.Carried); // nothing carried since
	uint32 Closings = 0;
	uint32 Abandonings = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Closings += E.Is(RouteClosedEvent) ? 1u : 0u;
		Abandonings += E.Is(SettlementAbandonedEvent) ? 1u : 0u;
	}
	VT_CHECK(Closings >= Flooded.RoutesClosed); // a road once built is reopened, and may close again
	VT_CHECK(Abandonings >= Flooded.Abandoned);
	// Drained again, the gaps return: new routes and new settlements, with new indices.
	for (uint32 Year = 1; Year <= 6; ++Year)
	{
		W.FillEvery(-1000000);
		W.Ages.Run(1);
	}
	W.Ages.Run(6);
	const TradeStats Again = W.Stats();
	VAELEN_LOG_INFO(LogTrade, "drained: %u routes open, %u settlements again (%u abandoned before)", Again.RoutesOpen,
					Again.Settlements, Again.Abandoned);
	VT_CHECK(Again.RoutesOpen > 0);
	VT_CHECK(Again.Settlements > 0);
	VT_CHECK_EQ(Again.Bad, 0u);
	uint32 Newest = 0;
	W.Instance.Components()
		.GetPool(W.Trade.Settlement)
		.ForEach([&](EntityHandle, const SettlementInfo& St) { Newest = std::max(Newest, St.Index); });
	VT_CHECK(Newest > Flooded.Abandoned);
}

VAELEN_TEST(Trade, RulesAndEdges)
{
	// A tight rule: one route a region at most, and never twice the same pair.
	TradeRules Tight;
	Tight.MaxRoutesPerRegion = 1;
	Run W(AelvorSeed, true, Tight);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	uint32 Over = 0;
	std::vector<uint32> At(200, 0u);
	W.Instance.Components()
		.GetPool(W.Trade.Route)
		.ForEach(
			[&](EntityHandle, const RouteInfo& R)
			{
				if (R.Closed == 0 && R.From < At.size() && R.To < At.size())
				{
					++At[R.From];
					++At[R.To];
				}
			});
	for (const uint32 N : At)
	{
		Over += N > 1 ? 1u : 0u;
	}
	VT_CHECK_EQ(Over, 0u);
	VT_CHECK(W.Stats().RoutesOpen > 0);
	VT_CHECK_EQ(MeasureTrade(W.Instance, W.Ages.Types(), W.Trade, Tight).Bad, 0u);
	// A route never opens without a gap: a rule asking for a hundredfold never opens one.
	TradeRules Never;
	Never.OpenGapPerMille = 100000;
	Run X(AelvorSeed, true, Never);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	VT_CHECK_EQ(X.Stats().RoutesOpen + X.Stats().RoutesClosed, 0u);
	VT_CHECK_EQ(X.Stats().Settlements + X.Stats().Abandoned, 0u);
	// A region without neighbours (an island) never has a route.
	const RegionGraph Graph = BuildRegionGraph(W.Instance.Map(), W.Ages.Types().World.Regions);
	uint32 Islands = 0;
	uint32 IslandRoutes = 0;
	for (uint32 R = 1; R < Graph.Neighbours.size(); ++R)
	{
		if (!Graph.Neighbours[R].empty())
		{
			continue;
		}
		++Islands;
		std::vector<RouteInfo> Routes;
		RoutesOf(W.Instance, W.Trade, R, Routes);
		IslandRoutes += static_cast<uint32>(Routes.size());
	}
	VAELEN_LOG_INFO(LogTrade, "%u islands without a neighbour, %u routes on them", Islands, IslandRoutes);
	VT_CHECK_EQ(IslandRoutes, 0u);
	// Two worlds: the same routes, settlements and digest.
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
}

VAELEN_TEST(Trade, FiveHundredYearsAt64HoldTheRoadsAndFreeze)
{
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	uint32 Failures = 0;
	uint32 MostOpen = 0;
	std::vector<uint8> Image;
	for (uint32 Year = 1; Year <= 500; ++Year)
	{
		A.Ages.Run(1);
		if (Year == 250)
		{
			SaveSnapshot(A.Instance, Image);
		}
		if (Year % 50 != 0)
		{
			continue;
		}
		const TradeStats S = A.Stats();
		MostOpen = std::max(MostOpen, S.RoutesOpen);
		// Every living settlement sits on a market; every closed route stays closed and idle.
		uint32 Bad = S.Bad;
		A.Instance.Components()
			.GetPool(A.Trade.Settlement)
			.ForEach([&](EntityHandle, const SettlementInfo& St)
					 { Bad += St.Abandoned == 0 && A.Market(St.Region) == nullptr ? 1u : 0u; });
		A.Instance.Components()
			.GetPool(A.Trade.Route)
			.ForEach([&](EntityHandle, const RouteInfo& R) { Bad += R.Closed != 0 && R.Closed < R.Opened ? 1u : 0u; });
		if (Bad != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad roads or settlements", Year, Bad);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK(MostOpen > 0);
	const TradeStats S = A.Stats();
	VAELEN_LOG_INFO(LogTrade,
					"500 years at 64: %u routes open, %u closed, %llu carried, %u settlements alive, %u abandoned; "
					"digest %016llx",
					S.RoutesOpen, S.RoutesClosed, static_cast<unsigned long long>(S.Carried), S.Settlements,
					S.Abandoned, static_cast<unsigned long long>(S.Digest));
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_TRADE_FROZEN_64});
	VT_CHECK_EQ(S.Carried, uint64{VAELEN_TRADE_CARRIED_64});
	VT_CHECK_EQ(S.Settlements + S.Abandoned, uint32{VAELEN_TRADE_SETTLEMENTS_64});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(250);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, S.Digest);
}
