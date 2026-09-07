// VAELEN - Tests/Politics
// Phase 07.03: authority and reach - the hold of a region falling with its
// distance from the seat, the grain it costs to carry a word that far, what
// slips free unpaid, and the ground a treasury can take.
//
// STATUS: VALIDATED (Phase 07)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Politics/Polities.h"
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
using namespace Vaelen::Politics;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (07.02): AELVOR 128 at
// year 300, the busiest region detailed, 100 years with every Phase 04, 05,
// 06 and 07 system so far.
#define VAELEN_REACH_FROZEN_128 0xb0849188fdca0aaeull
#define VAELEN_REACH_HELD_128 41u
#define VAELEN_REACH_SPENT_128 152520u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogReach);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, ReachRules InReach = ReachRules{}, LawRules InLaws = LawRules{},
					 PolityRules InRules = PolityRules{})
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
			Polities = PolityTypes::Declare(Instance);
			Laws = LawTypes::Declare(Instance);
			Reaches = ReachTypes::Declare(Instance);
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
			Rulers = std::make_unique<PolitySystem>(Instance, Ages.Types(), Persons, Organizations, Polities, InRules);
			Rulers->RunAfter("Lod");
			Statutes = std::make_unique<LawSystem>(Instance, Ages.Types(), Economy, Polities, Laws, InLaws);
			// The dues of a year are collected the year they are assessed.
			Statutes->RunAfter("Production");
			Harvest->ObserveDues(Laws.Dues);
			Words = std::make_unique<ReachSystem>(Instance, Ages.Types(), Economy, Polities, Laws, Reaches, InReach);
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
			Instance.Systems().Add(Rulers.get());
			Instance.Systems().Add(Statutes.get());
			Instance.Systems().Add(Words.get());
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
		PolityStats Stats() const { return MeasurePolities(Instance, Ages.Types(), Persons, Organizations, Polities); }
		LawStats Ledger(LawRules R = LawRules{}) const
		{
			return MeasureLaws(Instance, Ages.Types(), Polities, Laws, R);
		}
		const PolityLaw* Law(uint32 Polity) const { return LawOf(Instance, Polities, Laws, Polity); }
		const Treasury* Hoard(uint32 Polity) const { return TreasuryOf(Instance, Polities, Laws, Polity); }
		const RegionDues* Dues(uint32 Region) const { return DuesOf(Instance, Ages.Types(), Laws, Region); }
		ReachStats Words_(ReachRules R = ReachRules{}) const
		{
			return MeasureReach(Instance, Ages.Types(), Polities, Reaches, R);
		}
		const RegionAuthority* Hold(uint32 Region) const
		{
			return AuthorityOf(Instance, Ages.Types(), Reaches, Region);
		}
		const PolityReach* Far(uint32 Polity) const { return ReachOf(Instance, Polities, Reaches, Polity); }
		/// Fill a polity's treasury by hand, to buy it a reach it has not earned.
		void Endow(uint32 Polity, uint32 Grain)
		{
			Instance.Components()
				.GetPool(Polities.Polity)
				.ForEach(
					[&](EntityHandle H, const PolityInfo& P)
					{
						if (P.Index != Polity)
						{
							return;
						}
						Treasury* T = Instance.Components().GetPool(Laws.Hoard).TryGet(H);
						if (T != nullptr)
						{
							T->Amount[static_cast<uint32>(Good::Grain)] = Grain;
						}
					});
		}
		/// The index of the first polity standing, 0 when none is.
		uint32 FirstPolity() const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Polities.Polity)
				.ForEach(
					[&](EntityHandle, const PolityInfo& P)
					{
						if (P.Dissolved == 0 && (Out == 0 || P.Index < Out))
						{
							Out = P.Index;
						}
					});
			return Out;
		}
		const PolityInfo* Polity(uint32 Index) const { return PolityOf(Instance, Polities, Index); }
		const RegionRule* Rule(uint32 Region) const { return RuleOf(Instance, Ages.Types(), Polities, Region); }
		const OrganizationInfo* Council(uint32 Region) const
		{
			std::vector<OrganizationInfo> All;
			OrganizationsOf(Instance, Organizations, Region, All);
			static OrganizationInfo Found;
			for (const OrganizationInfo& O : All)
			{
				if (O.Kind == static_cast<uint32>(OrganizationKind::Council))
				{
					Found = O;
					return &Found;
				}
			}
			return nullptr;
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
		PolityTypes Polities;
		LawTypes Laws;
		ReachTypes Reaches;
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
		std::unique_ptr<PolitySystem> Rulers;
		std::unique_ptr<LawSystem> Statutes;
		std::unique_ptr<ReachSystem> Words;
	};
} // namespace

VAELEN_TEST(Reach, AuthorityFallsWithDistanceAndAWordIsPaidFor)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(W.Hold(Region) == nullptr); // nobody's word runs anywhere yet
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);

	// At the seat a polity is itself: no distance, the whole hold, no upkeep.
	const RegionAuthority* Seat = W.Hold(Region);
	VT_REQUIRE(Seat != nullptr);
	VT_CHECK_EQ(Seat->Polity, Polity);
	VT_CHECK_EQ(Seat->Distance, 0u);
	VT_CHECK_EQ(Seat->Hold, ReachRules{}.HoldAtSeat);
	const PolityReach* F = W.Far(Polity);
	VT_REQUIRE(F != nullptr);
	VT_CHECK(F->Reach >= 1);

	// A treasury buys ground: give it grain and it takes what it can walk to.
	W.Endow(Polity, 5000);
	W.Ages.Run(1);
	const ReachStats S = W.Words_();
	VT_CHECK(S.Taken > 0);
	VT_CHECK(S.Held > 1); // the seat and what it took
	VT_CHECK_EQ(S.Bad, 0u);
	VAELEN_LOG_INFO(LogReach, "with 5000 grain: %u regions held, %u taken, farthest %u hops, %llu spent", S.Held,
					S.Taken, S.Far, static_cast<unsigned long long>(S.Spent));

	// What it took is held less firmly than the seat, and the fall is exactly
	// the rule: one step of the rule per hop.
	uint32 Checked = 0;
	for (uint32 R = 1; R <= 120; ++R)
	{
		const RegionAuthority* A = W.Hold(R);
		if (A == nullptr || A->Polity != Polity || A->Distance == 0)
		{
			continue;
		}
		++Checked;
		VT_CHECK(A->Hold < ReachRules{}.HoldAtSeat);
		VT_CHECK(A->Hold >= ReachRules{}.HoldFloor);
		const uint32 Want = ReachRules{}.HoldAtSeat - A->Distance * ReachRules{}.HoldLostPerHop;
		VT_CHECK_EQ(A->Hold, Want);
	}
	VT_CHECK(Checked > 0);
	// Carrying a word costs: the treasury is lighter than what it was given.
	VT_CHECK(W.Far(Polity)->Spent > 0);
	VT_CHECK(W.Words_().Bad == 0u);
}

VAELEN_TEST(Reach, WhatCannotBePaidForSlipsFree)
{
	// A rule that makes the far edge unaffordable: a heavy upkeep, a hold that
	// falls fast, and a floor just under one hop.
	ReachRules Costly;
	Costly.UpkeepPerHop = 400;
	Costly.HoldLostPerHop = 300;
	Costly.HoldFloor = 500;
	Costly.UnpaidHoldLoss = 300;
	Costly.ClaimCost = 50;
	Costly.ReachPerGrain = 100;
	Run W(AelvorSeed, Costly);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	// Buy it a great deal of ground in one year, then take the money away.
	W.Endow(Polity, 40000);
	W.Ages.Run(1);
	const uint32 Reached = W.Words_(Costly).Held;
	VT_CHECK(Reached > 1);
	W.Endow(Polity, 0);
	W.Ages.Run(3);
	const ReachStats S = W.Words_(Costly);
	// Unpaid, the far edge is let go - and the seat never is.
	VT_CHECK(S.Unfunded > 0);
	VT_CHECK(S.Slipped > 0);
	VT_CHECK(S.Held < Reached);
	VT_CHECK_EQ(S.Bad, 0u);
	const RegionAuthority* Seat = W.Hold(Region);
	VT_REQUIRE(Seat != nullptr);
	VT_CHECK_EQ(Seat->Polity, Polity);
	VAELEN_LOG_INFO(LogReach, "unpaid: %u held (was %u), %u slipped, %u years short, %llu still owed", S.Held, Reached,
					S.Slipped, S.Unfunded, static_cast<unsigned long long>(S.Unpaid));
	// A region that slipped carries nobody's authority and belongs to nobody.
	uint32 Free = 0;
	for (uint32 R = 1; R <= 120 && Free == 0; ++R)
	{
		const RegionAuthority* A = W.Hold(R);
		if (A != nullptr && A->Polity == 0 && R != Region)
		{
			Free = R;
		}
	}
	VT_REQUIRE(Free != 0);
	VT_CHECK_EQ(W.Hold(Free)->Hold, 0u);
	const RegionRule* Rule = W.Rule(Free);
	VT_CHECK(Rule == nullptr || Rule->Polity == 0);
}

VAELEN_TEST(Reach, RulesAndEdges)
{
	// A word never carries past its ceiling, however rich the polity.
	ReachRules Short_;
	Short_.ReachCeiling = 1;
	Short_.ReachPerGrain = 1;
	Run W(AelvorSeed, Short_);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	W.Endow(Polity, 1000000);
	W.Ages.Run(2);
	VT_CHECK_EQ(W.Far(Polity)->Reach, 1u);
	VT_CHECK(W.Words_(Short_).Far <= 1u); // nothing is ever held two hops out
	VT_CHECK_EQ(W.Words_(Short_).Bad, 0u);

	// The lookups refuse what does not exist, and a world with no polity has
	// nobody's authority anywhere.
	VT_CHECK(W.Hold(0xfffffff0u) == nullptr);
	VT_CHECK(W.Far(0xfffffff0u) == nullptr);
	Run Empty(AelvorSeed);
	VT_REQUIRE(Empty.Ages.Generate(Run::Square(64), 40));
	Empty.Ages.Run(3);
	const ReachStats None = Empty.Words_();
	VT_CHECK_EQ(None.Held, 0u);
	VT_CHECK_EQ(None.Taken, 0u);
	VT_CHECK_EQ(None.Bad, 0u);

	// A polity taking ground never takes what another already holds, and the
	// authority of a region always names whoever rules it - checked every year.
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(A.Instance, A.Lod, A.Busiest()));
	A.Ages.Run(3);
	const uint32 P = A.FirstPolity();
	VT_REQUIRE(P != 0);
	A.Endow(P, 20000);
	for (uint32 Year = 0; Year < 12; ++Year)
	{
		A.Ages.Run(1);
		const ReachStats S = A.Words_();
		VT_CHECK_EQ(S.Bad, 0u);
		VT_CHECK_EQ(S.Held, A.Stats().Ruled);
		VT_CHECK_EQ(A.Stats().Bad, 0u);
	}

	// Two worlds of one seed take the same ground in the same year.
	Run B(AelvorSeed);
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, B.Busiest()));
	B.Ages.Run(15);
	Run C(AelvorSeed);
	VT_REQUIRE(C.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(C.Instance, C.Lod, C.Busiest()));
	C.Ages.Run(15);
	VT_CHECK_EQ(B.Words_().Digest, C.Words_().Digest);
}

VAELEN_TEST(Reach, DeterministicSnapshotSafeAndFrozen)
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
		const ReachStats S = A.Words_();
		if (S.Bad != 0 || A.Words_().Digest != B.Words_().Digest)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad, reaches %s", Year, S.Bad,
						 A.Words_().Digest == B.Words_().Digest ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const ReachStats S = A.Words_();
	VAELEN_LOG_INFO(LogReach, "frozen: reach128=%016llx held=%u spent=%llu (%u taken, %u slipped, %u short, far %u)",
					static_cast<unsigned long long>(S.Digest), S.Held, static_cast<unsigned long long>(S.Spent),
					S.Taken, S.Slipped, S.Unfunded, S.Far);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_REACH_FROZEN_128});
	VT_CHECK_EQ(S.Held, uint32{VAELEN_REACH_HELD_128});
	VT_CHECK_EQ(static_cast<uint32>(S.Spent), uint32{VAELEN_REACH_SPENT_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Words_().Digest, S.Digest);
}
