// VAELEN - Tests/Politics
// Phase 07.05: factions - a grievance with a place and sometimes a person,
// gathering while nobody answers it, and taking the ground when it can.
//
// STATUS: VALIDATED (Phase 07)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Reach.h"
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
#define VAELEN_FACTION_FROZEN_128 0x2906077fcd8b815dull
#define VAELEN_FACTION_FORMED_128 15u
#define VAELEN_FACTION_REVOLTS_128 13u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogFaction);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, FactionRules InFactions = FactionRules{}, SuccessionRules InLine = SuccessionRules{},
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
			Rebels->ObserveDues(Laws.Dues);
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
	};
} // namespace

VAELEN_TEST(Factions, NeglectBreedsAFactionAndAStrongOneTakesItsRegion)
{
	// A polity that overreaches: cheap ground, a hold that falls fast, and
	// people whose patience is short.
	FactionRules Quick;
	Quick.NeglectUnderHold = 800;
	Quick.NeglectYears = 2;
	Quick.StrengthAtBirth = 200;
	Quick.StrengthPerYearAggrieved = 200;
	Quick.RevoltAt = 600;
	ReachRules Thin;
	Thin.HoldLostPerHop = 250;
	Thin.HoldFloor = 100;
	Thin.ClaimCost = 60;
	Thin.ReachPerGrain = 100;
	Run W(AelvorSeed, Quick, SuccessionRules{}, Thin);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	// A seat is not a province: whatever else is aggrieved in this world, no
	// faction ever rises in the region a polity rules from.
	VT_CHECK_EQ(W.Parties_(Quick).Bad, 0u);
	const uint32 Capital = W.Polity(Polity)->Seat;
	W.Endow(Polity, 30000);
	W.Ages.Run(2);
	VT_CHECK(W.Words_(Thin).Held > 2);

	// Held loosely, year after year, its far provinces stop counting themselves
	// as ruled.
	W.Ages.Run(4);
	const FactionStats S = W.Parties_(Quick);
	VT_CHECK(S.Formed > 0);
	VT_CHECK(S.Aggrieved > 0);
	VT_CHECK_EQ(S.Bad, 0u);
	const std::vector<uint32> Standing = W.FactionsIn(Polity);
	VAELEN_LOG_INFO(LogFaction, "%u formed, %u standing, %u regions aggrieved, strongest %u", S.Formed,
					static_cast<uint32>(Standing.size()), S.Aggrieved, S.Strongest);
	if (!Standing.empty())
	{
		const FactionInfo* F = W.Faction(Standing.front());
		VT_REQUIRE(F != nullptr);
		VT_CHECK_EQ(F->Polity, Polity);
		VT_CHECK_EQ(F->Cause, static_cast<uint32>(Grievance::Neglect));
		VT_CHECK_EQ(F->Claimant, 0u); // a neglected province wants no one in particular
		VT_CHECK(F->Strength > 0 && F->Identity != 0 && F->Formed != 0 && F->Ended == 0);
		// The region it sits in is one the polity really rules.
		VT_CHECK(W.Rule(F->Region) != nullptr && W.Rule(F->Region)->Polity == Polity);
	}

	// Left unanswered it grows, and at its threshold the region simply leaves.
	W.Ages.Run(6);
	const FactionStats After = W.Parties_(Quick);
	VT_CHECK(After.Revolts > 0);
	VT_CHECK_EQ(After.Bad, 0u);
	VAELEN_LOG_INFO(LogFaction, "after ten years: %u formed, %u revolts, %u faded, %u standing", After.Formed,
					After.Revolts, After.Faded, After.Standing);
	// A region that revolted belongs to nobody and carries nobody's authority.
	uint32 Free = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(FactionRevoltedEvent))
		{
			Free = E.Get<PolityPayload>().Region;
		}
	}
	VT_REQUIRE(Free != 0);
	const RegionRule* Rule = W.Rule(Free);
	const RegionAuthority* A = W.Hold(Free);
	VT_CHECK(Rule == nullptr || Rule->Polity != Polity);
	VT_CHECK(A == nullptr || A->Polity != Polity);
	// The seat is never taken this way: a faction takes ground, not the throne.
	VT_CHECK(W.Rule(Region) != nullptr && W.Rule(Region)->Polity == Polity);
	VT_CHECK_EQ(Capital, Region);
	for (uint32 i = 1; i <= After.Formed; ++i)
	{
		const FactionInfo* F = W.Faction(i);
		VT_REQUIRE(F != nullptr);
		VT_CHECK(F->Region != Capital);
	}
}

VAELEN_TEST(Factions, AGrievanceAnsweredComesToNothing)
{
	// The grievance here has a cause the polity can actually remove: an upkeep
	// it cannot pay, which costs hold in every province at once. Pay it, and
	// the provinces are held well enough again.
	FactionRules Fickle;
	Fickle.NeglectUnderHold = 600;
	Fickle.NeglectYears = 2;
	Fickle.StrengthAtBirth = 200;
	Fickle.StrengthPerYearAggrieved = 40;
	Fickle.StrengthLostPerYear = 400;
	Fickle.RevoltAt = 100000; // it will never take anything: this is about fading
	ReachRules Thin;
	Thin.HoldLostPerHop = 250;
	Thin.HoldFloor = 100;
	Thin.ClaimCost = 60;
	Thin.ReachPerGrain = 100;
	// The upkeep is set far above anything the dues can bring in a year, so
	// that an empty treasury really means an unpaid word - the collector of
	// 07.02 runs before the carrier of 07.03 and would otherwise cover it.
	Thin.UpkeepPerHop = 5000;
	Thin.UnpaidHoldLoss = 250;
	Thin.ReachCeiling = 1; // every province one hop out, so the hold has one value
	Run W(AelvorSeed, Fickle, SuccessionRules{}, Thin);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(3);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	W.Endow(Polity, 4000);
	W.Ages.Run(2);
	VT_CHECK(W.Words_(Thin).Held > 1);

	// Take the money away: the upkeep goes unpaid, every hold falls, and the
	// provinces start counting the years.
	for (uint32 Year = 0; Year < 6; ++Year)
	{
		W.Endow(Polity, 0);
		W.Ages.Run(1);
	}
	const FactionStats Born = W.Parties_(Fickle);
	VT_CHECK(Born.Formed > 0);
	VT_CHECK_EQ(Born.Bad, 0u);
	VAELEN_LOG_INFO(LogFaction, "unpaid: %u formed, %u standing, %u aggrieved, unpaid %llu", Born.Formed, Born.Standing,
					Born.Aggrieved, static_cast<unsigned long long>(W.Words_(Thin).Unpaid));

	// Answer them: fill the treasury so the upkeep is paid again. The hold
	// comes back, the grievance is gone, and the faction comes to nothing.
	for (uint32 Year = 0; Year < 8; ++Year)
	{
		W.Endow(Polity, 400000);
		W.Ages.Run(1);
	}
	const FactionStats Gone = W.Parties_(Fickle);
	VT_CHECK(Gone.Faded > 0);
	VT_CHECK_EQ(Gone.Revolts, 0u);
	VT_CHECK_EQ(Gone.Bad, 0u);
	VAELEN_LOG_INFO(LogFaction, "answered: %u formed, %u faded, %u revolts, %u still standing", Gone.Formed, Gone.Faded,
					Gone.Revolts, Gone.Standing);
	// Nothing stands with no strength left in it.
	for (uint32 i = 1; i <= Gone.Formed; ++i)
	{
		const FactionInfo* F = W.Faction(i);
		if (F != nullptr && F->Ended == 0)
		{
			VT_CHECK(F->Strength > 0);
		}
	}
}

VAELEN_TEST(Factions, RulesAndEdges)
{
	// A polity bears one faction at a time, and never more, however many
	// provinces are aggrieved.
	FactionRules Loud;
	Loud.NeglectUnderHold = 999;
	Loud.NeglectYears = 1;
	Loud.RevoltAt = 100000; // never revolts, so they pile up if they can
	Loud.StrengthPerYearAggrieved = 1;
	ReachRules Thin;
	Thin.HoldFloor = 50;
	Thin.ClaimCost = 40;
	Thin.ReachPerGrain = 60;
	Run W(AelvorSeed, Loud, SuccessionRules{}, Thin);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(3);
	const uint32 Polity = W.FirstPolity();
	VT_REQUIRE(Polity != 0);
	W.Endow(Polity, 60000);
	for (uint32 Year = 0; Year < 12; ++Year)
	{
		W.Ages.Run(1);
		VT_CHECK(W.FactionsIn(Polity).size() <= 1);
		VT_CHECK_EQ(W.Parties_(Loud).Bad, 0u);
	}

	// A strength never stands above its ceiling.
	FactionRules Raging = Loud;
	Raging.StrengthPerYearAggrieved = 100000;
	Raging.StrengthCeiling = 700;
	Raging.RevoltAt = 100000;
	Run R(AelvorSeed, Raging, SuccessionRules{}, Thin);
	VT_REQUIRE(R.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(R.Instance, R.Lod, R.Busiest()));
	R.Ages.Run(3);
	const uint32 P = R.FirstPolity();
	VT_REQUIRE(P != 0);
	R.Endow(P, 60000);
	R.Ages.Run(8);
	VT_CHECK(R.Parties_(Raging).Strongest <= Raging.StrengthCeiling);
	VT_CHECK_EQ(R.Parties_(Raging).Bad, 0u);

	// The lookups refuse what does not exist, and a world with no polity has no
	// faction and nobody's patience.
	VT_CHECK(W.Faction(0xfffffff0u) == nullptr);
	VT_CHECK(W.FactionsIn(0).empty());
	Run Empty(AelvorSeed);
	VT_REQUIRE(Empty.Ages.Generate(Run::Square(64), 40));
	Empty.Ages.Run(3);
	const FactionStats None = Empty.Parties_();
	VT_CHECK_EQ(None.Standing, 0u);
	VT_CHECK_EQ(None.Formed, 0u);
	VT_CHECK_EQ(None.Aggrieved, 0u);
	VT_CHECK_EQ(None.Bad, 0u);

	// Two worlds of one seed raise the same factions in the same years.
	Run A(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(A.Instance, A.Lod, A.Busiest()));
	A.Ages.Run(25);
	Run C(AelvorSeed);
	VT_REQUIRE(C.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(C.Instance, C.Lod, C.Busiest()));
	C.Ages.Run(25);
	VT_CHECK_EQ(A.Parties_().Digest, C.Parties_().Digest);
}

VAELEN_TEST(Factions, DeterministicSnapshotSafeAndFrozen)
{
	FactionRules Live;
	Live.NeglectUnderHold = 800;
	Live.NeglectYears = 2;
	ReachRules Thin;
	Thin.HoldFloor = 100;
	Thin.ClaimCost = 60;
	Thin.ReachPerGrain = 100;
	Run A(AelvorSeed, Live, SuccessionRules{}, Thin);
	Run B(AelvorSeed, Live, SuccessionRules{}, Thin);
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
		const FactionStats S = A.Parties_(Live);
		if (S.Bad != 0 || A.Parties_(Live).Digest != B.Parties_(Live).Digest)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad, factions %s", Year, S.Bad,
						 A.Parties_(Live).Digest == B.Parties_(Live).Digest ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const FactionStats S = A.Parties_(Live);
	VAELEN_LOG_INFO(LogFaction,
					"frozen: factions128=%016llx formed=%u revolts=%u (%u faded, %u standing, %u aggrieved)",
					static_cast<unsigned long long>(S.Digest), S.Formed, S.Revolts, S.Faded, S.Standing, S.Aggrieved);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_FACTION_FROZEN_128});
	VT_CHECK_EQ(S.Formed, uint32{VAELEN_FACTION_FORMED_128});
	VT_CHECK_EQ(S.Revolts, uint32{VAELEN_FACTION_REVOLTS_128});
	VT_REQUIRE(!Image.empty());
	Run T(AelvorSeed, Live, SuccessionRules{}, Thin);
	VT_REQUIRE(LoadSnapshot(T.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	T.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(T.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(T.Parties_(Live).Digest, S.Digest);
}
