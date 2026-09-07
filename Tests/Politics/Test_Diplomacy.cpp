// VAELEN - Tests/Politics
// Phase 07.06: diplomacy - what two polities are to each other, warmed and
// cooled by the world they share, and what a war puts in play.
//
// STATUS: VALIDATED (Phase 07)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Factions.h"
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
#define VAELEN_TREATY_FROZEN_128 0x9f07d235c61d3712ull
#define VAELEN_TREATY_CONTACTS_128 4u
#define VAELEN_TREATY_TURNS_128 5u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogTreaty);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, DiplomacyRules InTreaties = DiplomacyRules{},
					 FactionRules InFactions = FactionRules{}, SuccessionRules InLine = SuccessionRules{},
					 ReachRules InReach = ReachRules{}, LawRules InLaws = LawRules{},
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
			Heirs = SuccessionTypes::Declare(Instance);
			Parties = FactionTypes::Declare(Instance);
			Treaties = DiplomacyTypes::Declare(Instance);
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
			Rebels = std::make_unique<FactionSystem>(Instance, Ages.Types(), Persons, Polities, Reaches, Heirs, Parties,
													 InFactions);
			Envoys = std::make_unique<DiplomacySystem>(Instance, Ages.Types(), Trade, Polities, Treaties, InTreaties);
			Words->ObserveContest(Treaties.Contested);
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
			Instance.Systems().Add(Rebels.get());
			Instance.Systems().Add(Envoys.get());
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
		FactionStats Parties_(FactionRules R = FactionRules{}) const
		{
			return MeasureFactions(Instance, Ages.Types(), Persons, Polities, Parties, R);
		}
		const FactionInfo* Faction(uint32 Index) const { return FactionOf(Instance, Parties, Index); }
		DiplomacyStats Treaties_(DiplomacyRules R = DiplomacyRules{}) const
		{
			return MeasureDiplomacy(Instance, Ages.Types(), Polities, Treaties, R);
		}
		const Relation* Bond(uint32 A, uint32 B) const { return RelationBetween(Instance, Treaties, A, B); }
		std::vector<uint32> Neighbours(uint32 Polity) const
		{
			std::vector<uint32> Out;
			NeighboursOf(Instance, Treaties, Polity, Out);
			return Out;
		}
		/// Every standing polity, in index order.
		std::vector<uint32> Powers() const
		{
			std::vector<uint32> Out;
			Instance.Components()
				.GetPool(Polities.Polity)
				.ForEach(
					[&](EntityHandle, const PolityInfo& P)
					{
						if (P.Dissolved == 0)
						{
							Out.push_back(P.Index);
						}
					});
			std::sort(Out.begin(), Out.end());
			return Out;
		}
		/// Set a relation's warmth by hand, to put two powers where a test needs them.
		void Chill(uint32 A, uint32 B, uint32 Warmth)
		{
			if (A > B)
			{
				std::swap(A, B);
			}
			Instance.Components()
				.GetPool(Treaties.Relation_)
				.ForEach(
					[&](EntityHandle, Relation& R)
					{
						if (R.A == A && R.B == B)
						{
							R.Warmth = Warmth;
						}
					});
		}
		std::vector<uint32> FactionsIn(uint32 Polity) const
		{
			std::vector<uint32> Out;
			FactionsOf(Instance, Parties, Polity, Out);
			return Out;
		}
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
		FactionTypes Parties;
		DiplomacyTypes Treaties;
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
		std::unique_ptr<FactionSystem> Rebels;
		std::unique_ptr<DiplomacySystem> Envoys;
	};
} // namespace

namespace
{
	/// Two powers on one world: the two most peopled regions simulated person by
	/// person, each founding a polity, each given the grain to grow until they meet.
	ReachRules WideReach()
	{
		ReachRules Wide;
		Wide.ClaimCost = 40;
		Wide.ReachPerGrain = 60;
		Wide.UpkeepPerHop = 2;
		Wide.HoldLostPerHop = 150;
		Wide.HoldFloor = 100;
		return Wide;
	}
} // namespace

VAELEN_TEST(Diplomacy, ContactIsAFactOfTheGroundAndAWarTakesIt)
{
	const ReachRules Wide = WideReach();
	Run W(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, Wide);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 2);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Ranked[0]));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Ranked[1]));
	W.Ages.Run(4);
	const std::vector<uint32> Powers = W.Powers();
	VT_REQUIRE(Powers.size() == 2);
	// Contact is a fact of the region graph: these two founded on ground close
	// enough that they already touch, and exactly one relation exists for the
	// pair however many borders they share.
	VT_CHECK_EQ(W.Treaties_().Relations_, 1u);
	VT_CHECK_EQ(W.Treaties_().Contacts, 1u);

	// Grow them until their ground meets. Contact is a fact of the region
	// graph: nobody decides it.
	uint32 Met = 0;
	for (uint32 Year = 0; Year < 30 && Met == 0; ++Year)
	{
		for (const uint32 P : W.Powers())
		{
			W.Endow(P, 60000);
		}
		W.Ages.Run(1);
		Met = W.Treaties_().Relations_;
	}
	VT_REQUIRE(Met == 1);
	const Relation* Bond = W.Bond(Powers[0], Powers[1]);
	VT_REQUIRE(Bond != nullptr);
	VT_CHECK(Bond->A < Bond->B);
	VT_CHECK_EQ(Bond->A, Powers[0]);
	VT_CHECK_EQ(Bond->B, Powers[1]);
	VT_CHECK(Bond->Border > 0 && Bond->Met != 0 && Bond->Identity != 0);
	// The same relation whichever side is asked for it.
	VT_CHECK_EQ(W.Bond(Powers[1], Powers[0])->Index, Bond->Index);
	VT_CHECK(W.Neighbours(Powers[0]) == std::vector<uint32>{Powers[1]});
	VAELEN_LOG_INFO(LogTreaty, "contact: %u and %u, warmth %u, border %u, stance %s", Bond->A, Bond->B, Bond->Warmth,
					Bond->Border, StanceName(static_cast<Stance>(Bond->Stance_)));

	// A long border between unequal neighbours cools fast. When it turns to
	// war, the weaker side's border regions are put in play and the stronger
	// takes them - at a price, from the border, one year at a time.
	uint32 Annexed = 0;
	for (uint32 Year = 0; Year < 20 && Annexed == 0; ++Year)
	{
		for (const uint32 P : W.Powers())
		{
			W.Endow(P, 60000);
		}
		W.Ages.Run(1);
		Annexed = W.Words_(Wide).Annexed;
		VT_CHECK_EQ(W.Treaties_().Bad, 0u);
	}
	VT_CHECK(Annexed > 0);
	const DiplomacyStats S = W.Treaties_();
	VT_CHECK(S.Contacts == 1 && S.Turns > 0 && S.Contests > 0);
	VT_CHECK_EQ(S.Bad, 0u);
	VAELEN_LOG_INFO(LogTreaty, "war: %u annexed, %u turns, %u contests, %u powers left", Annexed, S.Turns, S.Contests,
					static_cast<uint32>(W.Powers().size()));
	// Every annexation named the polity it was taken from.
	uint32 Named = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(RegionAnnexedEvent))
		{
			const PolityPayload P = E.Get<PolityPayload>();
			VT_CHECK(P.Person != 0 && P.Person != P.Polity);
			++Named;
		}
	}
	VT_CHECK_EQ(Named, Annexed);
}

VAELEN_TEST(Diplomacy, AStanceHoldsUntilTheWarmthPassesItsEdge)
{
	// Every drift silenced, so the warmth is exactly what the test puts there
	// and the hysteresis is the only thing deciding.
	DiplomacyRules Still;
	Still.WarmthSameCulture = 0;
	Still.WarmthPerRoute = 0;
	Still.ChillPerBorder = 0;
	Still.ChillPerSizeStep = 0;
	Still.Hysteresis = 60;
	const ReachRules Wide = WideReach();
	Run W(AelvorSeed, Still, FactionRules{}, SuccessionRules{}, Wide);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 2);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Ranked[0]));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Ranked[1]));
	W.Ages.Run(4);
	const std::vector<uint32> Powers = W.Powers();
	VT_REQUIRE(Powers.size() == 2);
	uint32 Met = 0;
	for (uint32 Year = 0; Year < 30 && Met == 0; ++Year)
	{
		for (const uint32 P : W.Powers())
		{
			W.Endow(P, 60000);
		}
		W.Ages.Run(1);
		Met = W.Treaties_().Relations_;
	}
	VT_REQUIRE(Met == 1);
	const uint32 A = Powers[0];
	const uint32 B = Powers[1];
	VT_CHECK_EQ(W.Bond(A, B)->Stance_, static_cast<uint32>(Stance::Peace));

	// At the bar exactly, a stance does not turn: it holds until the warmth has
	// passed the edge by the hysteresis.
	W.Chill(A, B, Still.PactAt);
	W.Ages.Run(1);
	VT_CHECK_EQ(W.Bond(A, B)->Stance_, static_cast<uint32>(Stance::Peace));
	W.Chill(A, B, Still.PactAt + Still.Hysteresis);
	W.Ages.Run(1);
	VT_CHECK_EQ(W.Bond(A, B)->Stance_, static_cast<uint32>(Stance::Pact));
	const uint64 Sworn = W.Bond(A, B)->Turned;

	// And a pact is not lost to one bad year: back at the bar it stands.
	W.Chill(A, B, Still.PactAt);
	W.Ages.Run(1);
	VT_CHECK_EQ(W.Bond(A, B)->Stance_, static_cast<uint32>(Stance::Pact));
	VT_CHECK_EQ(W.Bond(A, B)->Turned, Sworn);

	// Cold enough and it is war, and the ground goes in play.
	W.Chill(A, B, 0);
	W.Ages.Run(1);
	VT_CHECK_EQ(W.Bond(A, B)->Stance_, static_cast<uint32>(Stance::War));
	VT_CHECK(W.Treaties_(Still).InPlay > 0);
	VT_CHECK(W.Bond(A, B)->Turned > Sworn);
	VAELEN_LOG_INFO(LogTreaty, "war declared: %u regions in play, %u turns in all", W.Treaties_(Still).InPlay,
					W.Treaties_(Still).Turns);

	// Warm again and the ground comes out of play: nothing stays contested
	// once the war is over.
	W.Chill(A, B, Still.PeaceAt + Still.Hysteresis);
	W.Ages.Run(1);
	VT_CHECK_EQ(W.Bond(A, B)->Stance_, static_cast<uint32>(Stance::Peace));
	VT_CHECK_EQ(W.Treaties_(Still).InPlay, 0u);
	VT_CHECK_EQ(W.Treaties_(Still).Bad, 0u);
}

VAELEN_TEST(Diplomacy, RulesAndEdges)
{
	// One power alone is nothing to anybody, and nothing it holds is ever in play.
	const ReachRules Wide = WideReach();
	Run One(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, Wide);
	VT_REQUIRE(One.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(One.Instance, One.Lod, One.Busiest()));
	One.Ages.Run(3);
	const uint32 P = One.FirstPolity();
	VT_REQUIRE(P != 0);
	One.Endow(P, 60000);
	One.Ages.Run(10);
	const DiplomacyStats Alone = One.Treaties_();
	VT_CHECK_EQ(Alone.Relations_, 0u);
	VT_CHECK_EQ(Alone.InPlay, 0u);
	VT_CHECK_EQ(Alone.Contacts, 0u);
	VT_CHECK_EQ(Alone.Bad, 0u);
	// Without a war nothing is annexed: ground somebody holds is not takeable.
	VT_CHECK_EQ(One.Words_(Wide).Annexed, 0u);

	// The lookups refuse what does not exist and what makes no sense.
	VT_CHECK(One.Bond(0, 1) == nullptr);
	VT_CHECK(One.Bond(P, P) == nullptr);
	VT_CHECK(One.Bond(P, 0xfffffff0u) == nullptr);
	VT_CHECK(One.Neighbours(0).empty());
	VT_CHECK(One.Neighbours(P).empty());

	// A world with no polity at all has no diplomacy.
	Run Empty(AelvorSeed);
	VT_REQUIRE(Empty.Ages.Generate(Run::Square(64), 40));
	Empty.Ages.Run(3);
	const DiplomacyStats None = Empty.Treaties_();
	VT_CHECK_EQ(None.Relations_, 0u);
	VT_CHECK_EQ(None.Bad, 0u);

	// Two worlds of one seed keep the same relations, year for year.
	Run A(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, Wide);
	Run B(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, Wide);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	const std::vector<uint32> RankedA = A.Ranked();
	VT_REQUIRE(RankedA.size() >= 2);
	VT_CHECK(RequestDetail(A.Instance, A.Lod, RankedA[0]));
	VT_CHECK(RequestDetail(A.Instance, A.Lod, RankedA[1]));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, RankedA[0]));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, RankedA[1]));
	for (uint32 Year = 0; Year < 25; ++Year)
	{
		for (const uint32 Power : A.Powers())
		{
			A.Endow(Power, 60000);
		}
		for (const uint32 Power : B.Powers())
		{
			B.Endow(Power, 60000);
		}
		A.Ages.Run(1);
		B.Ages.Run(1);
	}
	VT_CHECK_EQ(A.Treaties_().Digest, B.Treaties_().Digest);
	VT_CHECK_EQ(A.Treaties_().Bad, 0u);
}

VAELEN_TEST(Diplomacy, DeterministicSnapshotSafeAndFrozen)
{
	const ReachRules Wide = WideReach();
	Run A(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, Wide);
	Run B(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, Wide);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	const std::vector<uint32> Ranked = A.Ranked();
	VT_REQUIRE(Ranked.size() >= 2);
	for (uint32 i = 0; i < 2; ++i)
	{
		VT_CHECK(RequestDetail(A.Instance, A.Lod, Ranked[i]));
		VT_CHECK(RequestDetail(B.Instance, B.Lod, Ranked[i]));
	}
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
		const DiplomacyStats S = A.Treaties_();
		if (S.Bad != 0 || A.Treaties_().Digest != B.Treaties_().Digest)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad, relations %s", Year, S.Bad,
						 A.Treaties_().Digest == B.Treaties_().Digest ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const DiplomacyStats S = A.Treaties_();
	VAELEN_LOG_INFO(LogTreaty, "frozen: treaty128=%016llx contacts=%u turns=%u (%u pacts, %u peaces, %u wars)",
					static_cast<unsigned long long>(S.Digest), S.Contacts, S.Turns, S.Pacts, S.Peaces, S.Wars);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_TREATY_FROZEN_128});
	VT_CHECK_EQ(S.Contacts, uint32{VAELEN_TREATY_CONTACTS_128});
	VT_CHECK_EQ(S.Turns, uint32{VAELEN_TREATY_TURNS_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, Wide);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Treaties_().Digest, S.Digest);
}
