// VAELEN - Tests/Economy
// Phase 06.05: wealth and inheritance - houses valued and ranked at their
// market, the rank weighing in standing, heirs named by the descent custom,
// goods passing to the heir when a house dies out.
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

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.05): AELVOR 128 at
// year 300, the busiest region detailed, 100 years with every Phase 04 body,
// Phase 05 society and Phase 06 system so far.
#define VAELEN_WEALTH_FROZEN_128 0x8347935dd85ca3e9ull
#define VAELEN_WEALTH_HEIRS_128 4u
#define VAELEN_WEALTH_INHERITANCES_128 12u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogWealth);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, bool WithHeirs = true) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
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
			if (WithHeirs)
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
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
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
} // namespace

VAELEN_TEST(Wealth, HousesAreValuedAndRankedAtTheirMarket)
{
	// The score rule alone: wealth is worth its share and nothing more.
	const StandingRules Rules;
	PersonInfo P;
	P.Born = 0;
	const uint32 Poor = StandingScore(P, nullptr, 0, 0, uint64{TicksPerYear} * 40u, Rules, 0);
	const uint32 Rich = StandingScore(P, nullptr, 0, 0, uint64{TicksPerYear} * 40u, Rules, 255);
	VT_CHECK_EQ(Rich - Poor, 255u * Rules.WealthPointsPerMille / 1000u);
	VT_CHECK_EQ(StandingScore(P, nullptr, 0, 0, uint64{TicksPerYear} * 40u, Rules), Poor); // no observer, no weight

	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(2);
	const std::vector<uint32> Houses = W.HousesOf(Region);
	VT_REQUIRE(Houses.size() >= 10);
	const WealthStats S = W.Stats(Region);
	VT_CHECK_EQ(S.Valued, static_cast<uint32>(Houses.size()));
	VT_CHECK_EQ(S.Stale, 0u);
	VT_CHECK(S.Value > 0);
	// Every house's value is its goods at the region's prices; the ranks span the whole scale.
	const RegionMarket* M = W.Market(Region);
	VT_REQUIRE(M != nullptr);
	uint32 Lowest = 255;
	uint32 Highest = 0;
	uint32 Wrong = 0;
	for (const uint32 Family : Houses)
	{
		const Society::HouseWealth* Purse = W.Purse(Family);
		const HouseStock* Stock = HouseStockOf(W.Instance, W.Families, W.Economy, Family);
		VT_REQUIRE(Purse != nullptr && Stock != nullptr);
		Wrong += Purse->Value != ValueOf(Stock->Amount, *M) ? 1u : 0u;
		Lowest = std::min(Lowest, Purse->Rank);
		Highest = std::max(Highest, Purse->Rank);
	}
	VT_CHECK_EQ(Wrong, 0u);
	VT_CHECK_EQ(Lowest, 0u);
	VT_CHECK_EQ(Highest, 255u);
	// The richest is first, and matches the highest value.
	std::vector<uint32> Order;
	RichestOf(W.Instance, W.Families, W.Wealth, Region, Order);
	VT_REQUIRE(Order.size() == Houses.size());
	VT_CHECK_EQ(W.Purse(Order.front())->Value, S.Richest);
	VT_CHECK(W.Purse(Order.front())->Value >= W.Purse(Order.back())->Value);
	// A pile of goods lifts a house and is told: the poorest becomes the richest.
	const uint32 Pauper = Order.back();
	const uint32 Before = W.Purse(Pauper)->Rank;
	const uint32 Changes = W.Stats(Region).Changes;
	VT_CHECK(AddStock(W.Instance, W.Ages.Types(), W.Families, W.Economy, Region, Pauper, Good::Luxuries, 5000,
					  W.Instance.Now()) > 0);
	W.Ages.Run(1);
	VAELEN_LOG_INFO(LogWealth, "region %u: %u houses valued, richest %u, poorest house %u went from rank %u to %u",
					Region, S.Valued, S.Richest, Pauper, Before, W.Purse(Pauper)->Rank);
	VT_CHECK_EQ(W.Purse(Pauper)->Rank, 255u);
	VT_CHECK(W.Stats(Region).Changes > Changes);
	RichestOf(W.Instance, W.Families, W.Wealth, Region, Order);
	VT_CHECK_EQ(Order.front(), Pauper);
	// The lookups say nothing about what does not exist.
	VT_CHECK(W.Purse(0xfffffff0u) == nullptr);
	VT_CHECK(W.Heir(0xfffffff0u) == nullptr);
	RichestOf(W.Instance, W.Families, W.Wealth, 0xfffffff0u, Order);
	VT_CHECK(Order.empty());
}

VAELEN_TEST(Wealth, WealthLiftsTheStandingOfAHousesMembers)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	// The elite of the region belong to wealthier houses than the region's mean.
	std::vector<uint32> Elite;
	EliteOf(W.Instance, W.Persons, W.Standing, Region, Elite);
	VT_REQUIRE(Elite.size() >= 5);
	uint64 EliteWealth = 0;
	uint32 Counted = 0;
	for (const uint32 Person : Elite)
	{
		const PersonInfo* P = FindPerson(W.Instance, W.Persons, Person);
		const Society::HouseWealth* Purse = P != nullptr && P->Family != 0 ? W.Purse(P->Family) : nullptr;
		if (Purse != nullptr)
		{
			EliteWealth += Purse->Rank;
			++Counted;
		}
	}
	VT_REQUIRE(Counted >= 5);
	uint64 AllWealth = 0;
	const std::vector<uint32> Houses = W.HousesOf(Region);
	for (const uint32 Family : Houses)
	{
		const Society::HouseWealth* Purse = W.Purse(Family);
		AllWealth += Purse != nullptr ? Purse->Rank : 0u;
	}
	VAELEN_LOG_INFO(LogWealth, "region %u: the elite's houses average rank %llu, the region's %llu", Region,
					static_cast<unsigned long long>(EliteWealth / Counted),
					static_cast<unsigned long long>(AllWealth / Houses.size()));
	VT_CHECK(EliteWealth * Houses.size() > AllWealth * Counted);
	// Standing still holds its own invariants: nobody ranked is dead or unborn.
	const StandingStats R = MeasureStanding(W.Instance, W.Persons, W.Standing, Region);
	VT_CHECK_EQ(R.Stale, 0u);
	VT_CHECK(R.Ranked > 0);
}

VAELEN_TEST(Wealth, HeirsFollowTheDescentCustom)
{
	uint32 Heirs[2] = {};
	for (const Descent Line : {Descent::Patrilineal, Descent::Matrilineal})
	{
		Run W(AelvorSeed);
		VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
		W.DescendBy(Line);
		const uint32 Region = W.Busiest();
		VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
		// A promotion makes persons without parents: heirs appear once children
		// born in the region are of age and have houses of their own.
		W.Ages.Run(40);
		const WealthStats S = W.Stats(Region);
		VT_CHECK(S.WithHeir > 0);
		VT_CHECK(S.HeirsNamed >= S.WithHeir);
		VT_CHECK_EQ(S.Stale, 0u);
		// Every heir named carries the line of its own house's culture: cultures
		// born of a split after the custom was set keep their own descent, so the
		// check follows each house rather than the world.
		uint32 Checked = 0;
		uint32 Wrong = 0;
		for (const uint32 Family : W.HousesOf(Region))
		{
			const HouseHeir* Named = W.Heir(Family);
			const FamilyInfo* Home = W.House(Family);
			if (Named == nullptr || Named->Family == 0 || Home == nullptr)
			{
				continue;
			}
			++Checked;
			Wrong += Named->Family == Family ? 1u : 0u; // never its own heir
			const NormSet* Custom = NormsOf(W.Instance, W.Ages.Types(), W.Norms, Home->Culture);
			VT_REQUIRE(Custom != nullptr);
			const uint8 Carries = Custom->Descent_ == static_cast<uint32>(Descent::Matrilineal)
									  ? static_cast<uint8>(Sex::Female)
									  : static_cast<uint8>(Sex::Male);
			// The heir's house is that of a living child of the head who carries the line.
			bool Found = false;
			W.Instance.Components()
				.GetPool(W.Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						Found =
							Found || (P.State == static_cast<uint8>(LifeState::Alive) && P.Sex == Carries &&
									  P.Family == Named->Family && (P.Father == Home->Head || P.Mother == Home->Head));
					});
			Wrong += Found ? 0u : 1u;
		}
		VAELEN_LOG_INFO(LogWealth, "%s: %u houses with an heir of %u valued, %u namings checked, %u wrong",
						Line == Descent::Matrilineal ? "matrilineal" : "patrilineal", S.WithHeir, S.Valued, Checked,
						Wrong);
		VT_CHECK(Checked > 0);
		VT_CHECK_EQ(Wrong, 0u);
		Heirs[Line == Descent::Matrilineal ? 1u : 0u] = S.WithHeir;
		// A custom nobody follows names nobody: with no living child of the line
		// a house keeps no heir.
		uint32 Named = 0;
		for (const uint32 Family : W.HousesOf(Region))
		{
			const HouseHeir* H = W.Heir(Family);
			Named += H != nullptr && H->Family != 0 ? 1u : 0u;
		}
		VT_CHECK_EQ(Named, S.WithHeir);
		VT_CHECK(Named < W.HousesOf(Region).size()); // not every house has an heir of its line
	}
	// The custom decides who inherits, and the world answers: a bride joins her
	// husband's house while a groom who already has one keeps it (04.03), so a
	// matrilineal house nearly always has a daughter's house to leave its goods
	// to, and a patrilineal one rarely has a son outside its own walls - its
	// sons hold the house itself, and when the line fails the commons take all.
	VAELEN_LOG_INFO(LogWealth, "heirs named: %u under patrilineal descent, %u under matrilineal", Heirs[0], Heirs[1]);
	VT_CHECK(Heirs[1] > Heirs[0] * 4u);
}

VAELEN_TEST(Wealth, AnExtinctHousePassesItsGoodsToItsHeir)
{
	Run W(AelvorSeed);		  // heirs honoured
	Run X(AelvorSeed, false); // the same world where nobody inherits
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	uint32 Failures = 0;
	for (uint32 Year = 1; Year <= 60; ++Year)
	{
		W.Ages.Run(1);
		X.Ages.Run(1);
		if (Year % 10 == 0 && (W.Stats(Region).Stale != 0 || X.Stats(Region).Stale != 0))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: a wealth or an heir went stale", Year);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const WealthStats S = W.Stats(Region);
	const StockStats Heirs = W.Stock(Region);
	const StockStats None = X.Stock(Region);
	VAELEN_LOG_INFO(LogWealth,
					"sixty years: %u inheritances and %u returns with heirs, %u inheritances and %u returns without",
					Heirs.Inheritances, Heirs.Returns, None.Inheritances, None.Returns);
	VT_CHECK(S.Inheritances > 0);
	VT_CHECK_EQ(S.Inheritances, Heirs.Inheritances);
	VT_CHECK_EQ(None.Inheritances, 0u); // without the hook nothing is inherited, everything returns
	VT_CHECK(None.Returns > Heirs.Returns);
	VT_CHECK(Heirs.Returns > 0); // a house whose line has no heir still loses all to the commons
	// Every inheritance names a living house of the region as the taker.
	uint32 Wrong = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(StockInheritedEvent))
		{
			continue;
		}
		const StockPayload P = E.Get<StockPayload>();
		const FamilyInfo* Taker = W.House(P.House);
		Wrong += Taker == nullptr || Taker->Region != P.Region ? 1u : 0u;
	}
	VT_CHECK_EQ(Wrong, 0u);
}

VAELEN_TEST(Wealth, DeterministicSnapshotSafeAndFrozen)
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
		if (Year % 10 == 0 &&
			(A.Stats().Stale != 0 || ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance)))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale, the worlds %s", Year, A.Stats().Stale,
						 ComputeStateDigest(A.Instance) == ComputeStateDigest(B.Instance) ? "agree" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const WealthStats S = A.Stats();
	VAELEN_LOG_INFO(LogWealth, "frozen: wealth=%016llx heirs=%u inheritances=%u (%u valued, richest %u, %u fortunes)",
					static_cast<unsigned long long>(S.Digest), S.WithHeir, S.Inheritances, S.Valued, S.Richest,
					S.Changes);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_WEALTH_FROZEN_128});
	VT_CHECK_EQ(S.WithHeir, uint32{VAELEN_WEALTH_HEIRS_128});
	VT_CHECK_EQ(S.Inheritances, uint32{VAELEN_WEALTH_INHERITANCES_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, S.Digest);
}
