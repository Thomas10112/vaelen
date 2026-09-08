// VAELEN - Tests/Military
// Phase 08.05: war as a thing with a beginning and an end - opened by a
// relation turning, held open while it runs, and closed by exhaustion.
//
// STATUS: PROTOTYPE (Phase 08)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Politics/Law.h"
#include "Vaelen/Politics/Reach.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/Battle.h"
#include "Vaelen/Military/Siege.h"
#include "Vaelen/Military/War.h"
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
using namespace Vaelen::Military;
using namespace Vaelen::Politics;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 (08.05): AELVOR 128 at year 300, the two
// most peopled regions detailed, 100 years with every Phase 04 to 08 system so
// far, wars included.
#define VAELEN_WAR_FROZEN_128 0x7f5a5a47a3be40dfull
#define VAELEN_WAR_OVER_128 5u
#define VAELEN_WAR_FALLEN_128 1968u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogWar);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, ArmyRules InHosts = ArmyRules{}, DiplomacyRules InTreaties = DiplomacyRules{},
					 FactionRules InFactions = FactionRules{}, SuccessionRules InLine = SuccessionRules{},
					 ReachRules InReach = ReachRules{}, LawRules InLaws = LawRules{},
					 PolityRules InRules = PolityRules{}, MarchRules InColumns = MarchRules{},
					 BattleRules InFields = BattleRules{}, SiegeRules InWalls = SiegeRules{},
					 WarRules InQuarrels = WarRules{})
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
			Quarrels = WarTypes::Declare(Instance);
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
			Envoys = std::make_unique<DiplomacySystem>(Instance, Ages.Types(), Trade, Polities, Treaties, InTreaties);
			Marshals = std::make_unique<ArmySystem>(Instance, Ages.Types(), Economy, Polities, Laws, Reaches, Treaties,
													Hosts, InHosts);
			Columns = std::make_unique<MarchSystem>(Instance, Ages.Types(), Economy, Polities, Reaches, Treaties, Hosts,
													Orders, InColumns);
			Swords = std::make_unique<BattleSystem>(Instance, Ages.Types(), Polities, Reaches, Treaties, Hosts, Orders,
													Fields, InFields);
			Ramparts = std::make_unique<SiegeSystem>(Instance, Ages.Types(), Polities, Reaches, Treaties, Hosts, Walls,
													 InWalls);
			Terms =
				std::make_unique<WarSystem>(Instance, Ages.Types(), Polities, Treaties, Fields, Quarrels, InQuarrels);
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
		const SiegeInfo* Rampart(uint32 Region) const { return SiegeOf(Instance, Ages.Types(), Walls, Region); }
		WarStats Quarrels_(WarRules R = WarRules{}) const { return MeasureWars(Instance, Quarrels, R); }
		const WarInfo* Quarrel(uint32 Index) const { return WarOf(Instance, Quarrels, Index); }
		const WarInfo* QuarrelBetween(uint32 A, uint32 B) const { return WarBetween(Instance, Quarrels, A, B); }
		/// Every war on record, in index order.
		std::vector<WarInfo> AllWars() const
		{
			std::vector<WarInfo> Out;
			Instance.Components()
				.GetPool(Quarrels.War)
				.ForEach([&](EntityHandle, const WarInfo& F) { Out.push_back(F); });
			std::sort(Out.begin(), Out.end(), [](const WarInfo& X, const WarInfo& Y) { return X.Index < Y.Index; });
			return Out;
		}
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
		WarTypes Quarrels;
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
		std::unique_ptr<ArmySystem> Marshals;
		std::unique_ptr<MarchSystem> Columns;
		std::unique_ptr<BattleSystem> Swords;
		std::unique_ptr<SiegeSystem> Ramparts;
		std::unique_ptr<WarSystem> Terms;
	};
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

	/// Two powers grown into each other until they are at war, or the years run
	/// out. Returns the year war broke out, or 0.
	uint32 UntilWar(Run& W, uint32 Years, uint32 Grain = 60000)
	{
		if (!W.Ages.Generate(Run::Square(128), 300))
		{
			return 0;
		}
		const std::vector<uint32> Ranked = W.Ranked();
		if (Ranked.size() < 2 || !RequestDetail(W.Instance, W.Lod, Ranked[0]) ||
			!RequestDetail(W.Instance, W.Lod, Ranked[1]))
		{
			return 0;
		}
		for (uint32 Year = 1; Year <= Years; ++Year)
		{
			for (const uint32 P : W.Powers())
			{
				W.Endow(P, Grain);
			}
			W.Ages.Run(1);
			if (W.Treaties_().Wars > 0)
			{
				return Year;
			}
		}
		return 0;
	}
} // namespace

VAELEN_TEST(War, AWarBeginsWithARelationAndEndsWithExhaustion)
{
	Run W(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(UntilWar(W, 40) != 0);
	for (uint32 Year = 0; Year < 120 && W.Quarrels_().Over < 2; ++Year)
	{
		for (const uint32 P : W.Powers())
		{
			W.Endow(P, 60000);
		}
		W.Ages.Run(1);
		const WarStats S = W.Quarrels_();
		VT_CHECK_EQ(S.Bad, 0u);
		VT_CHECK_EQ(W.Hosts_().Bad, 0u);
		VT_CHECK_EQ(W.Walls_().Bad, 0u);
		VT_CHECK_EQ(S.Began, S.Running + S.Over);
		VT_CHECK_EQ(S.Ended, S.Over);

		// While a war runs its relation is at war, whatever the warmth has
		// done; when it is over the relation is at peace, at a warmth that
		// will hold for a while.
		for (const WarInfo& F : W.AllWars())
		{
			const Relation* Bond = W.Bond(F.A, F.B);
			if (Bond == nullptr || W.QuarrelBetween(F.A, F.B) != nullptr)
			{
				continue;
			}
			if (F.Ended == 0)
			{
				VT_CHECK_EQ(Bond->Stance_, static_cast<uint32>(Stance::War));
			}
		}
	}
	const WarStats S = W.Quarrels_();
	VAELEN_LOG_INFO(LogWar, "wars: %u began, %u over (%u decided, %u white), %llu fallen, longest %u years", S.Began,
					S.Over, S.Decided, S.White, static_cast<unsigned long long>(S.Fallen), S.LongestYears);
	VT_REQUIRE(S.Over > 0);
	VT_CHECK(S.Fallen > 0);
	VT_CHECK(S.LongestYears >= WarRules{}.LeastYears);

	// Every war that is over left its relation at peace, warmed enough that
	// 07.06 will not turn it straight back round.
	uint32 Checked = 0;
	for (const WarInfo& F : W.AllWars())
	{
		if (F.Ended == 0)
		{
			continue;
		}
		const Relation* Bond = W.Bond(F.A, F.B);
		if (Bond == nullptr || W.QuarrelBetween(F.A, F.B) != nullptr)
		{
			continue; // they have fallen out again since
		}
		++Checked;
		VT_CHECK_MSG(Bond->Stance_ != static_cast<uint32>(Stance::War), "war %u is over and %u and %u are still at war",
					 F.Index, F.A, F.B);
		VT_CHECK(Bond->Warmth >= WarRules{}.PeaceWarmth);
	}
	VT_CHECK(Checked > 0);
	VAELEN_LOG_INFO(LogWar, "%u peace(s) written and still holding", Checked);
}

VAELEN_TEST(War, WhatItCostsIsWhatEndsIt)
{
	Run W(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(UntilWar(W, 40) != 0);
	for (uint32 Year = 0; Year < 140; ++Year)
	{
		for (const uint32 P : W.Powers())
		{
			W.Endow(P, 60000);
		}
		W.Ages.Run(1);
	}
	const std::vector<WarInfo> All = W.AllWars();
	VT_REQUIRE(!All.empty());
	uint32 Index = 0;
	uint32 ByExhaustion = 0;
	uint32 ByRuin = 0;
	for (const WarInfo& F : All)
	{
		VT_CHECK_EQ(F.Index, ++Index);
		VT_CHECK(F.A != 0 && F.B != 0 && F.A < F.B);
		VT_CHECK(F.WearA <= 1000 && F.WearB <= 1000);
		// Wear is at least what the years alone cost, and no side is worn
		// without the war having run.
		VT_CHECK(F.WearA >= std::min<uint32>(1000u, F.Years * WarRules{}.WearPerYear));
		VT_CHECK(F.WearB >= std::min<uint32>(1000u, F.Years * WarRules{}.WearPerYear));
		if (F.Ended == 0)
		{
			VT_CHECK_EQ(F.Winner, 0u);
			continue;
		}
		VT_CHECK(F.Ended >= F.Began);
		const PolityInfo* SideA = W.Polity(F.A);
		const PolityInfo* SideB = W.Polity(F.B);
		const bool Ruined = (SideA != nullptr && SideA->Dissolved != 0) || (SideB != nullptr && SideB->Dissolved != 0);
		if (Ruined)
		{
			++ByRuin;
			continue; // a war ends when a side does, whatever it had borne
		}
		++ByExhaustion;
		// Nothing ends by exhaustion before it has run its least years, and
		// nothing ends by exhaustion that neither side was willing to stop.
		VT_CHECK_MSG(F.Years >= WarRules{}.LeastYears, "war %u ended after %u year(s)", F.Index, F.Years);
		const bool Forfeited = F.WearA >= WarRules{}.ForfeitAt || F.WearB >= WarRules{}.ForfeitAt;
		const bool Willing = F.WearA >= WarRules{}.WillingAt && F.WearB >= WarRules{}.WillingAt;
		VT_CHECK_MSG(Forfeited || Willing, "war %u ended with wear %u and %u", F.Index, F.WearA, F.WearB);
		if (F.Winner != 0)
		{
			// A war somebody won is one the other side could not go on with.
			const uint32 LoserWear = F.Winner == F.A ? F.WearB : F.WearA;
			VT_CHECK_MSG(LoserWear >= WarRules{}.ForfeitAt, "war %u won by %u against a side worn only %u", F.Index,
						 F.Winner, LoserWear);
		}
	}
	VAELEN_LOG_INFO(LogWar, "%u war(s) ended by exhaustion, %u by a side being ruined", ByExhaustion, ByRuin);
	VT_CHECK(ByExhaustion + ByRuin > 0);
	VT_CHECK_EQ(W.Quarrels_().Bad, 0u);
}

VAELEN_TEST(War, RulesAndEdges)
{
	// A world with nothing in it has fought no wars, and the lookups refuse
	// what does not exist.
	Run Empty(AelvorSeed);
	const WarStats Nothing = Empty.Quarrels_();
	VT_CHECK_EQ(Nothing.Running, 0u);
	VT_CHECK_EQ(Nothing.Over, 0u);
	VT_CHECK_EQ(Nothing.Fallen, 0u);
	VT_CHECK_EQ(Nothing.Bad, 0u);
	VT_CHECK(Empty.Quarrel(0xfffffff0u) == nullptr);
	VT_CHECK(Empty.QuarrelBetween(0, 0) == nullptr);
	VT_CHECK(Empty.QuarrelBetween(7, 7) == nullptr);
	VT_CHECK(Empty.QuarrelBetween(1, 2) == nullptr);

	// A war nobody will ever agree to stop only ends when a side does.
	WarRules Endless;
	Endless.LeastYears = 100000;
	Run E(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach(), LawRules{},
		  PolityRules{}, MarchRules{}, BattleRules{}, SiegeRules{}, Endless);
	VT_REQUIRE(UntilWar(E, 40) != 0);
	for (uint32 Year = 0; Year < 40; ++Year)
	{
		for (const uint32 P : E.Powers())
		{
			E.Endow(P, 60000);
		}
		E.Ages.Run(1);
	}
	const WarStats Long = E.Quarrels_(Endless);
	VT_REQUIRE(Long.Began > 0);
	VT_CHECK_EQ(Long.Bad, 0u);
	for (const WarInfo& F : E.AllWars())
	{
		if (F.Ended == 0)
		{
			continue;
		}
		// The only way out was for a side to stop existing.
		const PolityInfo* SideA = E.Polity(F.A);
		const PolityInfo* SideB = E.Polity(F.B);
		VT_CHECK_MSG((SideA != nullptr && SideA->Dissolved != 0) || (SideB != nullptr && SideB->Dissolved != 0),
					 "war %u ended after %u year(s) with both sides standing", F.Index, F.Years);
	}

	// A war neither side has the stomach for ends the moment it is allowed to.
	WarRules Brief;
	Brief.WillingAt = 0;
	Brief.LeastYears = 3;
	Run B(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach(), LawRules{},
		  PolityRules{}, MarchRules{}, BattleRules{}, SiegeRules{}, Brief);
	VT_REQUIRE(UntilWar(B, 40) != 0);
	for (uint32 Year = 0; Year < 40; ++Year)
	{
		for (const uint32 P : B.Powers())
		{
			B.Endow(P, 60000);
		}
		B.Ages.Run(1);
	}
	const WarStats Short = B.Quarrels_(Brief);
	VT_REQUIRE(Short.Over > 0);
	VT_CHECK_EQ(Short.Bad, 0u);
	for (const WarInfo& F : B.AllWars())
	{
		if (F.Ended == 0)
		{
			continue;
		}
		VT_CHECK_MSG(F.Years <= Brief.LeastYears + 1u, "war %u ran %u years with nobody willing to fight it", F.Index,
					 F.Years);
	}
	VAELEN_LOG_INFO(LogWar, "edges: %u endless war(s) begun, %u brief war(s) over", Long.Began, Short.Over);
}

VAELEN_TEST(War, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	Run B(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
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
		const WarStats S = A.Quarrels_();
		if (S.Bad != 0 || A.Quarrels_().Digest != B.Quarrels_().Digest)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad, wars %s", Year, S.Bad,
						 A.Quarrels_().Digest == B.Quarrels_().Digest ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const WarStats S = A.Quarrels_();
	VAELEN_LOG_INFO(LogWar,
					"frozen: wars128=%016llx over=%u fallen=%llu (%u running, %u decided, %u white, longest %u)",
					static_cast<unsigned long long>(S.Digest), S.Over, static_cast<unsigned long long>(S.Fallen),
					S.Running, S.Decided, S.White, S.LongestYears);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_WAR_FROZEN_128});
	VT_CHECK_EQ(S.Over, uint32{VAELEN_WAR_OVER_128});
	VT_CHECK_EQ(S.Fallen, uint64{VAELEN_WAR_FALLEN_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Quarrels_().Digest, S.Digest);
}
