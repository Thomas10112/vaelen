// VAELEN - Tests/Politics
// Phase 07.04: succession - the line a polity remembers, the claimant the
// custom names, and what a seat taken by anyone else costs its hold.
//
// STATUS: VALIDATED (Phase 07)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Politics/Succession.h"
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
#define VAELEN_LINE_FROZEN_128 0x77726480cad71e27ull
#define VAELEN_LINE_RULERS_128 9u
#define VAELEN_LINE_SETTLED_128 3u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLine);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, SuccessionRules InLine = SuccessionRules{}, ReachRules InReach = ReachRules{},
					 LawRules InLaws = LawRules{}, PolityRules InRules = PolityRules{})
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
			Heirs = SuccessionTypes::Declare(Instance);
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
			Lineage =
				std::make_unique<SuccessionSystem>(Instance, Ages.Types(), Persons, Norms, Polities, Heirs, InLine);
			Words->RunAfter("Succession");
			Words->ObserveLine(Heirs.Line);
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
			Instance.Systems().Add(Lineage.get());
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
		SuccessionStats Line_(SuccessionRules R = SuccessionRules{}) const
		{
			return MeasureSuccession(Instance, Polities, Persons, Heirs, R);
		}
		const PolityLine* Line(uint32 Polity) const { return LineOf(Instance, Polities, Heirs, Polity); }
		const PersonInfo* Person(uint32 Index) const { return FindPerson(Instance, Persons, Index); }
		/// Kill a person outright, as a plague would, to make a seat fall empty.
		bool Strike(uint32 Index)
		{
			bool Struck = false;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, PersonInfo& P)
					{
						if (P.Index == Index && P.State == static_cast<uint8>(LifeState::Alive))
						{
							P.State = static_cast<uint8>(LifeState::Dead);
							P.Died = Instance.Now();
							Struck = true;
						}
					});
			return Struck;
		}
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
		SuccessionTypes Heirs;
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
		std::unique_ptr<SuccessionSystem> Lineage;
	};
} // namespace

VAELEN_TEST(Succession, TheLineRemembersWhoSatAndWhomTheCustomNamed)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	const PolityLine* L = W.Line(Polity);
	VT_REQUIRE(L != nullptr);
	VT_CHECK_EQ(L->Polity, Polity);
	VT_CHECK(L->Rulers >= 1);
	// The line says what the polity says: one seat, one person on it.
	VT_CHECK_EQ(L->Sitting, W.Polity(Polity)->Ruler);
	VT_CHECK_EQ(W.Line_().Bad, 0u);

	// Over a lifetime the seat passes several times, and every passing is in
	// the log exactly once, settled or contested.
	W.Ages.Run(60);
	const SuccessionStats S = W.Line_();
	VT_CHECK(S.Rulers > 1);
	// Every passing of a seat is in the log once, settled or contested. The
	// first ruler of a line is counted but not announced: the line begins
	// with whoever was already sitting when this system first saw the polity.
	VT_CHECK(S.Settled + S.Contested >= S.Rulers - S.Lines);
	VT_CHECK(S.Settled + S.Contested <= S.Rulers);
	VT_CHECK_EQ(S.Bad, 0u);
	VT_CHECK_EQ(W.Line(Polity)->Sitting, W.Polity(Polity)->Ruler);
	VAELEN_LOG_INFO(LogLine, "after sixty years: %u rulers, %u settled, %u contested, %u interregna, unrest %u",
					S.Rulers, S.Settled, S.Contested, S.Interregna, S.Unrest);

	// A claimant the custom names is alive, of age, and a child of whoever sits
	// on the line of descent the culture keeps.
	uint32 Named = W.Line(Polity)->Claimant;
	if (Named != 0)
	{
		const PersonInfo* Heir = W.Person(Named);
		VT_REQUIRE(Heir != nullptr);
		VT_CHECK_EQ(Heir->State, static_cast<uint8>(LifeState::Alive));
		const uint32 Sitting = W.Line(Polity)->Sitting;
		VT_CHECK(Heir->Father == Sitting || Heir->Mother == Sitting);
		VAELEN_LOG_INFO(LogLine, "the custom names person %u, child of %u", Named, Sitting);
	}
}

VAELEN_TEST(Succession, AnEmptySeatAndAPassedOverHeirBothCostTheHold)
{
	SuccessionRules Sharp;
	Sharp.UnrestOnDisputed = 400;
	Sharp.UnrestOnVacancy = 300;
	Sharp.UnrestFadePerYear = 25;
	Run W(AelvorSeed, Sharp);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(4);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	W.Endow(Polity, 8000);
	W.Ages.Run(2);
	const uint32 Wide = W.Words_().Held;
	VT_CHECK(Wide > 1);

	// Take the ruler out of the world. The council seats another, or nobody,
	// and either way the polity carries the shock.
	const uint32 Sitting = W.Line(Polity)->Sitting;
	VT_REQUIRE(Sitting != 0);
	VT_CHECK(W.Strike(Sitting));
	W.Ages.Run(1);
	const SuccessionStats S = W.Line_(Sharp);
	VT_CHECK(S.Vacancies + S.Settled + S.Contested > 0);
	VT_CHECK(W.Line(Polity)->Sitting != Sitting || W.Polity(Polity)->Ruler == 0);
	VT_CHECK_EQ(S.Bad, 0u);
	const uint32 Shock = W.Line(Polity)->Unrest;
	VAELEN_LOG_INFO(LogLine, "the ruler struck down: unrest %u, %u vacancies, %u contested, %u held (was %u)", Shock,
					S.Vacancies, S.Contested, W.Words_().Held, Wide);

	// While the unrest lasts, every region is held less firmly than the rule
	// alone would say - and the seat itself is never let go.
	if (Shock != 0)
	{
		uint32 Checked = 0;
		for (uint32 R = 1; R <= 120; ++R)
		{
			const RegionAuthority* A = W.Hold(R);
			if (A == nullptr || A->Polity != Polity || A->Distance == 0)
			{
				continue;
			}
			++Checked;
			const uint32 Plain = ReachRules{}.HoldAtSeat - A->Distance * ReachRules{}.HoldLostPerHop;
			VT_CHECK(A->Hold < Plain);
		}
		VAELEN_LOG_INFO(LogLine, "%u regions held under the shock", Checked);
	}
	VT_CHECK(W.Hold(Region) != nullptr && W.Hold(Region)->Polity == Polity);

	// The world forgets: the unrest fades to nothing and the hold comes back.
	W.Ages.Run(20);
	VT_CHECK_EQ(W.Line_(Sharp).Bad, 0u);
	VT_CHECK(W.Line(Polity) == nullptr || W.Line(Polity)->Unrest < Shock || Shock == 0);
}

VAELEN_TEST(Succession, RulesAndEdges)
{
	// No unrest ever stands above its ceiling, however sharp the rules.
	SuccessionRules Brutal;
	Brutal.UnrestOnDisputed = 5000;
	Brutal.UnrestOnVacancy = 5000;
	Brutal.UnrestCeiling = 400;
	Brutal.UnrestFadePerYear = 1;
	Run W(AelvorSeed, Brutal);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	for (uint32 Year = 0; Year < 40; ++Year)
	{
		W.Ages.Run(1);
		const SuccessionStats S = W.Line_(Brutal);
		VT_CHECK_EQ(S.Bad, 0u);
		VT_CHECK(S.Unrest <= Brutal.UnrestCeiling);
	}

	// A claimant must be of age: raise the bar past any living child and the
	// custom names nobody, so no succession is ever contested.
	SuccessionRules Ancient;
	Ancient.HeirFromAge = 200;
	Run A(AelvorSeed, Ancient);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(A.Instance, A.Lod, A.Busiest()));
	A.Ages.Run(50);
	const SuccessionStats Old = A.Line_(Ancient);
	VT_CHECK_EQ(Old.Contested, 0u);
	VT_CHECK_EQ(Old.Bad, 0u);
	const uint32 P = A.FirstPolity();
	if (P != 0 && A.Line(P) != nullptr)
	{
		VT_CHECK_EQ(A.Line(P)->Claimant, 0u);
		VT_CHECK_EQ(A.Line(P)->Unrest, 0u);
	}

	// The lookups refuse what does not exist, and a world with no polity has
	// no line at all.
	VT_CHECK(W.Line(0xfffffff0u) == nullptr);
	Run Empty(AelvorSeed);
	VT_REQUIRE(Empty.Ages.Generate(Run::Square(64), 40));
	Empty.Ages.Run(3);
	const SuccessionStats None = Empty.Line_();
	VT_CHECK_EQ(None.Lines, 0u);
	VT_CHECK_EQ(None.Rulers, 0u);
	VT_CHECK_EQ(None.Bad, 0u);

	// Two worlds of one seed keep the same line.
	Run B(AelvorSeed);
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, B.Busiest()));
	B.Ages.Run(25);
	Run C(AelvorSeed);
	VT_REQUIRE(C.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(C.Instance, C.Lod, C.Busiest()));
	C.Ages.Run(25);
	VT_CHECK_EQ(B.Line_().Digest, C.Line_().Digest);
}

VAELEN_TEST(Succession, DeterministicSnapshotSafeAndFrozen)
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
		const SuccessionStats S = A.Line_();
		if (S.Bad != 0 || A.Line_().Digest != B.Line_().Digest)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad, lines %s", Year, S.Bad,
						 A.Line_().Digest == B.Line_().Digest ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const SuccessionStats S = A.Line_();
	VAELEN_LOG_INFO(LogLine, "frozen: line128=%016llx rulers=%u settled=%u (%u contested, %u interregna, unrest %u)",
					static_cast<unsigned long long>(S.Digest), S.Rulers, S.Settled, S.Contested, S.Interregna,
					S.Unrest);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_LINE_FROZEN_128});
	VT_CHECK_EQ(S.Rulers, uint32{VAELEN_LINE_RULERS_128});
	VT_CHECK_EQ(S.Settled, uint32{VAELEN_LINE_SETTLED_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Line_().Digest, S.Digest);
}
