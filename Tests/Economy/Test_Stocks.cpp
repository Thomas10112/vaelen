// VAELEN - Tests/Economy
// Phase 06.01: goods and stocks - the land's endowment, the split at a
// promotion, the fold at a demotion, the return of an extinct house, moving
// stock by hand, conservation across a century.
//
// STATUS: VALIDATED (Phase 06)

#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.01): AELVOR 128 at
// year 300, the busiest region detailed, 100 years with lives, families, lod
// and stocks.
#define VAELEN_STOCKS_FROZEN_128 0xb96d3bc0a8ee4141ull
#define VAELEN_STOCKS_GRAIN_128 27581u
#define VAELEN_STOCKS_HOUSES_128 179u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogStocks);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, EconomyRules InRules = EconomyRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Economy = EconomyTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy, InRules);
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
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
		uint32 Regions() const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach([&](EntityHandle, const RegionInfo&) { ++N; });
			return N;
		}
		StockStats Stats(uint32 Region = 0) const
		{
			return MeasureStocks(Instance, Ages.Types(), Persons, Families, Economy, Region);
		}
		void Total(uint32 Region, uint32 Out[GoodCount]) const
		{
			TotalStock(Instance, Ages.Types(), Families, Economy, Region, Out);
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
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		LodTypes Lod;
		EconomyTypes Economy;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
	};

	bool SameTotals(const uint32 A[GoodCount], const uint32 B[GoodCount])
	{
		for (uint32 g = 0; g < GoodCount; ++g)
		{
			if (A[g] != B[g])
			{
				return false;
			}
		}
		return true;
	}

	uint32 Count(const World& W, EventType<StockPayload> Type)
	{
		uint32 N = 0;
		for (const Event& E : W.Log().All())
		{
			N += E.Is(Type) ? 1u : 0u;
		}
		return N;
	}
} // namespace

VAELEN_TEST(Stocks, TheLandEndowsEveryRegionOnceFromItsCapacityAndDeposits)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Regions = W.Regions();
	VT_REQUIRE(Regions > 8);
	// The pre-history ran the system: the endowment happened in its first year.
	W.Ages.Run(1);
	StockStats S = W.Stats();
	VT_CHECK_EQ(S.RegionsWithStock, Regions);
	VT_CHECK_EQ(S.Endowed, Regions);
	VT_CHECK_EQ(S.HousesWithStock, 0u); // no region is detailed
	VT_CHECK(S.Total[static_cast<uint32>(Good::Grain)] > 0);
	VT_CHECK_EQ(S.Total[static_cast<uint32>(Good::Cloth)], 0u); // nothing is made yet
	VT_CHECK_EQ(S.Total[static_cast<uint32>(Good::Tools)], 0u);
	// Grain follows the capacity; timber, ore and salt follow the deposits.
	uint32 Timbered = 0;
	uint32 Bare = 0;
	uint32 Wrong = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionStock* St = W.Instance.Components().GetPool(W.Economy.Region).TryGet(H);
				const RegionPopulation* P =
					W.Instance.Components().GetPool(W.Ages.Types().Population.Population).TryGet(H);
				if (St == nullptr)
				{
					++Wrong;
					return;
				}
				const uint64 Capacity = P != nullptr ? P->Capacity : 0u;
				Wrong += St->Amount[static_cast<uint32>(Good::Grain)] !=
								 Capacity * EconomyRules{}.EndowGrainPerCapacity / 1000u
							 ? 1u
							 : 0u;
				uint32 Richness = 0;
				W.Instance.Components()
					.GetPool(W.Ages.Types().World.DepositTypes_.Deposit)
					.ForEach(
						[&](EntityHandle, const DepositInfo& D) {
							Richness += D.Region == R.Index && D.Kind == static_cast<uint32>(ResourceKind::Timber)
											? D.Richness
											: 0u;
						});
				Wrong += St->Amount[static_cast<uint32>(Good::Timber)] != Richness ? 1u : 0u;
				Timbered += Richness > 0 ? 1u : 0u;
				Bare += Richness == 0 ? 1u : 0u;
			});
	VT_CHECK_EQ(Wrong, 0u);
	VT_CHECK(Timbered > 0 && Bare > 0); // both cases exist in AELVOR 128
	// Once: ten more years endow nothing and change nothing.
	const Hash64 Digest = S.Digest;
	W.Ages.Run(10);
	S = W.Stats();
	VT_CHECK_EQ(S.Endowed, Regions);
	VT_CHECK_EQ(S.Digest, Digest);
	VT_CHECK_EQ(S.Stale, 0u);
	VAELEN_LOG_INFO(LogStocks, "%u regions endowed: %u grain, %u timber, %u ore, %u salt, %u luxuries; %u with timber",
					Regions, S.Total[0], S.Total[4], S.Total[3], S.Total[5], S.Total[6], Timbered);
	// The names, and the fallbacks.
	for (uint32 g = 0; g < GoodCount; ++g)
	{
		VT_CHECK(GoodName(static_cast<Good>(g))[0] != '\0');
		for (uint32 h = 0; h < g; ++h)
		{
			VT_CHECK(std::string_view{GoodName(static_cast<Good>(g))} != GoodName(static_cast<Good>(h)));
		}
	}
	VT_CHECK(std::string_view{GoodName(Good::Count)} == "goods");
	VT_CHECK(StockOf(W.Instance, W.Ages.Types(), W.Economy, 0xfffffff0u) == nullptr);
	VT_CHECK(HouseStockOf(W.Instance, W.Families, W.Economy, 0xfffffff0u) == nullptr);
	// The same world again gives the same stocks.
	Run X(AelvorSeed);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	X.Ages.Run(11);
	VT_CHECK_EQ(X.Stats().Digest, S.Digest);
}

VAELEN_TEST(Stocks, APromotionSplitsAndADemotionFoldsWithoutLoss)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Ages.Run(1);
	const uint32 Region = W.Busiest();
	uint32 Before[GoodCount];
	W.Total(Region, Before);
	VT_REQUIRE(Before[0] > 100);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	VT_REQUIRE(IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	const std::vector<uint32> Houses = W.HousesOf(Region);
	VT_REQUIRE(Houses.size() >= 10);
	// Every living house holds a stock; the houses hold the share, the rest is common.
	StockStats S = W.Stats(Region);
	VT_CHECK_EQ(S.HousesWithStock, static_cast<uint32>(Houses.size()));
	VT_CHECK_EQ(S.Splits, 1u);
	VT_CHECK_EQ(S.Stale, 0u);
	uint32 After[GoodCount];
	W.Total(Region, After);
	VT_CHECK(SameTotals(Before, After));
	const uint32 Held = S.Total[0] - S.Common[0];
	VT_CHECK(Held > 0 && Held <= Before[0] * EconomyRules{}.HouseSharePerMille / 1000u);
	VT_CHECK(S.Common[0] >= Before[0] - Before[0] * EconomyRules{}.HouseSharePerMille / 1000u);
	uint32 WithGrain = 0;
	for (const uint32 H : Houses)
	{
		const HouseStock* St = HouseStockOf(W.Instance, W.Families, W.Economy, H);
		VT_REQUIRE(St != nullptr);
		WithGrain += St->Amount[0] > 0 ? 1u : 0u;
	}
	VT_CHECK(WithGrain > Houses.size() / 2); // most houses have members, so grain
	const Event* Split = nullptr;
	for (const Event& E : W.Instance.Log().All())
	{
		Split = E.Is(StockSplitEvent) ? &E : Split;
	}
	VT_REQUIRE(Split != nullptr);
	VT_CHECK_EQ(Split->Get<StockPayload>().House, static_cast<uint32>(Houses.size()));
	VT_CHECK_EQ(Split->Get<StockPayload>().Amount, Held + (S.Total[4] - S.Common[4]) + (S.Total[3] - S.Common[3]) +
													   (S.Total[5] - S.Common[5]) + (S.Total[6] - S.Common[6]));
	VAELEN_LOG_INFO(LogStocks, "region %u: %u grain, %u held by %u houses after the split, %u in common", Region,
					Before[0], Held, static_cast<uint32>(Houses.size()), S.Common[0]);
	// Thirty years: houses come and go, the whole is conserved, extinct houses return theirs.
	for (uint32 Year = 1; Year <= 30; ++Year)
	{
		W.Ages.Run(1);
		W.Total(Region, After);
		VT_CHECK_MSG(SameTotals(Before, After), "year %u: the region's whole changed", Year);
		VT_CHECK_EQ(W.Stats(Region).Stale, 0u);
	}
	S = W.Stats(Region);
	VT_CHECK(S.Returns > 0);								  // some house went extinct holding goods
	VT_CHECK(S.HousesWithStock >= W.HousesOf(Region).size()); // every living house, and only living houses (Stale 0)
	VT_CHECK_EQ(S.HousesWithStock, static_cast<uint32>(W.HousesOf(Region).size()));
	// A demotion folds everything back into the common stock.
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	VT_CHECK(!IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	S = W.Stats(Region);
	VT_CHECK_EQ(S.HousesWithStock, 0u);
	VT_CHECK_EQ(S.Folds, 1u);
	VT_CHECK_EQ(S.Stale, 0u);
	W.Total(Region, After);
	VT_CHECK(SameTotals(Before, After));
	VT_CHECK(SameTotals(S.Common, After));
	// Detailed again: split again, among the new houses.
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	S = W.Stats(Region);
	VT_CHECK_EQ(S.Splits, 2u);
	VT_CHECK(S.HousesWithStock >= 10);
	W.Total(Region, After);
	VT_CHECK(SameTotals(Before, After));
	VT_CHECK_EQ(Count(W.Instance, StockFoldedEvent), 1u);
}

VAELEN_TEST(Stocks, MovingStockByHandIsClampedAndCaused)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	W.Ages.Run(1);
	const uint32 Region = W.Busiest();
	const SimTick Now = W.Instance.Now();
	const RegionStock* Common = StockOf(W.Instance, W.Ages.Types(), W.Economy, Region);
	VT_REQUIRE(Common != nullptr);
	const uint32 Grain = Common->Amount[0];
	// The common stock: add, take, take more than there is, unknown region and good.
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Cloth, 100, Now), 100u);
	VT_CHECK_EQ(Common->Amount[static_cast<uint32>(Good::Cloth)], 100u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Cloth, -30, Now), 30u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Cloth, -1000, Now), 70u);
	VT_CHECK_EQ(Common->Amount[static_cast<uint32>(Good::Cloth)], 0u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Cloth, -1, Now), 0u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Cloth, 0, Now), 0u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, 0xfffffff0u, 0, Good::Cloth, 5, Now), 0u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Count, 5, Now), 0u);
	VT_CHECK_EQ(Common->Amount[0], Grain); // untouched
	// Saturation at the top.
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Salt, 0x7fffffff, Now),
				0x7fffffffu);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Salt, 0x7fffffff, Now),
				0x7fffffffu);
	const uint32 SaltNow = Common->Amount[static_cast<uint32>(Good::Salt)];
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Salt, 0x7fffffff, Now),
				0xffffffffu - SaltNow);
	VT_CHECK_EQ(Common->Amount[static_cast<uint32>(Good::Salt)], 0xffffffffu);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0, Good::Salt, 1, Now), 0u);
	// A house: unknown while coarse, then known, with the cause on the event.
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 1, Good::Tools, 5, Now), 0u);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(1);
	const std::vector<uint32> Houses = W.HousesOf(Region);
	VT_REQUIRE(!Houses.empty());
	const uint32 House = Houses[0];
	const Event& Cause = W.Instance.Log().All().back();
	const SimTick Later = W.Instance.Now();
	VT_CHECK_EQ(
		AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, House, Good::Tools, 50, Later, Cause.Id),
		50u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, House, Good::Tools, -20, Later),
				20u);
	const HouseStock* Held = HouseStockOf(W.Instance, W.Families, W.Economy, House);
	VT_REQUIRE(Held != nullptr);
	VT_CHECK_EQ(Held->Amount[static_cast<uint32>(Good::Tools)], 30u);
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region + 1, House, Good::Tools, 5, Later),
				0u); // a house is known by its home region
	VT_CHECK_EQ(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, 0xfffffff0u, Good::Tools, 5, Later),
				0u);
	uint32 Total[GoodCount];
	W.Total(Region, Total);
	VT_CHECK_EQ(Total[static_cast<uint32>(Good::Tools)], 30u);
	const StockStats S = W.Stats(Region);
	VT_CHECK_EQ(S.Added, 5u);
	VT_CHECK_EQ(S.Taken, 3u); // taking nothing is no event
	uint32 Caused = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(StockAddedEvent) && E.Get<StockPayload>().House == House)
		{
			VT_CHECK(E.Cause == Cause.Id);
			VT_CHECK(E.Subject.IsValid());
			VT_CHECK_EQ(E.Get<StockPayload>().Good, static_cast<uint32>(Good::Tools));
			VT_CHECK_EQ(E.Get<StockPayload>().Amount, 50u);
			++Caused;
		}
	}
	VT_CHECK_EQ(Caused, 1u);
}

VAELEN_TEST(Stocks, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(1);
	B.Ages.Run(1);
	uint32 Start[GoodCount];
	A.Total(0, Start); // the whole world's endowment
	uint32 Failures = 0;
	std::vector<uint8> Image;
	for (uint32 Year = 2; Year <= 100; ++Year)
	{
		A.Ages.Run(1);
		B.Ages.Run(1);
		if (Year == 50)
		{
			SaveSnapshot(A.Instance, Image);
		}
		if (Year % 10 != 0)
		{
			continue;
		}
		const StockStats S = A.Stats();
		uint32 Now[GoodCount];
		A.Total(0, Now);
		if (S.Stale != 0 || !SameTotals(Start, Now) || ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale, conserved %u, worlds %s", Year, S.Stale,
						 SameTotals(Start, Now) ? 1u : 0u,
						 ComputeStateDigest(A.Instance) == ComputeStateDigest(B.Instance) ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const StockStats S = A.Stats();
	VAELEN_LOG_INFO(LogStocks, "frozen: stocks128=%016llx grain=%u houses=%u (%u regions, %u splits, %u returns)",
					static_cast<unsigned long long>(S.Digest), S.Total[0], S.HousesWithStock, S.RegionsWithStock,
					S.Splits, S.Returns);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_STOCKS_FROZEN_128});
	VT_CHECK_EQ(S.Total[0], uint32{VAELEN_STOCKS_GRAIN_128});
	VT_CHECK_EQ(S.HousesWithStock, uint32{VAELEN_STOCKS_HOUSES_128});
	// The snapshot of year 50 continues to the same year 100.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, S.Digest);
}
