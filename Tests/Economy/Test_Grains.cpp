// VAELEN - Tests/Economy
// Phase 06.06: the economy across the grains - a still world whose goods are
// conserved to the unit through five hundred years of promotions and
// demotions, and a living one whose every economic invariant holds through
// the same.
//
// STATUS: VALIDATED (Phase 06)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
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
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.06): AELVOR 64 at
// year 120, three regions detailed in turn every 25 years for 500 years.
#define VAELEN_GRAINS_STILL_64 0xace29fbb38633cbfull
// Refrozen 2026-09-08: person indices are taken from a counter that only
// goes up, so a demoted region no longer hands its indices out again (see
// PersonCounter).
#define VAELEN_GRAINS_LIVING_64 0x3262bff481851b8dull
#define VAELEN_GRAINS_PROMOTIONS_64 21u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogGrains);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, bool Living = true) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
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
			if (Living)
			{
				Stocks->ObserveHeirs(Wealth.Heir);
			}
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
			// A still world holds only the owners of goods: nothing is made, eaten,
			// priced, carried or inherited, so the world's whole stock can only be
			// moved between a region's commons and its houses.
			if (Living)
			{
				Instance.Systems().Add(Minds.get());
				Instance.Systems().Add(Body.get());
				Instance.Systems().Add(Harvest.get());
				Instance.Systems().Add(Fair.get());
				Instance.Systems().Add(Roads.get());
				Instance.Systems().Add(Orgs.get());
				Instance.Systems().Add(Customs.get());
				Instance.Systems().Add(Purses.get());
				Instance.Systems().Add(Ranks.get());
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
		WealthStats Stats(uint32 Region = 0) const { return MeasureWealth(Instance, Families, Wealth, Region); }
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
	};

	/// Alternation: every 25 years another of the three busiest regions rests.
	void Want(Run& W, const std::vector<uint32>& Ranked, uint32 Year)
	{
		if (Year % 25 != 1)
		{
			return;
		}
		const uint32 Turn = (Year / 25) % 3;
		for (uint32 i = 0; i < 3; ++i)
		{
			if (i == Turn)
			{
				ReleaseDetail(W.Instance, W.Lod, Ranked[i]);
			}
			else
			{
				RequestDetail(W.Instance, W.Lod, Ranked[i]);
			}
		}
	}
} // namespace

VAELEN_TEST(Grains, AStillWorldKeepsEveryUnitThroughFiveHundredYearsOfGrainChanges)
{
	Run W(AelvorSeed, false);
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 120));
	W.Ages.Run(1); // the land endows every region once
	uint64 Start[GoodCount];
	W.WorldStock(Start);
	VT_REQUIRE(Start[0] > 0);
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 3);
	uint32 Failures = 0;
	uint32 Split = 0;
	for (uint32 Year = 1; Year <= 500; ++Year)
	{
		Want(W, Ranked, Year);
		W.Ages.Run(1);
		if (Year % 25 != 0)
		{
			continue;
		}
		uint64 Now[GoodCount];
		W.WorldStock(Now);
		const StockStats S = W.Stock();
		Split = std::max(Split, S.HousesWithStock);
		for (uint32 g = 0; g < GoodCount; ++g)
		{
			if (Now[g] != Start[g])
			{
				++Failures;
				VT_CHECK_MSG(false, "year %u: %s went from %llu to %llu", Year, GoodName(static_cast<Good>(g)),
							 static_cast<unsigned long long>(Start[g]), static_cast<unsigned long long>(Now[g]));
			}
		}
		if (S.Stale != 0 || W.Orphans() != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale, %u orphaned house stocks", Year, S.Stale, W.Orphans());
		}
	}
	const LodStats L = MeasureLod(W.Instance, W.Ages.Types(), W.Persons, W.Lod);
	const StockStats S = W.Stock();
	VAELEN_LOG_INFO(LogGrains,
					"still world, 500 years: %u promotions, %u demotions, %u splits, %u folds, %u returns; %llu grain "
					"from first to last; digest %016llx",
					L.Promotions, L.Demotions, S.Splits, S.Folds, S.Returns, static_cast<unsigned long long>(Start[0]),
					static_cast<unsigned long long>(S.Digest));
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK(L.Promotions >= 20 && L.Demotions + 2 >= L.Promotions); // the grain really changed, many times
	VT_CHECK(S.Splits >= L.Promotions);								 // every promotion split the commons
	VT_CHECK(S.Folds >= L.Demotions - 1u);							 // and every demotion folded them back
	VT_CHECK(Split > 0);
	VT_CHECK_EQ(S.Inheritances, 0u); // no heirs in a still world
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_GRAINS_STILL_64});
	VT_CHECK_EQ(L.Promotions, uint32{VAELEN_GRAINS_PROMOTIONS_64});
}

VAELEN_TEST(Grains, ALivingWorldHoldsEveryEconomicInvariantThroughTheSame)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const std::vector<uint32> Ranked = A.Ranked();
	VT_REQUIRE(Ranked.size() >= 3);
	const MarketRules Prices;
	uint32 Failures = 0;
	uint32 MostInherited = 0;
	std::vector<uint8> Image;
	for (uint32 Year = 1; Year <= 500; ++Year)
	{
		Want(A, Ranked, Year);
		Want(B, Ranked, Year);
		A.Ages.Run(1);
		B.Ages.Run(1);
		if (Year == 250)
		{
			SaveSnapshot(A.Instance, Image);
		}
		if (Year % 25 != 0)
		{
			continue;
		}
		const StockStats S = A.Stock();
		const WealthStats P = A.Stats();
		const TradeStats T = A.Roads_();
		const MarketStats M = A.Fairs();
		const ProductionStats F = A.Fields();
		MostInherited = std::max(MostInherited, S.Inheritances);
		// Nothing of a coarse or dead house survives; every market is bounded;
		// every ration is a ration; the roads join markets that exist.
		uint32 Bad = S.Stale + P.Stale + T.Bad + A.Orphans();
		Bad += F.RationMin > 1000 ? 1u : 0u;
		for (uint32 g = 0; g < GoodCount; ++g)
		{
			const uint32 Floor = Prices.BasePrice[g] * Prices.FloorPerMille / 1000u;
			const uint32 Ceiling = Prices.BasePrice[g] * Prices.CeilingPerMille / 1000u;
			Bad += M.Markets > 0 && (M.Lowest[g] < Floor || M.Highest[g] > Ceiling) ? 1u : 0u;
		}
		// Every region holding a market holds a stock, and the other way round.
		VT_CHECK_EQ(M.Markets, S.RegionsWithStock);
		if (Bad != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale stocks, %u stale purses, %u bad roads, %u orphans, ration %u", Year,
						 S.Stale, P.Stale, T.Bad, A.Orphans(), F.RationMin);
		}
		if (ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: the two worlds differ", Year);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const StockStats S = A.Stock();
	const WealthStats P = A.Stats();
	const TradeStats T = A.Roads_();
	const LodStats L = MeasureLod(A.Instance, A.Ages.Types(), A.Persons, A.Lod);
	VAELEN_LOG_INFO(LogGrains,
					"living world, 500 years: %u promotions, %u splits, %u folds, %u returns, %u inheritances; %u "
					"routes open, %u settlements; %u houses valued, %u with an heir; digest %016llx",
					L.Promotions, S.Splits, S.Folds, S.Returns, S.Inheritances, T.RoutesOpen, T.Settlements, P.Valued,
					P.WithHeir, static_cast<unsigned long long>(S.Digest));
	VT_CHECK(L.Promotions >= 20);
	VT_CHECK(S.Splits >= L.Promotions);
	VT_CHECK(MostInherited > 0); // heirs did receive across the centuries
	VT_CHECK(T.RoutesOpen > 0);	 // trade survived the alternation
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_GRAINS_LIVING_64});
	// The snapshot of year 250 continues to the same year 500.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	for (uint32 Year = 251; Year <= 500; ++Year)
	{
		Want(R, Ranked, Year);
		R.Ages.Run(1);
	}
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stock().Digest, S.Digest);
}
