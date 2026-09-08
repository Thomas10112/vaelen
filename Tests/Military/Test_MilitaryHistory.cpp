// VAELEN - Tests/Military
// Phase 08.07: war in the chronicle - the few military events a century keeps,
// a sentence for every one of them, and the why of a lost province.
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
#include "Vaelen/Military/MilitaryHistory.h"
#include "Vaelen/Military/Toll.h"
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

// Recorded on clang 18 / Linux x86_64 (08.07): AELVOR 128 at year 300, the two
// most peopled regions detailed, 100 years with every Phase 04 to 08 system so
// far, the chronicle included.
#define VAELEN_ANNALS_RECORDS_128 241u
#define VAELEN_ANNALS_TEXT_128 0x88eee3886fdd6d57ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogChronicle);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, ArmyRules InHosts = ArmyRules{}, DiplomacyRules InTreaties = DiplomacyRules{},
					 FactionRules InFactions = FactionRules{}, SuccessionRules InLine = SuccessionRules{},
					 ReachRules InReach = ReachRules{}, LawRules InLaws = LawRules{},
					 PolityRules InRules = PolityRules{}, MarchRules InColumns = MarchRules{},
					 BattleRules InFields = BattleRules{}, SiegeRules InWalls = SiegeRules{},
					 WarRules InQuarrels = WarRules{}, TollRules InCost = TollRules{},
					 MilitaryChronicleRules InAnnals = MilitaryChronicleRules{})
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
			Cost = TollTypes::Declare(Instance);
			Records = MilitaryChronicleTypes::Declare(Instance);
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
			Reckoning =
				std::make_unique<TollSystem>(Instance, Ages.Types(), Persons, Standing, Polities, Orders, Cost, InCost);
			Ranks->ObserveService(Standing.Service);
			SocietyCtx = SocietyContext{Persons, Families, Organizations};
			EconomyCtx = EconomyContext{Persons, Families, Trade, Markets, MarketRules{}, &SocietyCtx};
			PoliticsCtx =
				PoliticsContext{Persons, Polities, Laws, LawRules{}, Reaches, Heirs, Parties, Treaties, &EconomyCtx};
			MilitaryCtx = MilitaryContext{Hosts, Orders, Fields, Walls, Quarrels, Cost, &PoliticsCtx};
			Annalist = std::make_unique<MilitaryChronicle>(Instance, Ages.Types(), MilitaryCtx, Records, InAnnals);
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
		const SiegeInfo* Rampart(uint32 Region) const { return SiegeOf(Instance, Ages.Types(), Walls, Region); }
		WarStats Quarrels_(WarRules R = WarRules{}) const { return MeasureWars(Instance, Quarrels, R); }
		const WarInfo* Quarrel(uint32 Index) const { return WarOf(Instance, Quarrels, Index); }
		TollStats Cost_(TollRules R = TollRules{}) const
		{
			return MeasureToll(Instance, Ages.Types(), Standing, Cost, R);
		}
		const RegionToll* Reckoned(uint32 Region) const { return TollOf(Instance, Ages.Types(), Cost, Region); }
		MilitaryChronicleStats Annals_() const
		{
			return CheckMilitaryChronicle(Instance, Ages.Types(), MilitaryCtx, Records);
		}
		uint32 Chronicle(std::string& Out, uint32 MaxLines = 0) const
		{
			return ExportChronicleWithMilitary(Instance, Ages.Types(), MilitaryCtx, Out, MaxLines);
		}
		uint32 Why(PersistentId Id, std::string& Out) const
		{
			return ExportWhyWithMilitary(Instance, Ages.Types(), MilitaryCtx, Id, Out);
		}
		std::string Describe(const Event& E) const
		{
			std::string Line;
			DescribeMilitaryEvent(Instance, Ages.Types(), MilitaryCtx, E, Line);
			return Line;
		}
		/// Living people of a region, from the coarse count.
		uint32 Living(uint32 Region) const
		{
			const RegionPopulation* P = Counts(Region);
			return P != nullptr ? P->Total : 0u;
		}
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
		TollTypes Cost;
		MilitaryChronicleTypes Records;
		SocietyContext SocietyCtx;
		EconomyContext EconomyCtx;
		PoliticsContext PoliticsCtx;
		MilitaryContext MilitaryCtx;
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
		std::unique_ptr<TollSystem> Reckoning;
		std::unique_ptr<MilitaryChronicle> Annalist;
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

namespace
{
	/// The chronicle split into lines.
	std::vector<std::string> Lines(const Run& W, uint32 Max = 0)
	{
		std::string Text;
		W.Chronicle(Text, Max);
		std::vector<std::string> Out;
		usize At = 0;
		while (At < Text.size())
		{
			const usize End = Text.find('\n', At);
			Out.push_back(Text.substr(At, End - At));
			At = End == std::string::npos ? Text.size() : End + 1;
		}
		return Out;
	}

	/// A world run to war and then some, for the tests that need one.
	uint32 UntilWarAndOn(Run& W, uint32 Years)
	{
		if (UntilWar(W, 40) == 0)
		{
			return 0;
		}
		for (uint32 Year = 1; Year <= Years; ++Year)
		{
			for (const uint32 P : W.Powers())
			{
				W.Endow(P, 60000);
			}
			W.Ages.Run(1);
		}
		return Years;
	}
} // namespace

VAELEN_TEST(MilitaryHistory, TheChronicleKeepsWhatACenturyWouldRemember)
{
	Run W(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(UntilWarAndOn(W, 60) != 0);
	const MilitaryChronicleStats S = W.Annals_();
	VAELEN_LOG_INFO(LogChronicle,
					"annals: %u record(s), %u described, %u with a region, %u dropped; levy %u battle %u siege %u "
					"war %u toll %u",
					S.Records, S.Described, S.WithRegion, S.Dropped, S.ByType[0], S.ByType[1], S.ByType[2], S.ByType[3],
					S.ByType[4]);
	VT_REQUIRE(S.Records > 0);
	// Every record has a sentence, sits in the era it happened in, and all but a
	// war - which belongs to no one region - names one.
	VT_CHECK_EQ(S.Described, S.Records);
	VT_CHECK_EQ(S.EraConsistent, S.Records);
	VT_CHECK(S.WithRegion + S.ByType[3] >= S.Records);
	VT_CHECK_EQ(S.Dropped, 0u);
	// The kinds a war produces are all of them there.
	VT_CHECK(S.ByType[0] > 0); // a levy called
	VT_CHECK(S.ByType[1] > 0); // a battle
	VT_CHECK(S.ByType[3] > 0); // a war begun
	VT_CHECK_EQ(S.ByType[0] + S.ByType[1] + S.ByType[2] + S.ByType[3] + S.ByType[4], S.Records);

	// The chronicle is in order, and every line of it is a sentence.
	const std::vector<std::string> All = Lines(W);
	VT_REQUIRE(All.size() >= S.Records);
	uint32 Stamped = 0;
	for (const std::string& Line : All)
	{
		VT_CHECK(!Line.empty());
		Stamped += Line.rfind("Year ", 0) == 0 ? 1u : 0u;
		VT_CHECK(Line.back() == '.' || Line.back() == '!');
	}
	VT_CHECK_EQ(Stamped, static_cast<uint32>(All.size()));
	VAELEN_LOG_INFO(LogChronicle, "%u line(s), every one stamped with its year and its age",
					static_cast<uint32>(All.size()));
}

VAELEN_TEST(MilitaryHistory, EveryMilitaryEventHasASentence)
{
	Run W(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(UntilWarAndOn(W, 60) != 0);

	// Not only the recorded ones: every military event the world has published
	// can be read out, whether a chronicle kept it or not.
	uint32 Seen = 0;
	uint32 Named = 0;
	std::string Storm;
	std::string War;
	for (const Event& E : W.Instance.Log().All())
	{
		const std::string Line = W.Describe(E);
		if (E.Is(ArmyRaisedEvent) || E.Is(ArmyDisbandedEvent) || E.Is(ArmyStarvedEvent) || E.Is(ArmyMarchedEvent) ||
			E.Is(ArmyForagedEvent) || E.Is(ArmyArrivedEvent) || E.Is(BattleFoughtEvent) || E.Is(ArmyBrokenEvent) ||
			E.Is(ArmyRetreatedEvent) || E.Is(SiegeLaidEvent) || E.Is(SiegeLiftedEvent) || E.Is(SeatTakenEvent) ||
			E.Is(WarBeganEvent) || E.Is(WarEndedEvent) || E.Is(LevyReleasedEvent) || E.Is(WarDeadEvent) ||
			E.Is(PeopleFledEvent))
		{
			++Seen;
			VT_CHECK(!Line.empty());
			VT_CHECK(Line.rfind("Year ", 0) == 0);
			VT_CHECK(Line.back() == '.');
			// With the politics context at hand a polity that still stands is
			// named by its seat. One that has been dissolved has no seat left to
			// be named by and falls back on its number, which is the honest thing.
			Named += Line.find(" the polity of ") != std::string::npos || Line.rfind("polity ", 0) != std::string::npos
						 ? 1u
						 : 0u;
			if (E.Is(SeatTakenEvent) && Storm.empty())
			{
				Storm = Line;
			}
			if (E.Is(WarBeganEvent) && War.empty())
			{
				War = Line;
			}
		}
	}
	VT_REQUIRE(Seen > 0);
	VT_CHECK(Named > 0);
	VT_CHECK(!War.empty());
	VT_CHECK(War.find(" went to war.") != std::string::npos);
	VAELEN_LOG_INFO(LogChronicle, "%u military event(s), every one a sentence", Seen);
	VAELEN_LOG_INFO(LogChronicle, "war:   %s", War.c_str());
	if (!Storm.empty())
	{
		VT_CHECK(Storm.find(" stormed ") != std::string::npos);
		VT_CHECK(Storm.find(" and took it from ") != std::string::npos);
		VAELEN_LOG_INFO(LogChronicle, "storm: %s", Storm.c_str());
	}

	// And the why of one of them walks back to its root: a region buries
	// its men because they did not come back, because a battle was lost.
	PersistentId Subject{};
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(WarDeadEvent))
		{
			Subject = E.Id;
		}
	}
	VT_REQUIRE(Subject.Value != 0);
	std::string Why;
	const uint32 Steps = W.Why(Subject, Why);
	VT_CHECK_MSG(Steps >= 3, "a burial walked back only %u step(s)", Steps);
	VT_CHECK(!Why.empty());
	VT_CHECK(Why.find("because") != std::string::npos);
	VAELEN_LOG_INFO(LogChronicle, "why, %u step(s):\n%s", Steps, Why.c_str());
}

VAELEN_TEST(MilitaryHistory, RulesAndEdges)
{
	// A chronicle that keeps nothing keeps nothing, and the world runs all the
	// same.
	MilitaryChronicleRules Silent;
	Silent.RecordLevies = 0;
	Silent.RecordBattles = 0;
	Silent.RecordSieges = 0;
	Silent.RecordWars = 0;
	Silent.RecordTolls = 0;
	Run Quiet(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach(), LawRules{},
			  PolityRules{}, MarchRules{}, BattleRules{}, SiegeRules{}, WarRules{}, TollRules{}, Silent);
	VT_REQUIRE(UntilWarAndOn(Quiet, 40) != 0);
	const MilitaryChronicleStats None = Quiet.Annals_();
	VT_CHECK_EQ(None.Records, 0u);
	VT_CHECK_EQ(None.Dropped, 0u);
	VT_CHECK(Quiet.Hosts_().Raisings > 0); // the war happened; it simply was not written down

	// A chronicle allowed one record a region a year drops the rest, and says so.
	MilitaryChronicleRules Terse;
	Terse.MaxRecordsPerYear = 1;
	Run Short(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach(), LawRules{},
			  PolityRules{}, MarchRules{}, BattleRules{}, SiegeRules{}, WarRules{}, TollRules{}, Terse);
	VT_REQUIRE(UntilWarAndOn(Short, 60) != 0);
	const MilitaryChronicleStats Few = Short.Annals_();
	VT_CHECK(Few.Records > 0);
	VT_CHECK_MSG(Few.Dropped > 0, "a chronicle of one line a year dropped nothing over sixty years of war");
	VAELEN_LOG_INFO(LogChronicle, "terse: %u kept, %u dropped", Few.Records, Few.Dropped);

	// A toll too small to be remembered is not remembered.
	MilitaryChronicleRules Hard;
	Hard.DeadWorthRecording = 1000000;
	Hard.FledWorthRecording = 1000000;
	Run Stoic(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach(), LawRules{},
			  PolityRules{}, MarchRules{}, BattleRules{}, SiegeRules{}, WarRules{}, TollRules{}, Hard);
	VT_REQUIRE(UntilWarAndOn(Stoic, 40) != 0);
	const MilitaryChronicleStats Borne = Stoic.Annals_();
	VT_CHECK_EQ(Borne.ByType[4], 0u);
	VT_CHECK(Borne.Records > 0);
	VT_CHECK(Stoic.Cost_().Fallen > 0); // people still died; the chronicle just did not think it news
}

VAELEN_TEST(MilitaryHistory, DeterministicAndFrozen)
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
	A.Ages.Run(100);
	B.Ages.Run(100);
	std::string TextA;
	std::string TextB;
	A.Chronicle(TextA);
	B.Chronicle(TextB);
	VT_CHECK_EQ(TextA.size(), TextB.size());
	VT_CHECK(TextA == TextB);
	const MilitaryChronicleStats S = A.Annals_();
	VT_CHECK_EQ(S.Records, B.Annals_().Records);
	VT_CHECK_EQ(S.Described, S.Records);
	const Hash64 Text = HashString(TextA.c_str());
	VAELEN_LOG_INFO(LogChronicle, "frozen: %u military record(s), chronicle text %016llx (%u lines)", S.Records,
					static_cast<unsigned long long>(Text), static_cast<uint32>(Lines(A).size()));
	VT_CHECK_EQ(S.Records, uint32{VAELEN_ANNALS_RECORDS_128});
	VT_CHECK_EQ(Text, Hash64{VAELEN_ANNALS_TEXT_128});
}
