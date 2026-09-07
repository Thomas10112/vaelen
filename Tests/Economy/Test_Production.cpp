// VAELEN - Tests/Economy
// Phase 06.02: production and consumption - the harvest, the meals, the
// ration the need system observes, the deposits and the craft, the two grains
// agreeing, a century frozen.
//
// STATUS: VALIDATED (Phase 06)

#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/Deposits.h"
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

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.02): AELVOR 128 at
// year 300, the busiest region detailed, 100 years with lives, families,
// traits, needs, lod, stocks and production.
#define VAELEN_PRODUCTION_STOCKS_128 0x2e21806d2979bc15ull
#define VAELEN_PRODUCTION_RATIONS_128 0x06b87708f4c215efull
#define VAELEN_PRODUCTION_GRAIN_128 23070167ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogProduction);

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
		ProductionStats Stats(uint32 Region = 0) const
		{
			return MeasureProduction(Instance, Ages.Types(), Production, Region);
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
		ComponentType<RegionStores> Stores;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
	};
} // namespace

VAELEN_TEST(Production, CoarseRegionsHarvestEatAndKeepABoundedStore)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	// Every peopled region harvested last year, is rationed and holds a bounded store.
	uint32 Peopled = 0;
	uint32 NoRation = 0;
	uint32 Unbounded = 0;
	uint32 Timbered = 0;
	uint32 Short = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				const RegionPopulation* P =
					W.Instance.Components().GetPool(W.Ages.Types().Population.Population).TryGet(H);
				if (P == nullptr || P->Total == 0)
				{
					return;
				}
				++Peopled;
				const RegionRation* Rt = W.Instance.Components().GetPool(W.Production.Ration).TryGet(H);
				NoRation += Rt == nullptr ? 1u : 0u;
				Short += Rt != nullptr && Rt->PerMille < 1000 ? 1u : 0u;
				const RegionStock* St = W.Instance.Components().GetPool(W.Economy.Region).TryGet(H);
				const uint64 Last = W.Harvested(R.Index, 1);
				Unbounded += St != nullptr && St->Amount[0] > Last * 12u + 100u ? 1u : 0u;
				Timbered += St != nullptr && St->Amount[static_cast<uint32>(Good::Timber)] > 0 ? 1u : 0u;
			});
	VT_REQUIRE(Peopled > 8);
	const ProductionStats S = W.Stats();
	uint32 LastYear = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		LastYear += E.Is(HarvestEvent) && E.Tick + TicksPerYear >= W.Instance.Now() ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogProduction,
					"year 300: %u peopled regions, %u harvests last year, %u rationed (min %u), %u timbered, %u "
					"shortfalls in 300 years, %llu grain harvested",
					Peopled, LastYear, S.Rationed, S.RationMin, Timbered, S.Shortfalls,
					static_cast<unsigned long long>(S.Grain));
	VT_CHECK_EQ(LastYear, Peopled);
	VT_CHECK_EQ(NoRation, 0u);
	VT_CHECK_EQ(Unbounded, 0u);
	VT_CHECK(Timbered > 0);
	VT_CHECK(S.Grain > 0 && S.Harvests >= 300u * 8u);
	VT_CHECK(Short < Peopled / 2); // most regions feed themselves; the crowded do not
	VT_CHECK(S.Cut > 0);		   // the pre-history's droughts cut harvests
	// A drought cuts the busiest region's harvest and is the harvest's cause.
	const uint32 Region = W.Busiest();
	const uint64 Before = W.Harvested(Region, 1);
	VT_REQUIRE(Before > 0);
	VT_REQUIRE(W.Curse(Region, DisasterKind::Drought));
	W.Ages.Run(1);
	const uint64 After = W.Harvested(Region, 1);
	const ProductionStats R = W.Stats(Region);
	VAELEN_LOG_INFO(LogProduction, "region %u: harvest %llu, then %llu under a drought (%u harvests cut)", Region,
					static_cast<unsigned long long>(Before), static_cast<unsigned long long>(After), R.Cut);
	VT_CHECK(After <= Before * 75u / 100u);
	VT_CHECK(R.Cut >= 1);
	const Event& Last = W.Instance.Log().All().back();
	bool Found = false;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(HarvestEvent) && E.Tick == Last.Tick && E.Get<StockPayload>().Region == Region)
		{
			const Event* Cause = FindEvent(W.Instance.Log(), E.Cause);
			Found = Cause != nullptr && Cause->Is(DisasterStruckEvent);
		}
	}
	VT_CHECK(Found);
	VT_CHECK(RationOf(W.Instance, W.Ages.Types(), W.Production, 0xfffffff0u) == nullptr);
}

VAELEN_TEST(Production, ADetailedRegionFeedsItsHousesAndTheRationReachesTheNeeds)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	VT_REQUIRE(IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	const RegionRation* Fed = RationOf(W.Instance, W.Ages.Types(), W.Production, Region);
	VT_REQUIRE(Fed != nullptr);
	const NeedStats Well = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	const ProductionStats S = W.Stats(Region);
	VAELEN_LOG_INFO(LogProduction, "region %u detailed: ration %u, %u hungry of %u, %llu harvested in 3 years", Region,
					Fed->PerMille, Well.Hungry, Well.WithNeeds,
					static_cast<unsigned long long>(W.Harvested(Region, 3)));
	VT_CHECK(Fed->PerMille >= 900);
	VT_CHECK_EQ(S.Harvests, 3u + 300u);
	VT_CHECK(Well.Hungry < Well.WithNeeds / 10);
	// The harvest went to the houses: those with members hold grain after the meals.
	uint32 Holding = 0;
	uint32 Peopled = 0;
	W.Instance.Components()
		.GetPool(W.Families.Family)
		.ForEach(
			[&](EntityHandle H, const FamilyInfo& F)
			{
				const HouseStock* St = W.Instance.Components().GetPool(W.Economy.House).TryGet(H);
				if (F.Region != Region || F.Extinct != 0 || St == nullptr)
				{
					return;
				}
				++Peopled;
				Holding += St->Amount[0] > 0 ? 1u : 0u;
			});
	VT_CHECK(Peopled > 10 && Holding > Peopled / 2);
	// A people needing more than the land yields: the ration falls, the shortfalls
	// are logged, the people go hungry and starve, and the deaths say so.
	ProductionRules Poor;
	Poor.GrainPerPerson = 7; // the busiest region lives past its capacity: this much more is hunger
	Run X(AelvorSeed, Poor);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	X.Ages.Run(3); // the need system empties a hungry region within a few years (04.04)
	W.Ages.Run(3);
	const RegionRation* Thin = RationOf(X.Instance, X.Ages.Types(), X.Production, Region);
	VT_REQUIRE(Thin != nullptr);
	const NeedStats Hungry = MeasureNeeds(X.Instance, X.Persons, X.Needs, Region);
	const NeedStats Still = MeasureNeeds(W.Instance, W.Persons, W.Needs, Region);
	const ProductionStats P = X.Stats(Region);
	VAELEN_LOG_INFO(LogProduction, "a poor land: ration %u, %u shortfalls, %u hungry of %u, %u starved (%u well fed)",
					Thin->PerMille, P.Shortfalls, Hungry.Hungry, Hungry.WithNeeds, Hungry.StarvationDeaths,
					Still.StarvationDeaths);
	VT_CHECK(Thin->PerMille < 700);
	VT_CHECK(P.Shortfalls >= 3);
	VT_CHECK(Hungry.WithNeeds > 100 && Hungry.Hungry > Hungry.WithNeeds / 2);
	VT_CHECK(Hungry.StarvationDeaths > 0);
	VT_CHECK_EQ(Hungry.FamineDeaths, 0u); // no drought: starvation, not famine
	VT_CHECK_EQ(Still.StarvationDeaths, 0u);
	VT_CHECK(Hungry.FoodSum * Still.WithNeeds < Still.FoodSum * Hungry.WithNeeds); // less food a head
	// A council's granary: that share of the harvest reaches the common stock.
	Run G(AelvorSeed);
	VT_REQUIRE(G.Ages.Generate(Run::Square(128), 300));
	Run H(AelvorSeed);
	VT_REQUIRE(H.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(G.Instance, G.Lod, Region));
	VT_CHECK(RequestDetail(H.Instance, H.Lod, Region));
	G.Instance.Components().GetPool(G.Stores).Add(G.RegionHandle(Region), RegionStores{400, 0});
	G.Ages.Run(1);
	H.Ages.Run(1);
	const StockStats WithGranary = G.Stock(Region);
	const StockStats Without = H.Stock(Region);
	VAELEN_LOG_INFO(LogProduction, "granary: %u grain in common with a council keeping grain, %u without",
					WithGranary.Common[0], Without.Common[0]);
	VT_CHECK(WithGranary.Common[0] > Without.Common[0]);
	VT_CHECK_EQ(G.Harvested(Region, 1), H.Harvested(Region, 1)); // the same harvest, kept differently
}

VAELEN_TEST(Production, TheTwoGrainsAgreeAndLuxuriesAreNeitherMadeNorUsed)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = A.Busiest();
	const uint32 Luxuries = A.Stock().Total[static_cast<uint32>(Good::Luxuries)];
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(10);
	B.Ages.Run(10);
	const uint64 Coarse = A.Harvested(Region, 10);
	const uint64 Fine = B.Harvested(Region, 10);
	VAELEN_LOG_INFO(LogProduction, "region %u, ten years: %llu harvested coarse, %llu detailed", Region,
					static_cast<unsigned long long>(Coarse), static_cast<unsigned long long>(Fine));
	VT_CHECK(Fine >= Coarse * 70u / 100u && Fine <= Coarse * 130u / 100u);
	VT_CHECK_EQ(A.Stock().Total[static_cast<uint32>(Good::Luxuries)], Luxuries);
	VT_CHECK_EQ(B.Stock().Total[static_cast<uint32>(Good::Luxuries)], Luxuries);
	VT_CHECK_EQ(B.Stock().Stale, 0u);
	// Demoted again, the region harvests as a coarse one and the houses' grain is folded.
	VT_CHECK(ReleaseDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(10);
	B.Ages.Run(10);
	const uint64 CoarseAgain = A.Harvested(Region, 9);
	const uint64 Folded = B.Harvested(Region, 9);
	VT_CHECK(Folded >= CoarseAgain * 70u / 100u && Folded <= CoarseAgain * 130u / 100u);
	VT_CHECK_EQ(B.Stock(Region).HousesWithStock, 0u);
	VT_CHECK_EQ(B.Stock(Region).Folds, 1u);
}

VAELEN_TEST(Production, DeterministicSnapshotSafeAndFrozen)
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
		if (Year % 10 != 0)
		{
			continue;
		}
		const StockStats S = A.Stock();
		if (S.Stale != 0 || ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale, worlds %s", Year, S.Stale,
						 ComputeStateDigest(A.Instance) == ComputeStateDigest(B.Instance) ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const StockStats S = A.Stock();
	const ProductionStats P = A.Stats();
	VAELEN_LOG_INFO(LogProduction,
					"frozen: stocks=%016llx rations=%016llx grain=%llu (%u harvests, %u cut, %u shortfalls, %u "
					"rationed now)",
					static_cast<unsigned long long>(S.Digest), static_cast<unsigned long long>(P.Digest),
					static_cast<unsigned long long>(P.Grain), P.Harvests, P.Cut, P.Shortfalls, P.Rationed);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_PRODUCTION_STOCKS_128});
	VT_CHECK_EQ(P.Digest, Hash64{VAELEN_PRODUCTION_RATIONS_128});
	VT_CHECK_EQ(P.Grain, uint64{VAELEN_PRODUCTION_GRAIN_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stock().Digest, S.Digest);
	VT_CHECK_EQ(R.Stats().Digest, P.Digest);
}
