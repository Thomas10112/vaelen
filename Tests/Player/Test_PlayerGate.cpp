// VAELEN - Tests/Player
// Phase 10.08: the phase gate - a world at 256 with every system of Phases 04
// to 10 running, a person of it taken and played for a lifetime out of a
// recorded stream of intents, every invariant of every phase checked each
// decade, the whole thing frozen, and the stream replayed into a fresh world of
// the same seed to exactly the same life.
//
// This is the test the whole phase exists to pass. Everything else in Phase 10
// is an argument that a played life is a life of this world and not a second
// simulation beside it; this is the evidence. If the replay diverges by one
// grain of wheat, something in 10.04 or 10.05 wrote to the world from outside
// the simulation, and the phase is wrong.
//
// STATUS: PROTOTYPE (Phase 10)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Infrastructure/Buildings.h"
#include "Vaelen/Infrastructure/Places.h"
#include "Vaelen/Infrastructure/Decay.h"
#include "Vaelen/Infrastructure/InfrastructureHistory.h"
#include "Vaelen/Infrastructure/Logistics.h"
#include "Vaelen/Infrastructure/Roads.h"
#include "Vaelen/Infrastructure/Works.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/Toll.h"
#include "Vaelen/Military/War.h"
#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Society/SocietyHistory.h"
#include "Vaelen/Politics/PoliticsHistory.h"
#include "Vaelen/Military/MilitaryHistory.h"
#include "Vaelen/Military/Battle.h"
#include "Vaelen/Military/Siege.h"
#include "Vaelen/Military/March.h"
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
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Doings.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerHistory.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <chrono>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Infrastructure;
using namespace Vaelen::Player;
using namespace Vaelen::Military;
using namespace Vaelen::Politics;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-08 (09.08): AELVOR 256 at year
// 300, the two most peopled regions detailed, five centuries with every Phase
// 04 to 09 system running (506 s with assertions on).
// Recorded on clang 18 / Linux x86_64 on 2026-09-09 (10.08): AELVOR 256, three
// hundred years of pre-history, two regions detailed, sixty years more, then
// forty years played a day at a time across three lives - 14400 intents and 3
// takings - in 106.4 s with assertions on.
// The two state digests were re-recorded when the hold of 04.06 moved from
// LodTypes to PlayerTypes: the Player world registers one component type more
// than it did, and a state digest counts type ids. The LOG and the LIFE below
// did NOT move by that change and were not re-recorded - the history this world
// wrote and the life that was lived in it are the same ones, which is the
// evidence that the change was to the bookkeeping and not to the world.
#define VAELEN_PLAYERGATE_FROZEN_HALF 0x1346aac980b8c6b7ull
#define VAELEN_PLAYERGATE_FROZEN_END 0xd51dc2a7d4a468e8ull
#define VAELEN_PLAYERGATE_LOG 0x4a86e3f3bc62df03ull
#define VAELEN_PLAYERGATE_LIFE 0x7105a2243db1481eull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPlayerGate);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BuildingRules InBuilds = BuildingRules{}, WorksRules InWorth = WorksRules{},
					 PlaceRules InTowns = PlaceRules{}, RoadRules InWays = RoadRules{},
					 DecayRules InYears = DecayRules{}, LogisticsRules InLanes = LogisticsRules{},
					 WorksChronicleRules InAnnals = WorksChronicleRules{}, ArmyRules InHosts = ArmyRules{},
					 DiplomacyRules InTreaties = DiplomacyRules{}, FactionRules InFactions = FactionRules{},
					 SuccessionRules InLine = SuccessionRules{}, ReachRules InReach = ReachRules{},
					 LawRules InLaws = LawRules{}, PolityRules InRules = PolityRules{},
					 MarchRules InColumns = MarchRules{}, BattleRules InFields = BattleRules{},
					 SiegeRules InWalls = SiegeRules{})
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
			Bondage = BondageTypes::Declare(Instance);
			One = PlayerTypes::Declare(Instance);
			First = StartTypes::Declare(Instance);
			Clock = HourTypes::Declare(Instance);
			Queue = OrderTypes::Declare(Instance);
			Known = RegardTypes::Declare(Instance);
			Told = LifeChronicleTypes::Declare(Instance);
			Wealth = WealthTypes::Declare(Instance);
			Polities = PolityTypes::Declare(Instance);
			Laws = LawTypes::Declare(Instance);
			Reaches = ReachTypes::Declare(Instance);
			Heirs = SuccessionTypes::Declare(Instance);
			Parties = FactionTypes::Declare(Instance);
			Treaties = DiplomacyTypes::Declare(Instance);
			Hosts = ArmyTypes::Declare(Instance);
			Orders = MarchTypes::Declare(Instance);
			Fields = BattleTypes::Declare(Instance);
			Walls = SiegeTypes::Declare(Instance);
			Stores = Instance.Types().Register<RegionStores>("RegionStores"); // a council's granary (05.05)
			Instance.Components().CreatePool(Stores);
			Builds = InfrastructureTypes::Declare(Instance);
			Shops = WorksTypes::Declare(Instance);
			Towns = PlaceTypes::Declare(Instance);
			Ways = RoadTypes::Declare(Instance);
			Lanes = LogisticsTypes::Declare(Instance);
			Quarrels = WarTypes::Declare(Instance);
			Cost = TollTypes::Declare(Instance);
			Records = WorksChronicleTypes::Declare(Instance);
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
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage,
													BondageRules{});
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
			Envoys = std::make_unique<DiplomacySystem>(Instance, Ages.Types(), Trade, Polities, Treaties, InTreaties);
			Marshals = std::make_unique<ArmySystem>(Instance, Ages.Types(), Economy, Polities, Laws, Reaches, Treaties,
													Hosts, InHosts);
			Columns = std::make_unique<MarchSystem>(Instance, Ages.Types(), Economy, Polities, Reaches, Treaties, Hosts,
													Orders, InColumns);
			Swords = std::make_unique<BattleSystem>(Instance, Ages.Types(), Polities, Reaches, Treaties, Hosts, Orders,
													Fields, InFields);
			Ramparts = std::make_unique<SiegeSystem>(Instance, Ages.Types(), Polities, Reaches, Treaties, Hosts, Orders,
													 Walls, InWalls);
			Terms = std::make_unique<WarSystem>(Instance, Ages.Types(), Polities, Treaties, Fields, Hosts, Orders,
												Quarrels, WarRules{});
			Reckoning = std::make_unique<TollSystem>(Instance, Ages.Types(), Persons, Standing, Polities, Orders, Cost,
													 TollRules{});
			Ranks->ObserveService(Standing.Service);
			Masons =
				std::make_unique<BuildingSystem>(Instance, Ages.Types(), Families, Economy, Polities, Builds, InBuilds);
			Wrights = std::make_unique<WorksSystem>(Instance, Ages.Types(), Builds, Shops, InWorth);
			Surveyors = std::make_unique<PlaceSystem>(Instance, Ages.Types(), Trade, Builds, Towns, InTowns);
			Pavers = std::make_unique<RoadSystem>(Instance, Ages.Types(), Families, Economy, Trade, Ways, InWays);
			Years_ = std::make_unique<DecaySystem>(Instance, Ages.Types(), Families, Economy, Builds, InYears);
			// The year takes its toll first; the region then sees what it actually has.
			Years_->ObserveWar(Polities, Hosts, Walls);
			Masons->RunAfter("Decay");
			Carters = std::make_unique<LogisticsSystem>(Instance, Ages.Types(), Trade, Ways, Lanes, InWays, InLanes);
			// The one number, read by the two systems that walk the ground.
			// The chronicle of the topmost layer speaks for every layer under it.
			SocietyCtx = SocietyContext{Persons, Families, Organizations};
			EconomyCtx = EconomyContext{Persons, Families, Trade, Markets, MarketRules{}, &SocietyCtx};
			PoliticsCtx =
				PoliticsContext{Persons, Polities, Laws, LawRules{}, Reaches, Heirs, Parties, Treaties, &EconomyCtx};
			MilitaryCtx = MilitaryContext{Hosts, Orders, Fields, Walls, Quarrels, Cost, &PoliticsCtx};
			WorksCtx = WorksContext{Builds, Towns, Ways, &MilitaryCtx};
			Annalist = std::make_unique<WorksChronicle>(Instance, Ages.Types(), WorksCtx, Records, InAnnals);
			Words->ObserveWays(Lanes.Ways);
			Columns->ObserveWays(Lanes.Ways);
			// A road is read by the trade of the year after it is cut.
			Roads->ObserveRoads(Ways.Ease);
			// A year's building comes after that year's harvest and its polities;
			// what it is worth is written straight after, and read the year after
			// that by everything below.
			Masons->RunAfter("Polities");
			Wrights->ObserveStores(Stores);
			Harvest->ObserveWorkshops(Shops.Shops);
			Body->ObserveStores(Stores);
			Ramparts->ObserveWalls(Shops.Walls);
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
			Instance.Systems().Add(Marshals.get());
			Instance.Systems().Add(Columns.get());
			Instance.Systems().Add(Swords.get());
			Instance.Systems().Add(Ramparts.get());
			Instance.Systems().Add(Terms.get());
			Instance.Systems().Add(Reckoning.get());
			Instance.Systems().Add(Masons.get());
			Instance.Systems().Add(Wrights.get());
			Instance.Systems().Add(Surveyors.get());
			Instance.Systems().Add(Pavers.get());
			Instance.Systems().Add(Years_.get());
			Instance.Systems().Add(Carters.get());
			// Phase 10: somebody to be (05.04 binds them), a day at a time, what
			// they mean to do, what they do, and what the world makes of it.
			Bonds->RunAfter("Lod");
			Days_ = std::make_unique<PlayerDaySystem>(Instance, Ages.Types(), Persons, One, Clock, HourRules{});
			Acts_ =
				std::make_unique<PlayerOrderSystem>(Instance, Ages.Types(), Persons, One, Clock, Queue, OrderRules{});
			Hands = std::make_unique<Doings>(Ages.Types(), Persons, Families, Needs, Economy, DoingRules{});
			Acts_->ObserveDoing(Hands.get());
			Talk = std::make_unique<RegardSystem>(Instance, Ages.Types(), Persons, One, Standing, Known, RegardRules{});
			// The bridge honours the hold only when it is told to look for it,
			// which is what keeps every world without a player unchanged.
			Bridge->ObserveHeld(One.Held);
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Days_.get());
			Instance.Systems().Add(Acts_.get());
			Instance.Systems().Add(Talk.get());
			Chron.Persons = Persons;
			Chron.Families = Families;
			Chron.Player = One;
			Chron.Regard = Known;
			Chron.Works = &WorksCtx; // the topmost describer speaks for every layer under it
			Told_ = std::make_unique<LifeChronicle>(Instance, Ages.Types(), Chron, Told, LifeChronicleRules{});
			Instance.Build();
			Told_->Attach();
			Annalist->Attach();
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
		ArmyStats Hosts_(ArmyRules R = ArmyRules{}) const
		{
			return MeasureArmies(Instance, Ages.Types(), Polities, Hosts, R);
		}
		const ArmyInfo* Army(uint32 Index) const { return ArmyOf(Instance, Hosts, Index); }
		const RegionLevy* Levy(uint32 Region) const { return LevyOf(Instance, Ages.Types(), Hosts, Region); }
		MarchStats Columns_(MarchRules R = MarchRules{}) const
		{
			return MeasureMarches(Instance, Ages.Types(), Polities, Treaties, Hosts, Orders, R);
		}
		const MarchOrder* Order(uint32 Army) const { return OrderOf(Instance, Hosts, Orders, Army); }
		const RegionForage* Forage(uint32 Region) const { return ForageOf(Instance, Ages.Types(), Orders, Region); }
		BattleStats Fields_(BattleRules R = BattleRules{}) const
		{
			return MeasureBattles(Instance, Ages.Types(), Fields, R);
		}
		const BattleInfo* Battle(uint32 Index) const { return BattleOf(Instance, Fields, Index); }
		SiegeStats Walls_(SiegeRules R = SiegeRules{}) const { return MeasureSieges(Instance, Ages.Types(), Walls, R); }
		LogisticsStats Lanes_(RoadRules R = RoadRules{}, LogisticsRules L = LogisticsRules{}) const
		{
			return MeasureLogistics(Instance, Ages.Types(), Trade, Ways, Lanes, R, L);
		}
		uint32 Ease(uint32 Region) const { return EaseOf(Instance, Ages.Types(), Lanes, Region); }
		/// A region a host that is not its ruler's stands on, with a work standing
		/// there, and the kind of that work. Region 0 when there is none.
		std::pair<uint32, uint32> ForeignHostOnWorks() const
		{
			std::pair<uint32, uint32> Out{0u, 0u};
			Instance.Components()
				.GetPool(Hosts.Army)
				.ForEach(
					[&](EntityHandle, const ArmyInfo& A)
					{
						if (Out.first != 0 || A.Disbanded != 0 || A.Strength == 0 || A.Region == 0)
						{
							return;
						}
						const RegionRule* Rule = RuleOf(Instance, Ages.Types(), Polities, A.Region);
						if (Rule == nullptr || Rule->Polity == A.Polity)
						{
							return; // its own ground wears nothing extra
						}
						for (uint32 K = 0; K < WorkCount; ++K)
						{
							if (Repair(A.Region, K) > 0)
							{
								Out = {A.Region, K};
								return;
							}
						}
					});
			return Out;
		}
		WorksChronicleStats Annals_() const { return CheckWorksChronicle(Instance, Ages.Types(), WorksCtx, Records); }
		WarStats Quarrels_() const { return MeasureWars(Instance, Quarrels, WarRules{}); }
		TollStats Cost_() const { return MeasureToll(Instance, Ages.Types(), Standing, Cost, TollRules{}); }
		uint32 Chronicle(std::string& Out, uint32 MaxLines = 0) const
		{
			return ExportChronicleWithWorks(Instance, Ages.Types(), WorksCtx, Out, MaxLines);
		}
		uint32 Why_(PersistentId Id, std::string& Out) const
		{
			return ExportWhyWithWorks(Instance, Ages.Types(), WorksCtx, Id, Out);
		}
		void Describe(const Event& E, std::string& Out) const
		{
			DescribeWorksEvent(Instance, Ages.Types(), WorksCtx, E, Out);
		}
		/// Put a made road of that grade on a route touching a region, by hand, as
		/// the two ends would have done had they had the timber. False when the
		/// region has no route to make anything of.
		bool Pave(uint32 Region, uint32 Grade)
		{
			bool Done = false;
			Instance.Components()
				.GetPool(Trade.Route)
				.ForEach(
					[&](EntityHandle H, const RouteInfo& R)
					{
						if (Done || R.Closed != 0 || (R.From != Region && R.To != Region))
						{
							return;
						}
						if (Instance.Components().GetPool(Ways.Road).TryGet(H) != nullptr)
						{
							return;
						}
						RoadInfo Fresh;
						Fresh.Route = R.Index;
						Fresh.From = R.From;
						Fresh.To = R.To;
						Fresh.Grade = Grade;
						Fresh.Repair = 1000;
						Fresh.Cut = Instance.Now();
						Fresh.Seen = R.Carried;
						Instance.Components().GetPool(Ways.Road).Add(H, Fresh);
						Done = true;
					});
			return Done;
		}
		/// A region a standing host is on that a road could be run to, 0 when none.
		uint32 HostOnARoutedRegion() const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Hosts.Army)
				.ForEach(
					[&](EntityHandle, const ArmyInfo& A)
					{
						if (Out != 0 || A.Disbanded != 0 || A.Strength == 0 || A.Region == 0)
						{
							return;
						}
						bool Routed = false;
						Instance.Components()
							.GetPool(Trade.Route)
							.ForEach(
								[&](EntityHandle, const RouteInfo& R)
								{ Routed = Routed || (R.Closed == 0 && (R.From == A.Region || R.To == A.Region)); });
						// And one that actually ate off it: a host sitting at its
						// own seat under no order never forages, so it would
						// prove nothing either way.
						if (Routed && ForageOf(Instance, Ages.Types(), Orders, A.Region) != nullptr)
						{
							Out = A.Region;
						}
					});
			return Out;
		}
		/// The region a made road serves best, 0 when none is served.
		uint32 BestServed() const
		{
			uint32 Best = 0;
			uint32 Where = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const Politics::RegionWays* Held = Instance.Components().GetPool(Lanes.Ways).TryGet(H);
						const uint32 Ease_ = Held != nullptr ? Held->EasePerMille : 0u;
						if (Ease_ > Best || (Ease_ == Best && Ease_ != 0 && R.Index < Where))
						{
							Best = Ease_;
							Where = R.Index;
						}
					});
			return Where;
		}
		DecayStats Wear(DecayRules R = DecayRules{}) const { return MeasureDecay(Instance, Builds, R); }
		/// The repair of the work of that kind standing in a region, 0 when none is.
		uint32 Repair(uint32 Region, uint32 Kind) const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Builds.Building)
				.ForEach(
					[&](EntityHandle, const BuildingInfo& B)
					{
						if (B.Region == Region && B.Kind == Kind && B.Fell == 0)
						{
							Out = B.Repair;
						}
					});
			return Out;
		}
		/// A region a breaking blow struck on this exact tick, with a work standing
		/// on it, and the kind of that work. Region 0 when there is none.
		std::pair<uint32, uint32> StruckWithWorks() const
		{
			std::pair<uint32, uint32> Out{0u, 0u};
			Instance.Components()
				.GetPool(Ages.Types().Disasters.Disaster)
				.ForEach(
					[&](EntityHandle, const DisasterInfo& D)
					{
						if (Out.first != 0 || D.Struck + TicksPerYear < Instance.Now() || D.Severity == 0)
						{
							return;
						}
						const bool Breaks = D.Kind == static_cast<uint32>(DisasterKind::Flood) ||
											D.Kind == static_cast<uint32>(DisasterKind::Eruption);
						if (!Breaks)
						{
							return;
						}
						for (uint32 K = 0; K < WorkCount; ++K)
						{
							if (Repair(D.Region, K) > 0)
							{
								Out = {D.Region, K};
								return;
							}
						}
					});
			return Out;
		}
		bool Ruin(uint32 Region, Work Kind) const { return HasRuin(Instance, Builds, Region, Kind); }
		std::vector<uint32> Ruins(uint32 Region, Work Kind) const
		{
			std::vector<uint32> Out;
			RuinsIn(Instance, Builds, Region, Kind, Out);
			return Out;
		}
		RoadStats Ways_(RoadRules R = RoadRules{}) const { return MeasureRoads(Instance, Trade, Ways, R); }
		const RoadInfo* Road(uint32 Route) const { return RoadOn(Instance, Ways, Route); }
		const RoadInfo* Between(uint32 A, uint32 B) const { return RoadBetween(Instance, Trade, Ways, A, B); }
		/// Units carried on every route of the world, out of the log.
		uint64 CarriedEver() const
		{
			uint64 Sum = 0;
			for (const Event& E : Instance.Log().All())
			{
				if (E.Is(GoodsCarriedEvent))
				{
					Sum += E.Get<TradePayload>().Amount;
				}
			}
			return Sum;
		}
		PlaceStats Towns_(PlaceRules R = PlaceRules{}) const
		{
			return MeasurePlaces(Instance, Ages.Types(), Trade, Builds, Towns, R);
		}
		const PlaceInfo* Place(uint32 Settlement) const { return PlaceOf(Instance, Towns, Settlement); }
		const PlaceInfo* TownIn(uint32 Region) const { return PlaceIn(Instance, Towns, Region); }
		const BuildingPlace* Stands(uint32 Building) const
		{
			return PlaceOfBuilding(Instance, Builds, Towns, Building);
		}
		/// The region a tile belongs to, straight off the map.
		uint32 RegionOfTile(uint32 Tile) const
		{
			if (!Instance.Map().IsReady() || Tile >= Instance.Map().Grid().TileCount())
			{
				return 0;
			}
			return Instance.Map().GetLayer(Ages.Types().World.Regions.RegionIndex)[Tile];
		}
		BuildingStats Built(BuildingRules R = BuildingRules{}) const
		{
			return MeasureBuildings(Instance, Ages.Types(), Builds, R);
		}
		WorksStats Worth_(WorksRules R = WorksRules{}) const
		{
			return MeasureWorks(Instance, Ages.Types(), Builds, Shops, R, &Stores);
		}
		uint32 Worth(uint32 Region, Work Kind, WorksRules R = WorksRules{}) const
		{
			return WorthOf(Instance, Ages.Types(), Builds, R, Region, Kind);
		}
		const RegionWorks* Held(uint32 Region) const { return WorksOf(Instance, Ages.Types(), Builds, Region); }
		/// Units of a good added to any stock of the world since it began.
		uint64 MadeEver(Good G) const
		{
			uint64 Sum = 0;
			for (const Event& E : Instance.Log().All())
			{
				if (E.Is(HarvestEvent) && G == Good::Grain)
				{
					Sum += E.Get<StockPayload>().Amount;
				}
			}
			return Sum;
		}
		/// What every region of the world holds of a good, common and houses.
		uint64 HeldEverywhere(Good G) const
		{
			uint64 Sum = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle, const RegionInfo& R)
					{
						uint32 All[GoodCount] = {};
						TotalStock(Instance, Ages.Types(), Families, Economy, R.Index, All);
						Sum += All[static_cast<uint32>(G)];
					});
			return Sum;
		}
		const SiegeInfo* Rampart(uint32 Region) const { return SiegeOf(Instance, Ages.Types(), Walls, Region); }
		/// The last seat stormed: the taker, the seat, its host, and the polity that
		/// lost it. All zero while none has been.
		PolityPayload LastStorm() const
		{
			PolityPayload Out{};
			for (const Event& E : Instance.Log().All())
			{
				if (E.Is(SeatTakenEvent))
				{
					Out = E.Get<PolityPayload>();
				}
			}
			return Out;
		}
		/// The region a host is sitting before, 0 when none is.
		uint32 Besieged() const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const SiegeInfo* Wall = Instance.Components().GetPool(Walls.Siege).TryGet(H);
						if (Wall != nullptr && Wall->Besieger != 0 && Wall->Years > SiegeRules{}.YearsBeforeBreaching &&
							(Out == 0 || R.Index < Out))
						{
							Out = R.Index;
						}
					});
			return Out;
		}
		/// Raise a wall of that size on a region by hand - a real building, so the
		/// summary and the worth are computed from it the way they always are.
		void RaiseWall(uint32 Region, uint32 Size)
		{
			EntityHandle RH;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region && RH.IsNull())
						{
							RH = H;
						}
					});
			if (RH.IsNull() || Size == 0)
			{
				return;
			}
			uint32 Highest = 0;
			Instance.Components()
				.GetPool(Builds.Building)
				.ForEach([&](EntityHandle, const BuildingInfo& B) { Highest = std::max(Highest, B.Index); });
			BuildingInfo Fresh;
			Fresh.Index = Highest + 1u;
			Fresh.Kind = static_cast<uint32>(Work::Wall);
			Fresh.Region = Region;
			Fresh.Size = Size;
			Fresh.Repair = 1000;
			Fresh.Raised = Instance.Now();
			const EntityHandle BH = Instance.CreateEntity(IdKind::Building);
			Instance.Components().GetPool(Builds.Building).Add(BH, Fresh);
		}
		/// What a region's built walls are worth, as the siege system reads it.
		uint32 WallExtra(uint32 Region) const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index != Region)
						{
							return;
						}
						const Military::RegionWall* Stone = Instance.Components().GetPool(Shops.Walls).TryGet(H);
						Out = Stone != nullptr ? Stone->Extra : 0u;
					});
			return Out;
		}
		/// The wall of every region that has one, by region index.
		std::vector<std::pair<uint32, uint32>> Ramparts_() const
		{
			std::vector<std::pair<uint32, uint32>> Out;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const SiegeInfo* Wall = Instance.Components().GetPool(Walls.Siege).TryGet(H);
						if (Wall != nullptr)
						{
							Out.push_back({R.Index, Wall->Wall});
						}
					});
			std::sort(Out.begin(), Out.end());
			return Out;
		}
		std::vector<uint32> BattlesOn(uint32 Region) const
		{
			std::vector<uint32> Out;
			BattlesIn(Instance, Fields, Region, Out);
			return Out;
		}
		/// Every battle on record, in index order.
		std::vector<BattleInfo> AllBattles() const
		{
			std::vector<BattleInfo> Out;
			Instance.Components()
				.GetPool(Fields.Battle)
				.ForEach([&](EntityHandle, const BattleInfo& B) { Out.push_back(B); });
			std::sort(Out.begin(), Out.end(),
					  [](const BattleInfo& A, const BattleInfo& B) { return A.Index < B.Index; });
			return Out;
		}
		/// The index of the first host standing, 0 when none is.
		uint32 FirstHost() const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Hosts.Army)
				.ForEach(
					[&](EntityHandle, const ArmyInfo& A)
					{
						if (A.Disbanded == 0 && (Out == 0 || A.Index < Out))
						{
							Out = A.Index;
						}
					});
			return Out;
		}
		std::vector<uint32> ArmiesIn(uint32 Polity) const
		{
			std::vector<uint32> Out;
			ArmiesOf(Instance, Hosts, Polity, Out);
			return Out;
		}
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
		ArmyTypes Hosts;
		MarchTypes Orders;
		BattleTypes Fields;
		SiegeTypes Walls;
		ComponentType<RegionStores> Stores;
		BondageTypes Bondage;
		PlayerTypes One;
		StartTypes First;
		HourTypes Clock;
		OrderTypes Queue;
		RegardTypes Known;
		LifeChronicleTypes Told;
		LifeContext Chron;
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
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<PlayerDaySystem> Days_;
		std::unique_ptr<PlayerOrderSystem> Acts_;
		std::unique_ptr<RegardSystem> Talk;
		std::unique_ptr<Doings> Hands;
		std::unique_ptr<LifeChronicle> Told_;
		InfrastructureTypes Builds;
		WorksTypes Shops;
		std::unique_ptr<BuildingSystem> Masons;
		PlaceTypes Towns;
		std::unique_ptr<WorksSystem> Wrights;
		RoadTypes Ways;
		std::unique_ptr<PlaceSystem> Surveyors;
		std::unique_ptr<RoadSystem> Pavers;
		LogisticsTypes Lanes;
		WarTypes Quarrels;
		TollTypes Cost;
		WorksChronicleTypes Records;
		SocietyContext SocietyCtx;
		EconomyContext EconomyCtx;
		PoliticsContext PoliticsCtx;
		MilitaryContext MilitaryCtx;
		WorksContext WorksCtx;

		// Phase 10, on top of everything above.
		uint32 Begin(StartRules R = StartRules{})
		{
			// The mark holds them in the fine grain (PlayerTypes::Held), and the
			// bridge is told to honour it in the constructor: a player emigrated
			// to a neighbour is a life that ends without anybody dying, which is
			// how the first run of this gate lost its person in the third year.
			return BeginEnslaved(Instance, Ages.Types(), Persons, Bondage, Standing, One, First, R, Instance.Now());
		}
		bool Open() { return BeginOrders(Instance, One, Queue, Instance.Now()); }
		Refusal Mean(const PlayerCommand& C) { return Submit(Instance, One, Queue, OrderRules{}, C); }
		void Day() { Instance.TickMany(24); }
		uint32 Played() const { return PlayerPerson(Instance, One); }
		const PersonInfo* Person_(uint32 P) const { return FindPerson(Instance, Persons, P); }
		const PersonNeeds* NeedsOf(uint32 Person) const
		{
			const PersonNeeds* Out = nullptr;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (Out == nullptr && P.Index == Person)
						{
							Out = Instance.Components().GetPool(Needs.Needs).TryGet(H);
						}
					});
			return Out;
		}
		/// Somebody else alive in a region, by lowest index: the same choice in
		/// every run of the same world.
		uint32 SomebodyIn(uint32 Region, uint32 Not) const
		{
			uint32 Best = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						if (P.Region == Region && P.Index != Not && P.State == static_cast<uint8>(LifeState::Alive))
						{
							Best = Best == 0 || P.Index < Best ? P.Index : Best;
						}
					});
			return Best;
		}
		PlayerStats Mark_() const { return MeasurePlayer(Instance, Persons, One); }
		HourStats Hours_() const { return MeasureHours(Instance, Persons, One, Clock, HourRules{}); }
		OrderStats Acts() const { return MeasureOrders(Instance, Persons, One, Queue, OrderRules{}); }
		RegardStats Regard() const { return MeasureRegard(Instance, Persons, One, Known, RegardRules{}); }
		DoingStats Did() const { return MeasureDoings(Instance); }
		LifeChronicleStats Kept_() const { return CheckLifeChronicle(Instance, Ages.Types(), Chron, Told); }
		std::string Life() const
		{
			std::string Out;
			ExportLife(Instance, Ages.Types(), Chron, Out, 40);
			return Out;
		}
		std::unique_ptr<DecaySystem> Years_;
		std::unique_ptr<LogisticsSystem> Carters;
		std::unique_ptr<WarSystem> Terms;
		std::unique_ptr<TollSystem> Reckoning;
		std::unique_ptr<WorksChronicle> Annalist;
		std::unique_ptr<ArmySystem> Marshals;
		std::unique_ptr<MarchSystem> Columns;
		std::unique_ptr<BattleSystem> Swords;
		std::unique_ptr<SiegeSystem> Ramparts;
	};
} // namespace
namespace
{
} // namespace

namespace
{
} // namespace

namespace
{
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

	double Seconds(std::chrono::steady_clock::time_point Start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	}

	/// Every invariant of Phase 09, and of every phase it stands on, in one year.
	constexpr uint32 BeforeYears = 60; ///< years of detail before anybody is taken
	constexpr uint32 LifeYears = 40;   ///< a lifetime, or what is left of one
	constexpr uint32 DaysPerYear = 360;

	/// One intent as it was submitted, with the answer the door gave. The whole
	/// of the player's half of the input, and what a replay has to reproduce.
	struct Recorded
	{
		uint64 Tick = 0;
		PlayerCommand Command;
		Refusal Verdict = Refusal::None;
	};

	/// The other thing the outside world does: take somebody up. A world with
	/// mortality in it will not let one person be played for forty years, so a
	/// life ends and another is taken, and the tick that happened on is as much
	/// a part of the input as any intent. Seed plus takings plus intents is the
	/// whole of what a replay is given.
	struct Taking
	{
		uint64 Tick = 0;
		uint32 Person = 0;
	};

	/// What the person means to do today, decided from the world they are in
	/// rather than from a script: hungry, they eat; tired, they rest; otherwise
	/// they work, and now and then they have something to do with somebody.
	/// This is a recording BECAUSE it reads the world - a fixed list of intents
	/// would replay whatever the simulation did.
	PlayerCommand Decide(Run& W, uint32 Who, uint32 Year, uint32 Day)
	{
		PlayerCommand C;
		C.Issued = W.Instance.Now();
		C.Amount = 1 + (Day % 3u);
		const PersonNeeds* N = W.NeedsOf(Who);
		const PersonInfo* P = W.Person_(Who);
		if (N == nullptr || P == nullptr)
		{
			C.Kind = static_cast<uint8>(Intent::Wait); // there is nobody left to be
			return C;
		}
		if (N->Food < 200)
		{
			C.Kind = static_cast<uint8>(Intent::Eat);
			return C;
		}
		if (N->Rest < 120)
		{
			C.Kind = static_cast<uint8>(Intent::Rest);
			return C;
		}
		const uint32 Turn = (Year * DaysPerYear + Day) % 20u;
		if (Turn == 7 || Turn == 13)
		{
			C.Kind = static_cast<uint8>(Turn == 7 ? Intent::Speak : Intent::Give);
			C.Target = W.SomebodyIn(P->Region, Who);
			return C;
		}
		C.Kind = static_cast<uint8>(Intent::Work);
		return C;
	}

	/// Everything Phase 10 must keep, whatever the six phases under it do.
	uint32 CheckPlayerInvariants(VaelenTest::Context& Ctx, const Run& W, uint32 Year)
	{
		uint32 Failures = 0;
		const PlayerStats Mark = W.Mark_();
		const HourStats Hours = W.Hours_();
		const OrderStats Orders_ = W.Acts();
		const RegardStats Regard_ = W.Regard();
		const uint32 Bad = Mark.Bad + Hours.Bad + Orders_.Bad + Regard_.Bad;
		if (Bad != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad marks, %u hours, %u queues, %u regards", Year, Mark.Bad, Hours.Bad,
						 Orders_.Bad, Regard_.Bad);
		}
		// One player, one day, one queue: a world holds one of each or none.
		if (Mark.Marks > 1 || Hours.Records > 1 || Orders_.Queues > 1)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u mark(s), %u day(s), %u queue(s)", Year, Mark.Marks, Hours.Records,
						 Orders_.Queues);
		}
		// Nothing is done that was not meant, and nothing meant is lost: every
		// intent the queue ever took is either done, refused or still waiting.
		const PlayerOrders* Q = OrdersOf(W.Instance, W.Queue);
		if (Q != nullptr && Q->Held > MostOrders)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u intent(s) waiting in a ring of %zu", Year, Q->Held, MostOrders);
		}
		// Every record of a life has a line of its own layer. They are not all
		// of the person being played NOW: a world with mortality in it is
		// played across several lives, and what the earlier ones did stays in
		// the chronicle, which is the whole reason for keeping one.
		const LifeChronicleStats Kept = W.Kept_();
		if (Kept.Described != Kept.Records || Kept.OfThePlayer > Kept.Records)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u record(s), %u of the player, %u described", Year, Kept.Records,
						 Kept.OfThePlayer, Kept.Described);
		}
		return Failures;
	}

	uint32 CheckInvariants(VaelenTest::Context& Ctx, const Run& W, uint32 Year)
	{
		uint32 Failures = 0;
		const BuildingStats Bu = W.Built();
		const WorksStats Wo = W.Worth_();
		const PlaceStats Pl = W.Towns_();
		const RoadStats Ro = W.Ways_();
		const DecayStats De = W.Wear();
		const LogisticsStats Lo = W.Lanes_();
		const uint32 Bad = Bu.Bad + Wo.Bad + Pl.Bad + Ro.Bad + De.Bad + Lo.Bad;
		if (Bad != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad buildings, %u worths, %u places, %u roads, %u decays, %u lanes", Year,
						 Bu.Bad, Wo.Bad, Pl.Bad, Ro.Bad, De.Bad, Lo.Bad);
		}
		// Nothing is invented and nothing is lost: every building the world holds
		// is either standing or a ruin, and the two measures agree on which.
		if (De.Standing != Bu.Standing || De.Fallen != Bu.Ruined || De.Falls != De.Fallen)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u standing against %u, %u fallen against %u, %u falls", Year, De.Standing,
						 Bu.Standing, De.Fallen, Bu.Ruined, De.Falls);
		}
		// Every standing building stands somewhere: in a town, or in the country.
		if (Pl.Works + Pl.Loose != Bu.Standing)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u works in towns and %u loose against %u standing", Year, Pl.Works, Pl.Loose,
						 Bu.Standing);
		}
		// Every place is a settlement with a body, and every road a route with
		// something made of it.
		if (Pl.Places != Pl.Standing + Pl.Emptied || Ro.Roads != Ro.Made + Ro.Tracks)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u places against %u+%u, %u roads against %u+%u", Year, Pl.Places,
						 Pl.Standing, Pl.Emptied, Ro.Roads, Ro.Made, Ro.Tracks);
		}
		// Every record of the chronicle has a sentence and sits in its own era.
		const WorksChronicleStats An = W.Annals_();
		if (An.Described != An.Records || An.EraConsistent != An.Records)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u of %u records described, %u in their own era", Year, An.Described,
						 An.Records, An.EraConsistent);
		}
		// And the ground under it all still adds up: the Phase 07 and 08 measures.
		const ArmyStats Ho = W.Hosts_();
		const MarchStats Ma = W.Columns_();
		const BattleStats Ba = W.Fields_();
		const SiegeStats Si = W.Walls_();
		const WarStats Wa = W.Quarrels_();
		const TollStats To = W.Cost_();
		if (Ho.Bad + Ma.Bad + Ba.Bad + Si.Bad + Wa.Bad + To.Bad != 0 || Ho.Away != Ho.Men)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u/%u/%u/%u/%u/%u bad below, %u away against %u under arms", Year, Ho.Bad,
						 Ma.Bad, Ba.Bad, Si.Bad, Wa.Bad, To.Bad, Ho.Away, Ho.Men);
		}
		const PolityStats Po = MeasurePolities(W.Instance, W.Ages.Types(), W.Persons, W.Organizations, W.Polities);
		const ReachStats Re = MeasureReach(W.Instance, W.Ages.Types(), W.Polities, W.Reaches, WideReach());
		const DiplomacyStats Di =
			MeasureDiplomacy(W.Instance, W.Ages.Types(), W.Polities, W.Treaties, DiplomacyRules{});
		if (Po.Bad + Re.Bad + Di.Bad != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad polities, %u reaches, %u relations", Year, Po.Bad, Re.Bad, Di.Bad);
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(PlayerGate, ALifetimeAt256HoldsEveryInvariantAndFreezes)
{
	// One person of a world of 256 regions, taken out of the people 05.04 had
	// already bound, and lived a day at a time out of a stream of intents while
	// every other system of six phases runs around them at the year.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(256), 300));
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 2);
	const uint32 First_ = Ranked[0];
	const uint32 Second = Ranked[1];
	VT_CHECK(RequestDetail(W.Instance, W.Lod, First_));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Second));
	// Years enough for 05.04 to have bound somebody to be.
	W.Ages.Run(BeforeYears);
	StartRules Anywhere;
	Anywhere.PreferOre = 0;
	// Young, so that the forty years are mostly lived rather than mostly spent
	// dead: the first run of this gate took somebody of forty who died in year
	// ten, and three quarters of it was the refusal path for a corpse.
	Anywhere.ToAge = 25;
	const uint32 Who = W.Begin(Anywhere);
	VT_REQUIRE(Who != 0); // the world offered nobody bound to be
	VT_REQUIRE(W.Open());

	const auto Start = std::chrono::steady_clock::now();
	std::vector<Recorded> Stream;
	std::vector<Taking> Takings{Taking{W.Instance.Now(), Who}};
	uint32 Failures = 0;
	uint32 Died = 0;
	uint32 Ended = 0;
	uint32 Lives = 1;
	uint32 Playing = Who;
	// A life owns its records and they end with it, so the day-count and the
	// tallies of the queue describe the life being lived and not the forty
	// years. What spans the forty years is the sum of them, taken as each life
	// closes and once more at the end.
	uint32 DaysLived = 0;
	uint32 ActsTaken = 0;
	uint32 ActsRefused = 0;
	Hash64 AtHalf = 0;
	for (uint32 Year = 1; Year <= LifeYears; ++Year)
	{
		for (uint32 Day = 0; Day < DaysPerYear; ++Day)
		{
			if (Playing != 0)
			{
				const PlayerCommand C = Decide(W, Playing, Year, Day);
				Stream.push_back(Recorded{C.Issued, C, W.Mean(C)});
			}
			W.Day();
			// A life that ends: the mark comes off, and another of this world's
			// people is taken up. Everything that happens to the first one stays
			// in the world's history exactly as it happened.
			const PersonInfo* Now_ = Playing != 0 ? W.Person_(Playing) : nullptr;
			if (Playing != 0 && (Now_ == nullptr || Now_->State != static_cast<uint8>(LifeState::Alive)))
			{
				Ended = Now_ == nullptr ? 3u : Now_->State;
				Died = Died == 0 ? Year : Died;
				VAELEN_LOG_INFO(LogPlayerGate, "a life ended in year %u, %s", BeforeYears + Year,
								Ended == static_cast<uint32>(LifeState::Dead)
									? "dead"
									: (Ended == static_cast<uint32>(LifeState::Gone) ? "gone from the fine grain"
																					 : "no longer a person at all"));
				// Everything that was of that life ends with it: the mark, the
				// queue, the day, the start, and what people made of them. What
				// they did stays in the world's history, which is the point -
				// so the counts of it are taken before they go.
				DaysLived += W.Hours_().Days;
				ActsTaken += W.Acts().Taken;
				ActsRefused += W.Acts().Refused;
				ReleasePlayer(W.Instance, W.One, &W.Persons);
				EndOrders(W.Instance, W.Queue);
				EndStart(W.Instance, W.First);
				EndHours(W.Instance, W.Clock);
				EndRegard(W.Instance, W.Known);
				const uint32 Next_ = W.Begin(Anywhere);
				Playing = Next_;
				if (Next_ != 0)
				{
					Takings.push_back(Taking{W.Instance.Now(), Next_});
					++Lives;
					VT_CHECK(W.Open());
				}
			}
		}
		if (Year % 10 == 0)
		{
			Failures += CheckInvariants(Ctx, W, BeforeYears + Year);
			Failures += CheckPlayerInvariants(Ctx, W, BeforeYears + Year);
			const OrderStats O = W.Acts();
			const DoingStats D = W.Did();
			const RegardStats R = W.Regard();
			VAELEN_LOG_INFO(LogPlayerGate,
							"year %u: %u day(s) lived (%u missed), %u act(s) taken, %u refused, %u worked, %u ate, "
							"%u known, repute %d, %u record(s)",
							BeforeYears + Year, W.Hours_().Days, W.Hours_().Missed, O.Taken, O.Refused, D.Worked, D.Ate,
							R.Known, R.Repute, W.Kept_().Records);
			if (Failures > 20)
			{
				break;
			}
		}
		if (Year == LifeYears / 2)
		{
			AtHalf = ComputeStateDigest(W.Instance);
		}
	}
	const double Elapsed = Seconds(Start);
	VT_CHECK_EQ(Failures, 0u);
	const Hash64 AtEnd = ComputeStateDigest(W.Instance);
	const Hash64 Log = W.Instance.Log().Digest();
	const std::string Story = W.Life();
	const Hash64 LifeDigest = HashBytes(Story.data(), Story.size());
	const OrderStats Orders_ = W.Acts();
	const DoingStats Done = W.Did();
	VAELEN_LOG_INFO(LogPlayerGate,
					"gate: %u year(s) at 256 played across %u life/lives from person %u of region %u in %.1f s "
					"[asserts %s]; %zu intent(s) and %zu taking(s) recorded, %u taken, %u refused, %u dropped; "
					"first death in year %u; frozen: half=%016llx end=%016llx log=%016llx life=%016llx",
					LifeYears, Lives, Who, First_, Elapsed, VAELEN_ASSERTS_ENABLED ? "on" : "off", Stream.size(),
					Takings.size(), Orders_.Taken, Orders_.Refused, Orders_.Dropped, Died,
					static_cast<unsigned long long>(AtHalf), static_cast<unsigned long long>(AtEnd),
					static_cast<unsigned long long>(Log), static_cast<unsigned long long>(LifeDigest));
	VAELEN_LOG_INFO(LogPlayerGate, "the life:\n%s", Story.c_str());

	// A life really was lived: days turned, things were done, the world moved
	// because of them, and people came to think something of them.
	const HourStats Hours = W.Hours_();
	const RegardStats Regard_ = W.Regard();
	// A life that runs the whole forty years is 14400 days of it; one that ends
	// is as many days as it had years. Either is a life; neither is a fortnight,
	// and a played person who is quietly emigrated in the third year is not one
	// at all, which is what the hold of 04.06 is for.
	// Forty years of a played life, across as many people as this world's
	// mortality demanded: a bound person of a crowded region does not live
	// forty years, and a gate that pretended otherwise would be measuring a
	// world that does not exist. What must hold is that somebody was being
	// played on nearly every one of those days.
	DaysLived += Hours.Days;
	ActsTaken += Orders_.Taken;
	ActsRefused += Orders_.Refused;
	VT_CHECK_MSG(DaysLived > LifeYears * 250u, "the day turned through the whole of the forty years");
	VT_CHECK_MSG(Lives > 1, "and the world's mortality really did end a life and start another");
	VT_CHECK_MSG(Ended != static_cast<uint32>(LifeState::Gone), "the crossings left the played person alone");
	VT_CHECK(ActsTaken > 5000);
	VT_CHECK_MSG(ActsTaken > ActsRefused, "and most of what was meant was done rather than refused");
	VT_CHECK_MSG(Done.Worked > 0 && Done.Ate > 0, "they worked and they ate");
	VT_CHECK_MSG(Done.Caused > 0, "and the world moved because of it");
	VT_CHECK_MSG(Regard_.Known > 0, "and somebody came to think something of them");
	VT_CHECK_MSG(W.Kept_().Records > 0, "and the chronicle kept some of it");
	VT_CHECK_EQ(W.Mark_().Bad, 0u);
	VT_CHECK_EQ(Hours.Bad, 0u);
	VT_CHECK_EQ(Orders_.Bad, 0u);
	VT_CHECK_EQ(Regard_.Bad, 0u);
	VT_CHECK_MSG(Story.find("Year ") != std::string::npos, "and it reads as a life");

	// THE CLAIM OF THE PHASE. A fresh world of the same seed, the same stream of
	// intents submitted on the same ticks, and nothing else: the same life, the
	// same world around it, the same history, word for word.
	Run R(AelvorSeed);
	VT_REQUIRE(R.Ages.Generate(Run::Square(256), 300));
	VT_CHECK(RequestDetail(R.Instance, R.Lod, First_));
	VT_CHECK(RequestDetail(R.Instance, R.Lod, Second));
	R.Ages.Run(BeforeYears);
	usize Next = 0;
	usize Took = 0;
	uint32 Wrong = 0;
	// The replay knows nothing about the world. It takes up whoever the
	// recording says was taken up, on the tick it says, and submits the intents
	// on the ticks they were submitted. Nothing else - and if the world it is
	// rebuilding is not the same world, it will not have the same person to
	// take up, which is a stronger claim than the intents alone ever made.
	auto TakeUp = [&](Run& In)
	{
		while (Took < Takings.size() && Takings[Took].Tick <= In.Instance.Now())
		{
			if (Took > 0)
			{
				ReleasePlayer(In.Instance, In.One, &In.Persons);
				EndOrders(In.Instance, In.Queue);
				EndStart(In.Instance, In.First);
				EndHours(In.Instance, In.Clock);
				EndRegard(In.Instance, In.Known);
			}
			Wrong += In.Begin(Anywhere) == Takings[Took].Person ? 0u : 1u;
			VT_CHECK(In.Open());
			++Took;
		}
	};
	TakeUp(R);
	for (uint32 Year = 1; Year <= LifeYears; ++Year)
	{
		for (uint32 Day = 0; Day < DaysPerYear; ++Day)
		{
			while (Next < Stream.size() && Stream[Next].Tick <= R.Instance.Now())
			{
				Wrong += R.Mean(Stream[Next].Command) == Stream[Next].Verdict ? 0u : 1u;
				++Next;
			}
			R.Day();
			TakeUp(R);
		}
	}
	const std::string Again = R.Life();
	VAELEN_LOG_INFO(LogPlayerGate, "replay: %zu of %zu intent(s) and %zu of %zu taking(s), %u answered differently",
					Next, Stream.size(), Took, Takings.size(), Wrong);
	VT_CHECK_EQ(Next, Stream.size());
	VT_CHECK_EQ(Took, Takings.size());
	VT_CHECK_MSG(Wrong == 0, "the door gave the same answer to every intent");
	VT_CHECK_MSG(ComputeStateDigest(R.Instance) == AtEnd, "and the world came out the same, down to the last grain");
	VT_CHECK_MSG(R.Instance.Log().Digest() == Log, "with the same history in it");
	VT_CHECK_MSG(Again == Story, "and the same life, word for word");
	VT_CHECK_EQ(R.Acts().Digest, Orders_.Digest);

#if VAELEN_PLAYERGATE_FROZEN_END != 0x0ull
	VT_CHECK_EQ(AtHalf, Hash64{VAELEN_PLAYERGATE_FROZEN_HALF});
	VT_CHECK_EQ(AtEnd, Hash64{VAELEN_PLAYERGATE_FROZEN_END});
	VT_CHECK_EQ(Log, Hash64{VAELEN_PLAYERGATE_LOG});
	VT_CHECK_EQ(LifeDigest, Hash64{VAELEN_PLAYERGATE_LIFE});
#endif
}
