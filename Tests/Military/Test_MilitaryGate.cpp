// VAELEN - Tests/Military
// Phase 08.08: the phase gate - five centuries at 256 with every Phase 04 to 08
// system running, every invariant of every one of them checked each decade, and
// the whole thing frozen.
//
// STATUS: VALIDATED (Phase 08)

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
#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Military;
using namespace Vaelen::Politics;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 (08.08): AELVOR 256 at year 300, the two
// most peopled regions detailed, five centuries with every Phase 04 to 08
// system running.
#define VAELEN_MILGATE_FROZEN_256_250 0x795f25aed60dcb61ull
#define VAELEN_MILGATE_FROZEN_256_500 0x90b8eda971f657f2ull
#define VAELEN_MILGATE_LOG_256_500 0xde646de7ded3e7fdull
#define VAELEN_MILGATE_TEXT_256_500 0xd704a925653f8ecbull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogMilitaryGate);

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
			Ramparts = std::make_unique<SiegeSystem>(Instance, Ages.Types(), Polities, Reaches, Treaties, Hosts, Orders,
													 Walls, InWalls);
			Terms = std::make_unique<WarSystem>(Instance, Ages.Types(), Polities, Treaties, Fields, Hosts, Orders,
												Quarrels, InQuarrels);
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

} // namespace

namespace
{
	double Seconds(const std::chrono::steady_clock::time_point& From)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - From).count();
	}

	/// Everything that must hold in every year of the phase, of every module it
	/// stands on. Returns the failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, const Run& W, uint32 Year)
	{
		uint32 Failures = 0;
		const ArmyStats Ho = W.Hosts_();
		const MarchStats Ma = W.Columns_();
		const BattleStats Ba = W.Fields_();
		const SiegeStats Si = W.Walls_();
		const WarStats Wa = W.Quarrels_();
		const TollStats To = W.Cost_();
		const uint32 Bad = Ho.Bad + Ma.Bad + Ba.Bad + Si.Bad + Wa.Bad + To.Bad;
		if (Bad != 0)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad armies, %u marches, %u battles, %u sieges, %u wars, %u tolls", Year,
						 Ho.Bad, Ma.Bad, Ba.Bad, Si.Bad, Wa.Bad, To.Bad);
		}
		// Nothing is invented and nothing is lost: the men the regions say are
		// away are exactly the men under arms, in every year of five centuries.
		if (Ho.Away != Ho.Men)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u men away against %u under arms", Year, Ho.Away, Ho.Men);
		}
		// Every war that broke out is either still being fought or over.
		if (Wa.Began != Wa.Running + Wa.Over)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u wars begun, %u running and %u over", Year, Wa.Began, Wa.Running, Wa.Over);
		}
		// Every battle ends in a host falling back or a host destroyed.
		if (Ba.Breakings + Ba.Retreats != Ba.Battles_)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u battles, %u breakings and %u retreats", Year, Ba.Battles_, Ba.Breakings,
						 Ba.Retreats);
		}
		// Every record of the military chronicle has a sentence and sits in the
		// era it happened in.
		const MilitaryChronicleStats An = W.Annals_();
		if (An.Described != An.Records || An.EraConsistent != An.Records)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u of %u records described, %u in their own era", Year, An.Described,
						 An.Records, An.EraConsistent);
		}
		// And the ground under it all still adds up: the Phase 07 measures.
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

VAELEN_TEST(MilitaryGate, FiveCenturiesOfWarAt256HoldEveryInvariantAndFreeze)
{
	Run W(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(W.Ages.Generate(Run::Square(256), 300));
	// Two powers, so that the whole phase is exercised: a world with one polity
	// has no war to raise a levy for.
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
		Failures += CheckInvariants(Ctx, W, Decade * 10);
		if (Failures > 20)
		{
			break;
		}
		if (Decade % 10 == 0)
		{
			const ArmyStats Ho = W.Hosts_();
			const BattleStats Ba = W.Fields_();
			const SiegeStats Si = W.Walls_();
			const WarStats Wa = W.Quarrels_();
			const TollStats To = W.Cost_();
			const MilitaryChronicleStats An = W.Annals_();
			VAELEN_LOG_INFO(LogMilitaryGate,
							"year %u: %u host(s) of %u men, %u war(s) begun (%u over), %u battle(s), %u seat(s) "
							"stormed, %llu fallen, %llu fled, %u record(s)",
							Decade * 10, Ho.Standing, Ho.Men, Wa.Began, Wa.Over, Ba.Fought, Si.Stormed,
							static_cast<unsigned long long>(To.Fallen), static_cast<unsigned long long>(To.Fled),
							An.Records);
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
	W.Chronicle(Text);
	const Hash64 TextDigest = HashString(Text.c_str());
	VAELEN_LOG_INFO(LogMilitaryGate,
					"gate: 500 years at 256 with regions %u and %u detailed in %.1f s [asserts %s]; frozen: "
					"250=%016llx 500=%016llx log=%016llx text=%016llx",
					First, Second, Elapsed, VAELEN_ASSERTS_ENABLED ? "on" : "off",
					static_cast<unsigned long long>(At250), static_cast<unsigned long long>(At500),
					static_cast<unsigned long long>(Log), static_cast<unsigned long long>(TextDigest));

	// Five centuries of war really happened: levies called, hosts marched and
	// broken, capitals stormed, wars begun and ended, and people who paid for it.
	const ArmyStats Ho = W.Hosts_();
	const BattleStats Ba = W.Fields_();
	const SiegeStats Si = W.Walls_();
	const WarStats Wa = W.Quarrels_();
	const TollStats To = W.Cost_();
	const MilitaryChronicleStats An = W.Annals_();
	VT_CHECK(Ho.Raisings > 0);
	VT_CHECK(Ba.Fought > 0);
	VT_CHECK(Wa.Began > 0 && Wa.Over > 0);
	VT_CHECK(To.Fallen > 0);
	VT_CHECK(An.Records > 0 && An.Described == An.Records);
	VT_CHECK(Text.find(" went to war.") != std::string::npos);
	VT_CHECK(Text.find(" won a battle in ") != std::string::npos);
	VAELEN_LOG_INFO(
		LogMilitaryGate, "five centuries: %u levies, %u battles, %u seats stormed, %u wars (%u decided), %llu men dead",
		Ho.Raisings, Ba.Fought, Si.Stormed, Wa.Began, Wa.Decided, static_cast<unsigned long long>(To.Fallen));

	// The world of year 250 is still there, and it runs on into the same year
	// 500 the first one reached.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed, ArmyRules{}, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), At250);
	R.Ages.Run(250);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), At500);

	VT_CHECK_EQ(At250, Hash64{VAELEN_MILGATE_FROZEN_256_250});
	VT_CHECK_EQ(At500, Hash64{VAELEN_MILGATE_FROZEN_256_500});
	VT_CHECK_EQ(Log, Hash64{VAELEN_MILGATE_LOG_256_500});
	VT_CHECK_EQ(TextDigest, Hash64{VAELEN_MILGATE_TEXT_256_500});
}
