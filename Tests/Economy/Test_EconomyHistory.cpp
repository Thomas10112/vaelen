// VAELEN - Tests/Economy
// Phase 06.07: the economy in the chronicle - a line for every economic
// event, only what matters recorded, and the why of a dear loaf reaching the
// drought three layers down.
//
// STATUS: VALIDATED (Phase 06)

#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.07): AELVOR 128 at
// year 300, the busiest region detailed, 100 years with every Phase 04, 05
// and 06 system.
#define VAELEN_ECONOMYHISTORY_RECORDS_128 574u
#define VAELEN_ECONOMYHISTORY_TEXT_128 0xaa1061baa4db7c3aull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogEconomyHistory);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, EconomyChronicleRules InRules = EconomyChronicleRules{})
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
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Wealth = WealthTypes::Declare(Instance);
			Chronicle = EconomyChronicleTypes::Declare(Instance);
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
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Purses = std::make_unique<WealthSystem>(Instance, Ages.Types(), Persons, Families, Economy, Markets, Norms,
													Wealth, WealthRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Ranks->RunAfter("Wealth");
			Ranks->ObserveWealth(Wealth.Wealth);
			Stocks->ObserveHeirs(Wealth.Heir);
			Context = EconomyContext{Persons, Families, Trade, Markets, MarketRules{}};
			Scribe = std::make_unique<EconomyChronicle>(Instance, Ages.Types(), Context, Chronicle, InRules);
			Scribe->Attach();
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveStores(Stores);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Purses.get());
			Instance.Systems().Add(Ranks.get());
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
		WealthStats Stats(uint32 Region = 0) const { return MeasureWealth(Instance, Families, Wealth, Region); }
		EconomyChronicleStats Told() const { return CheckEconomyChronicle(Instance, Ages.Types(), Context, Chronicle); }
		std::string Line(const Event& E) const
		{
			std::string Out;
			DescribeEconomyEvent(Instance, Ages.Types(), Context, E, Out);
			return Out;
		}
		TradeStats Roads_() const { return MeasureTrade(Instance, Ages.Types(), Trade, TradeRules{}); }
		MarketStats Fairs() const { return MeasureMarkets(Instance, Ages.Types(), Markets, 0); }
		ProductionStats Fields() const { return MeasureProduction(Instance, Ages.Types(), Production, 0); }
		/// The whole world's goods: every region's commons and its houses.
		void WorldStock(uint64 Out[GoodCount]) const
		{
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				Out[g] = 0;
			}
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle, const RegionInfo& R)
					{
						uint32 Here[GoodCount];
						TotalStock(Instance, Ages.Types(), Families, Economy, R.Index, Here);
						for (uint32 g = 0; g < GoodCount; ++g)
						{
							Out[g] += Here[g];
						}
					});
		}
		/// House stocks held by a house that is extinct or whose region is coarse.
		uint32 Orphans() const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle H, const FamilyInfo& F)
					{
						if (Instance.Components().GetPool(Economy.House).TryGet(H) == nullptr)
						{
							return;
						}
						N += F.Extinct != 0 || !IsDetailed(Instance, Ages.Types(), Persons, F.Region) ? 1u : 0u;
					});
			return N;
		}
		std::vector<uint32> Ranked() const
		{
			std::vector<std::pair<uint32, uint32>> All;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && P->Total > 0)
						{
							All.push_back({P->Total, R.Index});
						}
					});
			std::sort(All.begin(), All.end(), [](const auto& A, const auto& B)
					  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
			std::vector<uint32> Out;
			for (const auto& [People, Index] : All)
			{
				Out.push_back(Index);
			}
			return Out;
		}
		void Total(uint32 Region, uint32 Out[GoodCount]) const
		{
			TotalStock(Instance, Ages.Types(), Families, Economy, Region, Out);
		}
		const Society::HouseWealth* Purse(uint32 Family) const { return WealthOf(Instance, Families, Wealth, Family); }
		const HouseHeir* Heir(uint32 Family) const { return HeirOf(Instance, Families, Wealth, Family); }
		const FamilyInfo* House(uint32 Family) const
		{
			const FamilyInfo* Found = nullptr;
			Instance.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle, const FamilyInfo& F)
					{
						if (F.Index == Family && Found == nullptr)
						{
							Found = &F;
						}
					});
			return Found;
		}
		/// Living houses of a region, in index order.
		std::vector<uint32> HousesOf(uint32 Region) const
		{
			std::vector<uint32> Out;
			Instance.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle, const FamilyInfo& F)
					{
						if (F.Region == Region && F.Extinct == 0)
						{
							Out.push_back(F.Index);
						}
					});
			std::sort(Out.begin(), Out.end());
			return Out;
		}
		/// Every culture's descent custom set to one line.
		void DescendBy(Descent Line)
		{
			Instance.Components()
				.GetPool(Ages.Types().Population.Culture)
				.ForEach(
					[&](EntityHandle, const CultureInfo& C)
					{
						const NormSet* N = NormsOf(Instance, Ages.Types(), Norms, C.Index);
						if (N != nullptr)
						{
							NormSet Copy = *N;
							Copy.Descent_ = static_cast<uint32>(Line);
							SetNorms(Instance, Ages.Types(), Norms, C.Index, Copy);
						}
					});
		}
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
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		WealthTypes Wealth;
		EconomyChronicleTypes Chronicle;
		EconomyContext Context;
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
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<WealthSystem> Purses;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<EconomyChronicle> Scribe;
	};

} // namespace

VAELEN_TEST(EconomyHistory, EveryEconomicEventHasItsOwnLine)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(10);
	std::map<uint64, uint32> Seen;
	uint32 Generic = 0;
	uint32 BadPrefix = 0;
	uint32 Shown = 0;
	std::string Other;
	for (const Event& E : W.Instance.Log().All())
	{
		const std::string L = W.Line(E);
		const bool Economic = E.Is(RouteOpenedEvent) || E.Is(RouteClosedEvent) || E.Is(GoodsCarriedEvent) ||
							  E.Is(SettlementFoundedEvent) || E.Is(SettlementAbandonedEvent) ||
							  E.Is(PriceChangedEvent) || E.Is(HarvestEvent) || E.Is(ShortfallEvent) ||
							  E.Is(FortuneChangedEvent) || E.Is(HeirNamedEvent) || E.Is(StockInheritedEvent) ||
							  E.Is(StockReturnedEvent) || E.Is(StockSplitEvent) || E.Is(StockFoldedEvent) ||
							  E.Is(StockEndowedEvent) || E.Is(StockAddedEvent) || E.Is(StockTakenEvent);
		if (!Economic)
		{
			Other.clear();
			DescribePersonEvent(W.Instance, W.Ages.Types(), W.Persons, W.Families, E, Other);
			VT_CHECK(L == Other); // the lower layers keep their own words
			continue;
		}
		++Seen[E.TypeHash];
		Generic += L.find("something happened") != std::string::npos ? 1u : 0u;
		BadPrefix += L.rfind("Year ", 0) == 0 && L.find(": ") != std::string::npos ? 0u : 1u;
		VT_CHECK(L.find("person ") == std::string::npos && L.find("family ") == std::string::npos);
		if (Shown < 10 && (E.Is(RouteOpenedEvent) || E.Is(SettlementFoundedEvent) || E.Is(ShortfallEvent) ||
						   E.Is(FortuneChangedEvent) || E.Is(StockInheritedEvent) ||
						   (E.Is(PriceChangedEvent) && E.Get<StockPayload>().Good == 0)))
		{
			++Shown;
			VAELEN_LOG_INFO(LogEconomyHistory, "%s", L.c_str());
		}
	}
	VT_CHECK_EQ(Generic, 0u);
	VT_CHECK_EQ(BadPrefix, 0u);
	VT_CHECK(Seen.size() >= 6); // harvests, prices, roads, carries, splits, fortunes at least
	VT_CHECK(Seen[HarvestEvent.TypeHash] > 100);
	VT_CHECK(Seen[PriceChangedEvent.TypeHash] > 0 && Seen[StockSplitEvent.TypeHash] > 0);
	// Names, and the fallbacks for what no longer exists.
	std::string Name;
	NameRoute(W.Instance, W.Ages.Types(), W.Context, 0xfffffff0u, Name);
	VT_CHECK(Name == "road 4294967280");
	Name.clear();
	NameSettlement(W.Instance, W.Ages.Types(), W.Context, 0xfffffff0u, Name);
	VT_CHECK(Name == "town 4294967280");
	std::vector<RouteInfo> Routes;
	RoutesOf(W.Instance, W.Trade, Region, Routes);
	if (!Routes.empty())
	{
		Name.clear();
		NameRoute(W.Instance, W.Ages.Types(), W.Context, Routes[0].Index, Name);
		VT_CHECK(Name.rfind("the road from ", 0) == 0 && Name.find(" to ") != std::string::npos);
	}
}

VAELEN_TEST(EconomyHistory, OnlyWhatMattersIsRecordedAndTheWhyReachesTheDrought)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(10);
	const EconomyChronicleStats S = W.Told();
	uint32 Harvests = 0;
	uint32 Carries = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Harvests += E.Is(HarvestEvent) ? 1u : 0u;
		Carries += E.Is(GoodsCarriedEvent) ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogEconomyHistory,
					"%u records (%u dropped) of %u described: %u roads, %u towns, %u prices, %u shortfalls, %u "
					"fortunes, %u inheritances; %u harvests and %u carries left out",
					S.Records, S.Dropped, S.Described, S.ByType[0], S.ByType[1], S.ByType[2], S.ByType[3], S.ByType[4],
					S.ByType[5], Harvests, Carries);
	VT_CHECK(S.Records > 0);
	VT_CHECK_EQ(S.Described, S.Records);
	VT_CHECK_EQ(S.EraConsistent, S.Described);
	VT_CHECK(Harvests > 1000 && Carries > 0);
	VT_CHECK(S.Records < Harvests / 4); // a harvest a region a year is not history
	// Every recorded price is at a bound; every recorded fortune really moved.
	const MarketRules Prices;
	uint32 Middling = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().History.Record)
		.ForEach(
			[&](EntityHandle, const RecordInfo& R)
			{
				const Event* E = FindEvent(W.Instance.Log(), PersistentId{R.Event});
				if (E == nullptr || !E->Is(PriceChangedEvent))
				{
					return;
				}
				const StockPayload P = E->Get<StockPayload>();
				const uint32 Floor = std::max<uint32>(1u, Prices.BasePrice[P.Good] * Prices.FloorPerMille / 1000u);
				const uint32 Ceiling = Prices.BasePrice[P.Good] * Prices.CeilingPerMille / 1000u;
				Middling += P.Amount > Floor && P.Amount < Ceiling ? 1u : 0u;
			});
	VT_CHECK_EQ(Middling, 0u);
	// A rule that records nothing records nothing.
	EconomyChronicleRules Silent;
	Silent.RecordRoutes = 0;
	Silent.RecordSettlements = 0;
	Silent.RecordExtremePrices = 0;
	Silent.RecordShortfalls = 0;
	Silent.RecordFortunes = 0;
	Silent.RecordInheritances = 0;
	Run Q(AelvorSeed, Silent);
	VT_REQUIRE(Q.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(Q.Instance, Q.Lod, Region));
	Q.Ages.Run(10);
	VT_CHECK_EQ(Q.Told().Records, 0u);
	// The why of a dear loaf: the price, the harvest under it, the drought under that.
	Run X(AelvorSeed);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	const RegionStock* Common = StockOf(X.Instance, X.Ages.Types(), X.Economy, Region);
	VT_REQUIRE(Common != nullptr);
	VT_REQUIRE(X.Curse(Region, DisasterKind::Drought));
	AddStock(X.Instance, X.Ages.Types(), X.Families, X.Economy, Region, 0, Good::Grain,
			 -static_cast<int32>(Common->Amount[0]), X.Instance.Now());
	X.Ages.Run(1);
	const Event* Price = nullptr;
	for (const Event& E : X.Instance.Log().All())
	{
		if (E.Is(PriceChangedEvent) && E.Get<StockPayload>().Region == Region &&
			E.Get<StockPayload>().Good == static_cast<uint32>(Good::Grain) && E.Cause.IsValid())
		{
			Price = &E;
		}
	}
	VT_REQUIRE(Price != nullptr);
	std::string Why;
	const uint32 Lines = ExportWhyWithEconomy(X.Instance, X.Ages.Types(), X.Context, Price->Id, Why);
	VAELEN_LOG_INFO(LogEconomyHistory, "why a loaf grew dear (%u lines):\n%s", Lines, Why.c_str());
	VT_CHECK(Lines >= 3); // the price, the harvest, the drought
	VT_CHECK(Why.find("because") != std::string::npos);
	VT_CHECK(Why.find("harvested") != std::string::npos);
	VT_CHECK(Why.find("drought") != std::string::npos);
}

VAELEN_TEST(EconomyHistory, DeterministicSnapshotSafeAndFrozen)
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
	const EconomyChronicleStats S = A.Told();
	std::string Text;
	const uint32 Lines = ExportChronicleWithEconomy(A.Instance, A.Ages.Types(), A.Context, Text);
	const Hash64 Digest = HashBytes(Text.data(), Text.size());
	VAELEN_LOG_INFO(LogEconomyHistory, "frozen: economyhistory128 records=%u text=%016llx (%u lines, %u dropped)",
					S.Records, static_cast<unsigned long long>(Digest), Lines, S.Dropped);
	VT_CHECK(Lines > 0);
	VT_CHECK_EQ(S.Records, uint32{VAELEN_ECONOMYHISTORY_RECORDS_128});
	VT_CHECK_EQ(Digest, Hash64{VAELEN_ECONOMYHISTORY_TEXT_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	std::string Again;
	ExportChronicleWithEconomy(R.Instance, R.Ages.Types(), R.Context, Again);
	VT_CHECK_EQ(HashBytes(Again.data(), Again.size()), Digest);
}
