// VAELEN - Tests/Politics
// Phase 07.08: the Phase 06 gate - five hundred years of the whole world at
// 256 with one detailed region and every Phase 04, 05 and 06 system running,
// every invariant held every decade, the state, the log and the chronicle
// frozen, and a snapshot of year 250 continuing to the same year 500.
//
// STATUS: VALIDATED (Phase 07)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Decisions.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/SocietyHistory.h"
#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Diplomacy.h"
#include "Vaelen/Politics/Factions.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Politics/PoliticsHistory.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Politics/Succession.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Economy;
using namespace Vaelen::Society;
using namespace Vaelen::Politics;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (06.08): AELVOR 256 at
// year 300, the busiest region detailed, 500 years with every Phase 04, 05
// and 06 system.

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (07.08): AELVOR 256 at
// year 300, its two most peopled regions detailed, 500 years with every
// Phase 04, 05, 06 and 07 system.
// Refrozen 2026-09-08: person indices are taken from a counter that only ever
// goes up, so a demoted region no longer hands its indices out again to the
// people made after it, and the world's one counter entity is state like any
// other. Every invariant of the gate is unchanged; only the state digests are.
#define VAELEN_POLGATE_FROZEN_256_250 0xa289bf9e93673717ull
#define VAELEN_POLGATE_FROZEN_256_500 0x810fde4acd8a1221ull
#define VAELEN_POLGATE_LOG_256_500 0x9dee76991a6bc9c8ull
#define VAELEN_POLGATE_TEXT_256_500 0xa4e5ee1fb64db8abull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPoliticsGate);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	double Seconds(std::chrono::steady_clock::time_point Start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	}

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			PersonRecords = PersonChronicleTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Decisions = DecisionTypes::Declare(Instance);
			State = SocietyChronicleTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			Wealth = WealthTypes::Declare(Instance);
			Ledger = EconomyChronicleTypes::Declare(Instance);
			Polities = PolityTypes::Declare(Instance);
			Laws = LawTypes::Declare(Instance);
			Reaches = ReachTypes::Declare(Instance);
			Heirs = SuccessionTypes::Declare(Instance);
			Parties = FactionTypes::Declare(Instance);
			Treaties = DiplomacyTypes::Declare(Instance);
			Annals = PoliticsChronicleTypes::Declare(Instance);
			Context = SocietyContext{Persons, Families, Organizations};
			Trades = EconomyContext{Persons, Families, Trade, Markets, MarketRules{}, &Context};
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Persons_ = std::make_unique<PersonChronicle>(Instance, Ages.Types(), Persons, Families, PersonRecords,
														 PersonChronicleRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage,
													BondageRules{});
			Acts = std::make_unique<DecisionSystem>(Instance, Ages.Types(), Persons, Traits, Organizations, Decisions,
													DecisionRules{});
			Society_ =
				std::make_unique<SocietyChronicle>(Instance, Ages.Types(), Context, State, SocietyChronicleRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Trade,
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Purses = std::make_unique<WealthSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Norms,
													Wealth, WealthRules{});
			Economy__ =
				std::make_unique<EconomyChronicle>(Instance, Ages.Types(), Trades, Ledger, EconomyChronicleRules{});
			// Phase 07: rule, law, reach, succession, factions, diplomacy, and the
			// chronicle of them all. Wide reach rules, so that two powers on one
			// world grow into each other within five centuries rather than sit
			// still - a gate that never lets its systems meet tests nothing.
			Wide_.ClaimCost = 200;
			Wide_.ReachPerGrain = 400;
			Rulers =
				std::make_unique<PolitySystem>(Instance, Ages.Types(), Persons, Organizations, Polities, PolityRules{});
			Statutes = std::make_unique<LawSystem>(Instance, Ages.Types(), Economy_, Polities, Laws, LawRules{});
			Words = std::make_unique<ReachSystem>(Instance, Ages.Types(), Economy_, Polities, Laws, Reaches, Wide_);
			Lineage = std::make_unique<SuccessionSystem>(Instance, Ages.Types(), Persons, Norms, Polities, Heirs,
														 SuccessionRules{});
			Rebels = std::make_unique<FactionSystem>(Instance, Ages.Types(), Persons, Polities, Reaches, Heirs, Parties,
													 FactionRules{});
			Rebels->ObserveDues(Laws.Dues);
			Envoys =
				std::make_unique<DiplomacySystem>(Instance, Ages.Types(), Trade, Polities, Treaties, DiplomacyRules{});
			Rule_ = PoliticsContext{Persons, Polities, Laws, LawRules{}, Reaches, Heirs, Parties, Treaties, &Trades};
			Annalist =
				std::make_unique<PoliticsChronicle>(Instance, Ages.Types(), Rule_, Annals, PoliticsChronicleRules{});
			Rulers->RunAfter("Lod");
			Statutes->RunAfter("Production");
			Harvest->ObserveDues(Laws.Dues);
			Words->RunAfter("Succession");
			Words->ObserveLine(Heirs.Line);
			Words->ObserveContest(Treaties.Contested);
			// Not Houses->RunAfter("Needs") as the Phase 05 gate had it: once the
			// needs read the economy's ration, that edge closes a cycle -
			// Families -> Needs -> Production -> Stocks -> Families. The chain that
			// carries the grain wins, and the family system forms its marriages on
			// the living of the tick before this year's hunger.
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Body->ObserveStores(Decisions.Stores);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Orgs->RunAfter("Needs");
			Bonds->RunAfter("Lod");
			Ranks->ObserveBonds(Bondage.Bond);
			Ranks->RunAfter("Wealth");
			Ranks->ObserveWealth(Wealth.Wealth);
			Stocks->RunAfter("Lod");
			Stocks->ObserveHeirs(Wealth.Heir);
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveStores(Decisions.Stores);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Acts.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
			Instance.Systems().Add(Purses.get());
			Instance.Systems().Add(Rulers.get());
			Instance.Systems().Add(Statutes.get());
			Instance.Systems().Add(Lineage.get());
			Instance.Systems().Add(Words.get());
			Instance.Systems().Add(Rebels.get());
			Instance.Systems().Add(Envoys.get());
			Persons_->Attach();
			Society_->Attach();
			Economy__->Attach();
			Annalist->Attach();
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
		/// The regions with the most people, most first.
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
						if (P != nullptr && P->Total > People)
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		NeedTypes Needs;
		TraitTypes Traits;
		LodTypes Lod;
		PersonChronicleTypes PersonRecords;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		DecisionTypes Decisions;
		SocietyChronicleTypes State;
		SocietyContext Context;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<PersonChronicle> Persons_;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<DecisionSystem> Acts;
		std::unique_ptr<SocietyChronicle> Society_;
		EconomyTypes Economy_;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		WealthTypes Wealth;
		EconomyChronicleTypes Ledger;
		EconomyContext Trades;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
		std::unique_ptr<WealthSystem> Purses;
		std::unique_ptr<EconomyChronicle> Economy__;
		PolityTypes Polities;
		LawTypes Laws;
		ReachTypes Reaches;
		SuccessionTypes Heirs;
		FactionTypes Parties;
		DiplomacyTypes Treaties;
		PoliticsChronicleTypes Annals;
		PoliticsContext Rule_;
		ReachRules Wide_;
		std::unique_ptr<PolitySystem> Rulers;
		std::unique_ptr<LawSystem> Statutes;
		std::unique_ptr<ReachSystem> Words;
		std::unique_ptr<SuccessionSystem> Lineage;
		std::unique_ptr<FactionSystem> Rebels;
		std::unique_ptr<DiplomacySystem> Envoys;
		std::unique_ptr<PoliticsChronicle> Annalist;
	};

	// Every invariant Phases 05 and 06 promise, on the live state. Returns the failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, Run& W, uint32 Year, uint32 Region, bool WithChronicle)
	{
		uint32 Failures = 0;
		const World& World_ = W.Instance;
		const PreHistoryTypes& T = W.Ages.Types();
		const DetailStats D = MeasureDetail(World_, T, W.Persons);
		if (D.DetailedRegions != 2 || D.Inconsistent != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u detailed regions, %u inconsistent", Year, D.DetailedRegions,
						 D.Inconsistent);
		}
		const LifeStats L = MeasureLives(World_, W.Persons, Region, World_.Now());
		if (L.Alive == 0 || L.Oldest >= 110)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u alive, oldest %u", Year, L.Alive, L.Oldest);
		}
		const OrganizationStats O = MeasureOrganizations(World_, T, W.Persons, W.Organizations);
		if (O.Astray != 0 || O.CountMismatch != 0 || O.HeadsAlive != O.Alive)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u astray memberships, %u count mismatches, %u heads for %u organisations",
						 Year, O.Astray, O.CountMismatch, O.HeadsAlive, O.Alive);
		}
		const StandingStats R = MeasureStanding(World_, W.Persons, W.Standing, Region);
		if (R.Stale != 0 || R.PerTier[0] + R.PerTier[1] + R.PerTier[2] != R.Ranked)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale standings", Year, R.Stale);
		}
		const NormStats N = MeasureNorms(World_, T, W.Norms);
		if (N.WithNorms != N.Cultures || N.MirrorMismatch != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u of %u cultures with customs, %u mirrors wrong", Year, N.WithNorms,
						 N.Cultures, N.MirrorMismatch);
		}
		const BondageStats B = MeasureBondage(World_, T, W.Persons, W.Bondage, Region);
		const RegionStrata* St = StrataOf(World_, T, W.Bondage, Region);
		if (B.Stale != 0 || B.HolderLost != 0 || St == nullptr || St->Bonded != B.Bonded ||
			St->Enslaved != B.Enslaved || St->Free + St->Bonded + St->Enslaved != L.Alive)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u stale bonds, %u holders lost, strata %s", Year, B.Stale, B.HolderLost,
						 St == nullptr ? "missing" : "wrong");
		}
		// Every bond's entry and every caused decision resolve to an event in the log.
		uint32 BadCause = 0;
		for (const Event& E : World_.Log().All())
		{
			if (!E.Cause.IsValid() || !(E.Is(BondEnteredEvent) || E.Is(BondLeftEvent) || E.Is(DecisionMadeEvent)))
			{
				continue;
			}
			BadCause += FindEvent(World_.Log(), E.Cause) == nullptr ? 1u : 0u;
		}
		if (BadCause != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u society causes missing from the log", Year, BadCause);
		}
		// Phase 06: nothing stale or orphaned, a market wherever a stock is, prices
		// within their bounds, a ration that is a ration, roads between real markets.
		const StockStats S = MeasureStocks(World_, T, W.Persons, W.Families, W.Economy_, 0);
		const WealthStats P = MeasureWealth(World_, W.Families, W.Wealth, 0);
		const TradeStats Tr = MeasureTrade(World_, T, W.Trade, TradeRules{});
		const MarketStats M = MeasureMarkets(World_, T, W.Markets, 0);
		const ProductionStats F = MeasureProduction(World_, T, W.Production, 0);
		const MarketRules Prices;
		uint32 Bad = S.Stale + P.Stale + Tr.Bad;
		Bad += M.Markets != S.RegionsWithStock ? 1u : 0u;
		Bad += F.RationMin > 1000 ? 1u : 0u;
		for (uint32 g = 0; g < GoodCount; ++g)
		{
			const uint32 Floor = Prices.BasePrice[g] * Prices.FloorPerMille / 1000u;
			const uint32 Ceiling = Prices.BasePrice[g] * Prices.CeilingPerMille / 1000u;
			Bad += M.Markets > 0 && (M.Lowest[g] < Floor || M.Highest[g] > Ceiling) ? 1u : 0u;
		}
		if (Bad != 0)
		{
			++Failures;
			VT_CHECK_MSG(false,
						 "year %u: %u stale stocks, %u stale purses, %u bad roads, %u markets for %u stocks, ration %u",
						 Year, S.Stale, P.Stale, Tr.Bad, M.Markets, S.RegionsWithStock, F.RationMin);
		}
		// Phase 07: nothing ruled by a polity that is gone, nothing demanded of a
		// region nobody rules, every ruled region carrying the authority of its
		// master, no line that disagrees with its polity, no faction in ground its
		// polity does not hold, no relation with a power that has ended.
		const PolityStats Po = MeasurePolities(World_, T, W.Persons, W.Organizations, W.Polities);
		const LawStats La = MeasureLaws(World_, T, W.Polities, W.Laws, LawRules{});
		const ReachStats Re = MeasureReach(World_, T, W.Polities, W.Reaches, W.Wide_);
		const SuccessionStats Su = MeasureSuccession(World_, W.Polities, W.Persons, W.Heirs, SuccessionRules{});
		const FactionStats Fa = MeasureFactions(World_, T, W.Persons, W.Polities, W.Parties, FactionRules{});
		const DiplomacyStats Di = MeasureDiplomacy(World_, T, W.Polities, W.Treaties, DiplomacyRules{});
		const uint32 Rule = Po.Bad + La.Bad + Re.Bad + Su.Bad + Fa.Bad + Di.Bad + (Re.Held != Po.Ruled ? 1u : 0u) +
							(La.Taxed > Po.Ruled ? 1u : 0u);
		if (Rule != 0)
		{
			++Failures;
			VT_CHECK_MSG(false,
						 "year %u: %u bad polities, %u laws, %u reaches, %u lines, %u factions, %u relations; %u held "
						 "for %u ruled, %u taxed",
						 Year, Po.Bad, La.Bad, Re.Bad, Su.Bad, Fa.Bad, Di.Bad, Re.Held, Po.Ruled, La.Taxed);
		}
		if (!WithChronicle)
		{
			return Failures;
		}
		const ChronicleStats C = CheckChronicle(World_, T);
		const PersonChronicleStats PC = CheckPersonChronicle(World_, T, W.Persons, W.Families, W.PersonRecords);
		const SocietyChronicleStats SC = CheckSocietyChronicle(World_, T, W.Context, W.State);
		const EconomyChronicleStats EC = CheckEconomyChronicle(World_, T, W.Trades, W.Ledger);
		const PoliticsChronicleStats AC = CheckPoliticsChronicle(World_, T, W.Rule_, W.Annals);
		if (C.Resolved != C.Records || C.EraConsistent != C.Records || PC.Described != PC.Records ||
			SC.Described != SC.Records || EC.Described != EC.Records || EC.EraConsistent != EC.Described ||
			AC.Described != AC.Records || AC.EraConsistent != AC.Records)
		{
			++Failures;
			VT_CHECK_MSG(false,
						 "year %u: chronicle %u/%u resolved, %u/%u person, %u/%u society, %u/%u economy and %u/%u "
						 "politics records described",
						 Year, C.Resolved, C.Records, PC.Described, PC.Records, SC.Described, SC.Records, EC.Described,
						 EC.Records, AC.Described, AC.Records);
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(PoliticsGate, FiveCenturiesOfTwoPowersAt256HoldEveryInvariantAndFreeze)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(256), 300));
	// Two powers, so that the whole phase is exercised: a world with one polity
	// has no diplomacy, no war and no ground that can change hands.
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 2);
	const uint32 First = Ranked[0];
	const uint32 Second = Ranked[1];
	VT_CHECK(RequestDetail(W.Instance, W.Lod, First));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Second));
	const auto Start = std::chrono::steady_clock::now();
	uint32 Failures = 0;
	Hash64 At250 = 0;
	std::vector<uint8> Image;
	for (uint32 Decade = 1; Decade <= 50; ++Decade)
	{
		W.Ages.Run(10);
		Failures += CheckInvariants(Ctx, W, Decade * 10, First, Decade % 5 == 0);
		if (Failures > 20)
		{
			break;
		}
		if (Decade % 10 == 0)
		{
			const PolityStats Po = MeasurePolities(W.Instance, W.Ages.Types(), W.Persons, W.Organizations, W.Polities);
			const LawStats La = MeasureLaws(W.Instance, W.Ages.Types(), W.Polities, W.Laws, LawRules{});
			const ReachStats Re = MeasureReach(W.Instance, W.Ages.Types(), W.Polities, W.Reaches, W.Wide_);
			const SuccessionStats Su = MeasureSuccession(W.Instance, W.Polities, W.Persons, W.Heirs, SuccessionRules{});
			const FactionStats Fa =
				MeasureFactions(W.Instance, W.Ages.Types(), W.Persons, W.Polities, W.Parties, FactionRules{});
			const DiplomacyStats Di =
				MeasureDiplomacy(W.Instance, W.Ages.Types(), W.Polities, W.Treaties, DiplomacyRules{});
			const PoliticsChronicleStats AC = CheckPoliticsChronicle(W.Instance, W.Ages.Types(), W.Rule_, W.Annals);
			VAELEN_LOG_INFO(LogPoliticsGate,
							"year %u: %u polities standing (%u ended), %u regions ruled, %llu grain taken, %u rulers "
							"(%u disputed), %u revolts, %u wars, %u annexed, %u political records",
							Decade * 10, Po.Standing, Po.Dissolved, Po.Ruled, static_cast<unsigned long long>(La.Held),
							Su.Rulers, Su.Disputes, Fa.Revolts, Di.Wars, Re.Annexed, AC.Records);
		}
		if (Decade == 25)
		{
			At250 = ComputeStateDigest(W.Instance);
			SaveSnapshot(W.Instance, Image);
		}
	}
	const double Elapsed = Seconds(Start);
	VT_CHECK_EQ(Failures, 0u);
	const Hash64 At500 = ComputeStateDigest(W.Instance);
	const Hash64 Log = W.Instance.Log().Digest();
	std::string Text;
	ExportChronicleWithPolitics(W.Instance, W.Ages.Types(), W.Rule_, Text, 0);
	const Hash64 TextDigest = HashString(Text);
	VAELEN_LOG_INFO(LogPoliticsGate,
					"gate: 500 years at 256 with regions %u and %u detailed in %.1f s [asserts %s]; frozen: "
					"250=%016llx 500=%016llx log=%016llx text=%016llx",
					First, Second, Elapsed, VAELEN_ASSERTS_ENABLED ? "on" : "off",
					static_cast<unsigned long long>(At250), static_cast<unsigned long long>(At500),
					static_cast<unsigned long long>(Log), static_cast<unsigned long long>(TextDigest));

	// The politics of five centuries really happened: polities founded and
	// ended, rulers seated, grain taken, ground held, and a chronicle that says
	// so in the world's own words.
	const PolityStats Po = MeasurePolities(W.Instance, W.Ages.Types(), W.Persons, W.Organizations, W.Polities);
	const LawStats La = MeasureLaws(W.Instance, W.Ages.Types(), W.Polities, W.Laws, LawRules{});
	const SuccessionStats Su = MeasureSuccession(W.Instance, W.Polities, W.Persons, W.Heirs, SuccessionRules{});
	const PoliticsChronicleStats AC = CheckPoliticsChronicle(W.Instance, W.Ages.Types(), W.Rule_, W.Annals);
	VT_CHECK(Po.Standing + Po.Dissolved >= 2);
	VT_CHECK(Po.Ruled > 0);
	VT_CHECK(La.Held > 0 && La.Paid > 0);
	VT_CHECK(Su.Rulers > 10);
	VT_CHECK(AC.Records > 0 && AC.Described == AC.Records);
	VT_CHECK(Text.find(" was founded in ") != std::string::npos);
	VT_CHECK(Text.find(" in every thousand of the harvest.") != std::string::npos);
	VT_CHECK_EQ(At250, Hash64{VAELEN_POLGATE_FROZEN_256_250});
	VT_CHECK_EQ(At500, Hash64{VAELEN_POLGATE_FROZEN_256_500});
	VT_CHECK_EQ(Log, Hash64{VAELEN_POLGATE_LOG_256_500});
	VT_CHECK_EQ(TextDigest, Hash64{VAELEN_POLGATE_TEXT_256_500});

	// The snapshot of year 250 restored into a fresh object continues to the
	// same year 500 - the same world, the same log, the same story.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(IsDetailed(R.Instance, R.Ages.Types(), R.Persons, First));
	VT_CHECK(IsDetailed(R.Instance, R.Ages.Types(), R.Persons, Second));
	const auto Again = std::chrono::steady_clock::now();
	R.Ages.Run(250);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), At500);
	VT_CHECK_EQ(R.Instance.Log().Digest(), Log);
	std::string TextR;
	ExportChronicleWithPolitics(R.Instance, R.Ages.Types(), R.Rule_, TextR, 0);
	VT_CHECK(TextR == Text);
	VAELEN_LOG_INFO(LogPoliticsGate, "snapshot: %llu bytes at year 250, the same year 500 after %.1f s",
					static_cast<unsigned long long>(Image.size()), Seconds(Again));
}
