// VAELEN - Tests/Infrastructure
// Phase 09.07: infrastructure in the chronicle - a granary raised, a mill
// fallen in, a road cut, a town settled; the small part of six tasks of
// building that a century would keep.
//
// STATUS: VALIDATED (Phase 09)

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
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Infrastructure;
using namespace Vaelen::Military;
using namespace Vaelen::Politics;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-08 (09.07): AELVOR 128 at
// year 300, the two most peopled regions detailed, 250 years with every Phase
// 04 to 09 system.
#define VAELEN_ANNALS_RECORDS_128 707u
#define VAELEN_ANNALS_TEXT_128 0x299790f3a611932bull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogAnnals);

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
			Instance.Systems().Add(Masons.get());
			Instance.Systems().Add(Wrights.get());
			Instance.Systems().Add(Surveyors.get());
			Instance.Systems().Add(Pavers.get());
			Instance.Systems().Add(Years_.get());
			Instance.Systems().Add(Carters.get());
			Instance.Build();
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
		WorksChronicleStats Annals_() const { return CheckWorksChronicle(Instance, Ages.Types(), WorksCtx, Records); }
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
		std::unique_ptr<DecaySystem> Years_;
		std::unique_ptr<LogisticsSystem> Carters;
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
	/// A world grown to 300 years, its two busiest regions detailed, then Years
	/// more on what the land itself gives. No gift of goods here: a region that
	/// is handed timber every year keeps everything it has ever built, and a
	/// world where nothing is ever lost is not what this task is about.
	bool Settled(Run& W, uint32 Years, int32 Gift = 0)
	{
		if (!W.Ages.Generate(Run::Square(128), 300))
		{
			return false;
		}
		const std::vector<uint32> Ranked = W.Ranked();
		if (Ranked.size() < 2 || !RequestDetail(W.Instance, W.Lod, Ranked[0]) ||
			!RequestDetail(W.Instance, W.Lod, Ranked[1]))
		{
			return false;
		}
		for (uint32 Year = 0; Year < Years; ++Year)
		{
			if (Gift != 0)
			{
				W.FillEvery(Gift);
			}
			W.Ages.Run(1);
		}
		return true;
	}
} // namespace

VAELEN_TEST(WorksHistory, TheChronicleKeepsWhatACenturyWouldRemember)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Settled(W, 250, 0));
	const WorksChronicleStats S = W.Annals_();
	VAELEN_LOG_INFO(LogAnnals, "records=%u dropped=%u described=%u raised=%u fallen=%u roads=%u places=%u", S.Records,
					S.Dropped, S.Described, S.ByType[0], S.ByType[1], S.ByType[2], S.ByType[3]);
	VT_CHECK_MSG(S.Records > 0, "two and a half centuries of building leave something a century would keep");
	VT_CHECK_MSG(S.Described == S.Records, "every record kept has a sentence");
	VT_CHECK_MSG(S.WithRegion == S.Records, "every record is filed on a region");
	VT_CHECK_MSG(S.EraConsistent == S.Records, "every record carries the age it happened in");
	VT_CHECK_MSG(S.ByType[0] > 0, "things were raised");
	VT_CHECK_MSG(S.ByType[1] > 0, "and things fell in");

	// An enlargement is not history by default, and a raising is.
	const BuildingStats B = W.Built();
	VT_CHECK_MSG(S.ByType[0] <= B.Raised, "no more raisings kept than there were raisings");
	VT_CHECK_EQ(S.ByType[1], W.Wear().Falls);
}

VAELEN_TEST(WorksHistory, EveryEventHasASentence)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Settled(W, 250, 0));

	// Every infrastructure event of the whole log has a line, whether the
	// chronicle kept it or not.
	uint32 Seen = 0;
	std::string Raised;
	std::string Fell;
	std::string Cut;
	std::string Town;
	for (const Event& E : W.Instance.Log().All())
	{
		const bool Ours = E.Is(BuildingRaisedEvent) || E.Is(BuildingEnlargedEvent) || E.Is(BuildingFellEvent) ||
						  E.Is(RoadCutEvent) || E.Is(RoadLostEvent) || E.Is(PlaceSettledEvent) ||
						  E.Is(PlaceGrewEvent) || E.Is(PlaceEmptiedEvent);
		if (!Ours)
		{
			continue;
		}
		std::string Line;
		W.Describe(E, Line);
		VT_CHECK(!Line.empty());
		++Seen;
		if (E.Is(BuildingRaisedEvent) && Raised.empty())
		{
			Raised = Line;
		}
		if (E.Is(BuildingFellEvent) && Fell.empty())
		{
			Fell = Line;
		}
		if (E.Is(RoadCutEvent) && Cut.empty())
		{
			Cut = Line;
		}
		if (E.Is(PlaceSettledEvent) && Town.empty())
		{
			Town = Line;
		}
	}
	VT_CHECK(Seen > 0);
	VAELEN_LOG_INFO(LogAnnals, "%u infrastructure event(s), every one a sentence", Seen);
	VAELEN_LOG_INFO(LogAnnals, "raised: %s", Raised.c_str());
	VAELEN_LOG_INFO(LogAnnals, "fell:   %s", Fell.c_str());
	VAELEN_LOG_INFO(LogAnnals, "road:   %s", Cut.c_str());
	VAELEN_LOG_INFO(LogAnnals, "town:   %s", Town.c_str());
	VT_CHECK(!Raised.empty());
	VT_CHECK(!Fell.empty());

	// And an event of a layer below gets the sentence of that layer: the
	// chronicle of the topmost layer speaks for every layer under it.
	std::string Lower;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(HarvestEvent))
		{
			continue;
		}
		W.Describe(E, Lower);
		break;
	}
	VAELEN_LOG_INFO(LogAnnals, "below:  %s", Lower.c_str());
	VT_CHECK_MSG(!Lower.empty(), "an infrastructure chronicle can tell you about a harvest");
}

VAELEN_TEST(WorksHistory, TheWhyOfAFallenWork)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Settled(W, 250, 0));

	// A work that fell in the year a flood struck names the flood: the why of a
	// fallen mill is the weather, walked back a step.
	PersistentId Storm;
	PersistentId Quiet;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(BuildingFellEvent))
		{
			continue;
		}
		if (E.Cause.IsValid() && !Storm.IsValid())
		{
			Storm = E.Id;
		}
		if (!E.Cause.IsValid() && !Quiet.IsValid())
		{
			Quiet = E.Id;
		}
	}
	VT_REQUIRE(Quiet.IsValid());
	std::string Why;
	const uint32 Steps = W.Why_(Quiet, Why);
	VAELEN_LOG_INFO(LogAnnals, "why, %u step(s):\n%s", Steps, Why.c_str());
	VT_CHECK_MSG(Steps >= 1, "a fall the years alone brought about is one line, and that is the truth of it");

	if (Storm.IsValid())
	{
		std::string Chain;
		const uint32 Deep = W.Why_(Storm, Chain);
		VAELEN_LOG_INFO(LogAnnals, "why, %u step(s):\n%s", Deep, Chain.c_str());
		VT_CHECK_MSG(Deep >= 2, "a fall the weather finished names the weather");
		VT_CHECK(Chain.find("because") != std::string::npos);
	}
	else
	{
		VAELEN_LOG_INFO(LogAnnals, "no work fell in a year the weather struck it; the gate will see one");
	}
}

VAELEN_TEST(WorksHistory, RulesAndEdges)
{
	// A chronicle that keeps nothing keeps nothing.
	{
		WorksChronicleRules Silent;
		Silent.RecordRaisings = 0;
		Silent.RecordFalls = 0;
		Silent.RecordRoads = 0;
		Silent.RecordPlaces = 0;
		Run W(AelvorSeed, BuildingRules{}, WorksRules{}, PlaceRules{}, RoadRules{}, DecayRules{}, LogisticsRules{},
			  Silent);
		VT_REQUIRE(Settled(W, 120, 0));
		VT_CHECK_EQ(W.Annals_().Records, 0u);
	}
	// A chronicle with room for one record a year drops the rest, and says so.
	{
		WorksChronicleRules Terse;
		Terse.MaxRecordsPerYear = 1;
		Run W(AelvorSeed, BuildingRules{}, WorksRules{}, PlaceRules{}, RoadRules{}, DecayRules{}, LogisticsRules{},
			  Terse);
		VT_REQUIRE(Settled(W, 250, 0));
		const WorksChronicleStats S = W.Annals_();
		Run Full(AelvorSeed);
		VT_REQUIRE(Settled(Full, 250, 0));
		VAELEN_LOG_INFO(LogAnnals, "terse: %u kept, %u dropped; full: %u kept", S.Records, S.Dropped,
						Full.Annals_().Records);
		VT_CHECK(S.Records <= Full.Annals_().Records);
	}
	// Enlargements are kept when they are asked for, and not otherwise.
	{
		WorksChronicleRules Everything;
		Everything.RecordEnlargements = 1;
		Run W(AelvorSeed, BuildingRules{}, WorksRules{}, PlaceRules{}, RoadRules{}, DecayRules{}, LogisticsRules{},
			  Everything);
		VT_REQUIRE(Settled(W, 250, 0));
		Run Plain(AelvorSeed);
		VT_REQUIRE(Settled(Plain, 250, 0));
		VT_CHECK(W.Annals_().ByType[0] >= Plain.Annals_().ByType[0]);
	}
	// Nothing is named that does not exist.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(W.Ages.Generate(Run::Square(64), 60));
		std::string Name;
		NameWork(W.Instance, W.Ages.Types(), 0, Work::Granary, Name);
		VT_CHECK(!Name.empty());
		Name.clear();
		NameRoad(W.Instance, W.Ages.Types(), W.WorksCtx, 999999, Name);
		VT_CHECK(Name.find("road ") != std::string::npos);
		std::string Why;
		VT_CHECK_EQ(W.Why_(PersistentId{}, Why), 0u);
	}
}

VAELEN_TEST(WorksHistory, TwoWorldsWriteTheSameChronicle)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(Settled(A, 250, 0));
	VT_REQUIRE(Settled(B, 250, 0));
	const WorksChronicleStats SA = A.Annals_();
	VT_CHECK_EQ(SA.Records, B.Annals_().Records);

	std::string TextA;
	std::string TextB;
	const uint32 Lines = A.Chronicle(TextA);
	VT_CHECK_EQ(Lines, B.Chronicle(TextB));
	VT_CHECK_MSG(TextA == TextB, "two worlds of one seed write the same chronicle, word for word");
	const Hash64 Digest = HashString(TextA.c_str());
	VAELEN_LOG_INFO(LogAnnals, "%u record(s), %u line(s), text=%016llx", SA.Records, Lines,
					static_cast<unsigned long long>(Digest));
#if VAELEN_ANNALS_RECORDS_128 != 0u
	VT_CHECK_EQ(SA.Records, VAELEN_ANNALS_RECORDS_128);
	VT_CHECK_EQ(Digest, Hash64{VAELEN_ANNALS_TEXT_128});
#endif
}
