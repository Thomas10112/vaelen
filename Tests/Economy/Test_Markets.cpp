// VAELEN - Tests/Economy
// Phase 06.03: markets and prices - scarcity within the bounds, a drought
// behind a price, prices across the world and across the grains, a century
// frozen.
//
// STATUS: VALIDATED (Phase 06)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
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

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.03): AELVOR 128 at
// year 300, the busiest region detailed, 100 years with every Phase 04 body
// and Phase 06 system so far.
#define VAELEN_MARKETS_FROZEN_128 0x5c3edf001360621eull
#define VAELEN_MARKETS_CHANGES_128 584u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogMarkets);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, ProductionRules InRules = ProductionRules{})
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
														 InRules);
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, InRules,
												  MarketRules{});
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
		MarketStats Stats(uint32 Region = 0) const { return MeasureMarkets(Instance, Ages.Types(), Markets, Region); }
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
		ComponentType<RegionStores> Stores;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
	};
} // namespace

VAELEN_TEST(Markets, PricesFollowScarcityWithinTheBounds)
{
	// The price rule alone.
	const MarketRules Rules;
	const ProductionRules Use;
	VT_CHECK_EQ(PriceFor(Rules, Good::Grain, 0, 0), 2u);	  // nothing wanted: the floor (a quarter of 10)
	VT_CHECK_EQ(PriceFor(Rules, Good::Grain, 100, 0), 80u);	  // nothing held: the ceiling (eight times)
	VT_CHECK_EQ(PriceFor(Rules, Good::Grain, 100, 100), 10u); // held what is wanted: the base
	VT_CHECK_EQ(PriceFor(Rules, Good::Grain, 100, 400), 2u);  // plenty: the floor, never under
	VT_CHECK_EQ(PriceFor(Rules, Good::Grain, 300, 100), 30u);
	VT_CHECK_EQ(PriceFor(Rules, Good::Luxuries, 1, 1), 200u);
	VT_CHECK_EQ(PriceFor(Rules, Good::Count, 1, 1), 0u);
	VT_CHECK_EQ(WantedStock(Use, Rules, Good::Grain, 100), 800u); // two years of four a head
	VT_CHECK_EQ(WantedStock(Use, Rules, Good::Grain, 0), 0u);
	VT_CHECK_EQ(WantedStock(Use, Rules, Good::Timber, 0), 1u); // a little is always wanted
	VT_CHECK_EQ(WantedStock(Use, Rules, Good::Luxuries, 400), 3u);
	VT_CHECK_EQ(WantedStock(Use, Rules, Good::Count, 400), 0u);
	RegionMarket M;
	for (uint32 g = 0; g < GoodCount; ++g)
	{
		M.Price[g] = Rules.BasePrice[g];
	}
	const uint32 Nothing[GoodCount] = {};
	const uint32 Some[GoodCount] = {10, 1, 0, 0, 0, 0, 1};
	VT_CHECK_EQ(ValueOf(Nothing, M), 0u);
	VT_CHECK_EQ(ValueOf(Some, M), 100u + 40u + 200u);
	// The world: every region holding a stock has prices within the bounds.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const MarketStats S = W.Stats();
	VT_CHECK_EQ(S.Markets, W.Stock().RegionsWithStock);
	uint32 OutOfBounds = 0;
	uint32 Cheap = 0;
	uint32 Regions = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionMarket* Mk = W.Instance.Components().GetPool(W.Markets.Market).TryGet(H);
				if (Mk == nullptr)
				{
					return;
				}
				++Regions;
				for (uint32 g = 0; g < GoodCount; ++g)
				{
					const uint32 Floor = Rules.BasePrice[g] * Rules.FloorPerMille / 1000u;
					const uint32 Ceiling = Rules.BasePrice[g] * Rules.CeilingPerMille / 1000u;
					OutOfBounds += Mk->Price[g] < Floor || Mk->Price[g] > Ceiling ? 1u : 0u;
				}
				// Grain held beyond two years of need is never dearer than the base.
				uint32 Total[GoodCount];
				TotalStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, R.Index, Total);
				const RegionPopulation* P =
					W.Instance.Components().GetPool(W.Ages.Types().Population.Population).TryGet(H);
				const uint64 Wanted = WantedStock(Use, Rules, Good::Grain, P != nullptr ? P->Total : 0u);
				if (Total[0] >= Wanted)
				{
					++Cheap;
					VT_CHECK(Mk->Price[0] <= Rules.BasePrice[0]);
				}
			});
	VT_CHECK_EQ(OutOfBounds, 0u);
	VT_CHECK(Cheap > 0);
	VAELEN_LOG_INFO(LogMarkets,
					"year 300: %u markets; grain %u..%u (%u at the floor), tools %u..%u, luxuries %u..%u "
					"(%u at the ceiling); %u price events, %u caused",
					S.Markets, S.Lowest[0], S.Highest[0], S.AtFloor[0], S.Lowest[2], S.Highest[2], S.Lowest[6],
					S.Highest[6], S.AtCeiling[6], S.Changes, S.Caused);
	// Scarcity by hand: the busiest region's grain taken, its price climbs; a flood of grain, the floor.
	const uint32 Region = W.Busiest();
	const uint32 Before = W.Price(Region, Good::Grain);
	VT_REQUIRE(Before > 0);
	std::vector<uint32> Houses;
	W.Instance.Components()
		.GetPool(W.Families.Family)
		.ForEach([&](EntityHandle, const FamilyInfo& F) { Houses.push_back(F.Region == Region ? F.Index : 0u); });
	AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Grain, -0x7fffffff, W.Instance.Now());
	W.Ages.Run(1);
	const uint32 Scarce = W.Price(Region, Good::Grain);
	AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Grain, 0x7fffffff, W.Instance.Now());
	W.Ages.Run(1);
	const uint32 Flooded = W.Price(Region, Good::Grain);
	VAELEN_LOG_INFO(LogMarkets, "region %u grain: %u, %u once the store is taken, %u once flooded", Region, Before,
					Scarce, Flooded);
	VT_CHECK(Scarce > Before);
	VT_CHECK_EQ(Flooded, Rules.BasePrice[0] * Rules.FloorPerMille / 1000u);
	VT_CHECK(MarketOf(W.Instance, W.Ages.Types(), W.Markets, 0xfffffff0u) == nullptr);
	VT_CHECK_EQ(W.Price(0xfffffff0u, Good::Grain), 0u);
}

VAELEN_TEST(Markets, AScarceHarvestMovesThePriceWithTheDroughtBehindIt)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	const uint32 Before = W.Price(Region, Good::Grain);
	const uint32 Changes = W.Stats(Region).Changes;
	// A drought and a thin store together: the price moves, and the event says why.
	VT_REQUIRE(W.Curse(Region, DisasterKind::Drought));
	const RegionStock* Common = StockOf(W.Instance, W.Ages.Types(), W.Economy, Region);
	VT_REQUIRE(Common != nullptr);
	const int32 Keep = static_cast<int32>(Common->Amount[0] / 8u);
	AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Grain,
			 -static_cast<int32>(Common->Amount[0]) + Keep, W.Instance.Now());
	W.Ages.Run(1);
	const uint32 After = W.Price(Region, Good::Grain);
	const MarketStats S = W.Stats(Region);
	VAELEN_LOG_INFO(LogMarkets, "region %u: grain %u, then %u after a drought on a thin store (%u price events)",
					Region, Before, After, S.Changes - Changes);
	VT_CHECK(After > Before);
	VT_CHECK(S.Changes > Changes);
	const Event* Change = nullptr;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(PriceChangedEvent) && E.Get<StockPayload>().Region == Region &&
			E.Get<StockPayload>().Good == static_cast<uint32>(Good::Grain))
		{
			Change = &E;
		}
	}
	VT_REQUIRE(Change != nullptr);
	VT_CHECK_EQ(Change->Get<StockPayload>().Amount, After);
	const Event* Harvest = FindEvent(W.Instance.Log(), Change->Cause);
	VT_REQUIRE(Harvest != nullptr && Harvest->Is(HarvestEvent));
	const Event* Drought = FindEvent(W.Instance.Log(), Harvest->Cause);
	VT_REQUIRE(Drought != nullptr && Drought->Is(DisasterStruckEvent));
	VT_CHECK_EQ(Drought->Get<DisasterPayload>().Kind, static_cast<uint32>(DisasterKind::Drought));
	// Price events are sparse: at most one a good a region a year, most years none.
	uint32 Peopled = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().Population.Population)
		.ForEach([&](EntityHandle, const RegionPopulation& P) { Peopled += P.Total > 0 ? 1u : 0u; });
	const MarketStats All = W.Stats();
	VT_CHECK(All.Changes < 301u * Peopled * GoodCount / 4u);
}

VAELEN_TEST(Markets, PricesVaryAcrossTheWorldAndAgreeAcrossTheGrains)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	const MarketStats S = A.Stats();
	VT_CHECK(S.Lowest[0] < S.Highest[0]);				 // grain is dear somewhere, cheap elsewhere
	VT_CHECK(S.AtCeiling[3] > 0);						 // ore where no deposit is
	VT_CHECK(S.AtFloor[0] + S.AtCeiling[0] < S.Markets); // and grain has prices between the bounds
	VT_CHECK_EQ(S.Digest, B.Stats().Digest);
	// The busiest region detailed in B: its prices stay close to A's.
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(5);
	B.Ages.Run(5);
	uint32 Apart = 0;
	for (uint32 g = 0; g < GoodCount; ++g)
	{
		const uint32 PA = A.Price(Region, static_cast<Good>(g));
		const uint32 PB = B.Price(Region, static_cast<Good>(g));
		Apart += PB * 2u < PA || PB > PA * 2u ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogMarkets, "region %u after five years: grain %u coarse, %u detailed; %u goods apart", Region,
					A.Price(Region, Good::Grain), B.Price(Region, Good::Grain), Apart);
	VT_CHECK_EQ(Apart, 0u);
	VT_CHECK(B.Market(Region) != nullptr);
}

VAELEN_TEST(Markets, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	uint32 Failures = 0;
	std::vector<uint8> Image;
	for (uint32 Year = 1; Year <= 100; ++Year)
	{
		A.Ages.Run(1);
		B.Ages.Run(1);
		if (Year == 50)
		{
			SaveSnapshot(A.Instance, Image);
		}
		if (Year % 10 == 0 && ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the two worlds differ", Year);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const MarketStats S = A.Stats();
	VAELEN_LOG_INFO(LogMarkets, "frozen: markets=%016llx changes=%u (%u markets, grain %u..%u)",
					static_cast<unsigned long long>(S.Digest), S.Changes, S.Markets, S.Lowest[0], S.Highest[0]);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_MARKETS_FROZEN_128});
	VT_CHECK_EQ(S.Changes, uint32{VAELEN_MARKETS_CHANGES_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, S.Digest);
}
