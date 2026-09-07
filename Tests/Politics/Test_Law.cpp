// VAELEN - Tests/Politics
// Phase 07.02: law - what a polity demands written onto the regions it rules,
// the grain it takes for having demanded it, and the share moving by itself.
//
// STATUS: VALIDATED (Phase 07)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Law.h"
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
#define VAELEN_LAW_FROZEN_128 0x6e8b720c1543fb20ull
#define VAELEN_LAW_TAKEN_128 3476u
#define VAELEN_LAW_CHANGES_128 50u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLaw);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, LawRules InLaws = LawRules{}, PolityRules InRules = PolityRules{})
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
	};
} // namespace

VAELEN_TEST(Law, TheLawIsWrittenOntoTheRegionsAndTheGrainIsTaken)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	// Nobody rules, so nobody demands: no law anywhere, no dues on any region.
	VT_CHECK_EQ(W.Ledger().Laws, 0u);
	VT_CHECK(W.Dues(Region) == nullptr);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(2);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);

	// The founding writes a law, and the law is written down onto the seat.
	const PolityLaw* L = W.Law(Polity);
	VT_REQUIRE(L != nullptr);
	VT_CHECK_EQ(L->Polity, Polity);
	// The founding year is the law's own: the first thing the log says a polity
	// demanded is exactly what the rules say a new polity demands.
	uint32 First = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(LawChangedEvent) && First == 0)
		{
			First = E.Get<PolityPayload>().Value;
		}
	}
	VT_CHECK_EQ(First, LawRules{}.TaxAtFounding);
	VT_CHECK(L->TaxPerMille >= LawRules{}.TaxFloor && L->TaxPerMille <= LawRules{}.TaxCeiling);
	const RegionDues* Owed = W.Dues(Region);
	VT_REQUIRE(Owed != nullptr);
	VT_CHECK_EQ(Owed->PerMille, L->TaxPerMille);
	VAELEN_LOG_INFO(LogLaw, "polity %u demands %u per mille of region %u", Polity, L->TaxPerMille, Region);

	// The years pass: the harvest is assessed, the collector comes, the store fills.
	W.Ages.Run(20);
	const LawStats S = W.Ledger();
	VT_CHECK_EQ(S.Laws, 1u);
	VT_CHECK_EQ(S.Taxed, 1u);
	VT_CHECK_EQ(S.Bad, 0u);
	VT_CHECK(S.Paid > 0);
	const Treasury* Hoard = W.Hoard(Polity);
	VT_REQUIRE(Hoard != nullptr);
	VT_CHECK(Hoard->Amount[static_cast<uint32>(Good::Grain)] > 0);
	// Nothing is spent yet, so what was taken is exactly what is held, and
	// exactly what the log says was paid.
	const PolityLaw* Now = W.Law(Polity);
	VT_REQUIRE(Now != nullptr);
	VT_CHECK_EQ(static_cast<uint32>(Now->Taken), Hoard->Amount[static_cast<uint32>(Good::Grain)]);
	uint64 Paid = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(DuesPaidEvent))
		{
			Paid += E.Get<PolityPayload>().Value;
		}
	}
	VT_CHECK_EQ(Paid, Now->Taken);
	VAELEN_LOG_INFO(LogLaw, "after twenty years: %llu grain taken in %u payments, %u short, %u changes of law",
					static_cast<unsigned long long>(Now->Taken), S.Paid, S.Unpaid, S.Changes);
	// Only a region that is ruled is demanded of.
	VT_CHECK_EQ(S.Taxed, W.Stats().Ruled);
}

VAELEN_TEST(Law, TheShareMovesWithWhatItHoldsAndRelentsWhenNothingCanBePaid)
{
	LawRules Tight;
	Tight.TaxAtFounding = 100;
	Tight.TaxFloor = 20;
	Tight.TaxCeiling = 200;
	Tight.TaxStep = 40;
	Tight.WantPerPerson = 1000; // never satisfied: the share climbs to its ceiling
	Run W(AelvorSeed, Tight);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(2);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	VT_CHECK(W.Law(Polity)->TaxPerMille >= Tight.TaxFloor && W.Law(Polity)->TaxPerMille <= Tight.TaxCeiling);
	W.Ages.Run(8);
	// A polity that cannot fill its store demands more, and never past its ceiling.
	const PolityLaw* Climbed = W.Law(Polity);
	VT_REQUIRE(Climbed != nullptr);
	VT_CHECK_EQ(Climbed->TaxPerMille, Tight.TaxCeiling);
	VT_CHECK(Climbed->Changes >= 2);
	VT_CHECK_EQ(W.Dues(Region)->PerMille, Tight.TaxCeiling);
	VAELEN_LOG_INFO(LogLaw, "the share climbed to %u per mille in %u moves", Climbed->TaxPerMille, Climbed->Changes);

	// A polity that is full relents, down to its floor and no further.
	LawRules Fat = Tight;
	Fat.WantPerPerson = 0; // the store is always fat enough
	Run F(AelvorSeed, Fat);
	VT_REQUIRE(F.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(F.Instance, F.Lod, F.Busiest()));
	F.Ages.Run(10);
	const uint32 Rich = F.FirstPolity();
	VT_REQUIRE(Rich != 0);
	VT_CHECK_EQ(F.Law(Rich)->TaxPerMille, Fat.TaxFloor);
	VT_CHECK_EQ(F.Ledger(Fat).Bad, 0u);

	// A region stripped of its grain cannot pay: the dues stand as arrears, the
	// polity records the failure, and after its patience it relents anyway.
	LawRules Patient = Tight;
	Patient.FailuresBeforeRelief = 2;
	Run P(AelvorSeed, Patient);
	VT_REQUIRE(P.Ages.Generate(Run::Square(128), 300));
	const uint32 Poor = P.Busiest();
	VT_CHECK(RequestDetail(P.Instance, P.Lod, Poor));
	P.Ages.Run(4);
	const uint32 Ruler = P.FirstPolity();
	VT_REQUIRE(Ruler != 0);
	const uint32 Before = P.Law(Ruler)->TaxPerMille;
	for (uint32 Year = 0; Year < 4; ++Year)
	{
		P.FillEvery(-1000000); // the common stock of every region emptied
		P.Ages.Run(1);
	}
	const LawStats Short = P.Ledger(Patient);
	VT_CHECK(Short.Unpaid > 0);
	VT_CHECK(Short.Arrears > 0);
	VT_CHECK_EQ(Short.Bad, 0u);
	VT_CHECK(P.Law(Ruler)->TaxPerMille <= Before);
	VAELEN_LOG_INFO(LogLaw, "stripped: %u unpaid, %llu in arrears, the share now %u (was %u)", Short.Unpaid,
					static_cast<unsigned long long>(Short.Arrears), P.Law(Ruler)->TaxPerMille, Before);
}

VAELEN_TEST(Law, RulesAndEdges)
{
	// A polity that ends demands nothing more, and the region it held is
	// demanded nothing of - but what it already owed it still owes.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(8);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	VT_CHECK(W.Dues(Region) != nullptr && W.Dues(Region)->PerMille != 0);
	// A region nobody rules is never demanded of: it has no dues at all, and
	// the count of the demanded never runs ahead of the count of the ruled.
	uint32 Untouched = 0;
	for (uint32 Other = 1; Other <= 40 && Untouched == 0; ++Other)
	{
		if (Other != Region && W.Rule(Other) == nullptr)
		{
			Untouched = Other;
		}
	}
	VT_REQUIRE(Untouched != 0);
	VT_CHECK(W.Dues(Untouched) == nullptr);
	for (uint32 Year = 0; Year < 6; ++Year)
	{
		W.Ages.Run(1);
		const LawStats Step = W.Ledger();
		VT_CHECK_EQ(Step.Bad, 0u); // no dues without a master, no polity that is gone still demanding
		VT_CHECK_EQ(Step.Taxed, W.Stats().Ruled);
		// The law and what is written on the region never drift apart.
		VT_CHECK_EQ(W.Dues(Region)->PerMille, W.Law(Polity)->TaxPerMille);
	}
	const LawStats After = W.Ledger();
	VT_CHECK_EQ(After.Bad, 0u);

	// The lookups refuse what does not exist, and a world with no polity has no law.
	VT_CHECK(W.Law(0xfffffff0u) == nullptr);
	VT_CHECK(W.Hoard(0xfffffff0u) == nullptr);
	VT_CHECK(W.Dues(0xfffffff0u) == nullptr);
	Run Empty(AelvorSeed);
	VT_REQUIRE(Empty.Ages.Generate(Run::Square(64), 40));
	Empty.Ages.Run(3);
	const LawStats None = Empty.Ledger();
	VT_CHECK_EQ(None.Laws, 0u);
	VT_CHECK_EQ(None.Taxed, 0u);
	VT_CHECK_EQ(None.Held, 0u);
	VT_CHECK_EQ(None.Bad, 0u);

	// A share is never written outside its bounds, whatever the rules ask for.
	LawRules Absurd;
	Absurd.TaxAtFounding = 5000;
	Absurd.TaxFloor = 30;
	Absurd.TaxCeiling = 60;
	Run A(AelvorSeed, Absurd);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(A.Instance, A.Lod, A.Busiest()));
	A.Ages.Run(6);
	const uint32 Bound = A.FirstPolity();
	VT_REQUIRE(Bound != 0);
	VT_CHECK(A.Law(Bound)->TaxPerMille >= Absurd.TaxFloor && A.Law(Bound)->TaxPerMille <= Absurd.TaxCeiling);
	VT_CHECK_EQ(A.Ledger(Absurd).Bad, 0u);

	// Two worlds of one seed agree on every law and every due.
	Run B(AelvorSeed);
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, B.Busiest()));
	Run C(AelvorSeed);
	VT_REQUIRE(C.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(C.Instance, C.Lod, C.Busiest()));
	B.Ages.Run(15);
	C.Ages.Run(15);
	VT_CHECK_EQ(B.Ledger().Digest, C.Ledger().Digest);
}

VAELEN_TEST(Law, DeterministicSnapshotSafeAndFrozen)
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
		const LawStats S = A.Ledger();
		if (S.Bad != 0 || A.Ledger().Digest != B.Ledger().Digest)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad laws, ledgers %s", Year, S.Bad,
						 A.Ledger().Digest == B.Ledger().Digest ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const LawStats S = A.Ledger();
	VAELEN_LOG_INFO(LogLaw, "frozen: law128=%016llx held=%llu changes=%u (%u laws, %u taxed, %u paid, %u unpaid)",
					static_cast<unsigned long long>(S.Digest), static_cast<unsigned long long>(S.Held), S.Changes,
					S.Laws, S.Taxed, S.Paid, S.Unpaid);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_LAW_FROZEN_128});
	VT_CHECK_EQ(static_cast<uint32>(S.Held), uint32{VAELEN_LAW_TAKEN_128});
	VT_CHECK_EQ(S.Changes, uint32{VAELEN_LAW_CHANGES_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Ledger().Digest, S.Digest);
}
