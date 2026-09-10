// VAELEN - Tests/Politics
// Phase 07.07: politics in the chronicle - a line for every political event,
// and a record only for what a century would remember.
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
#include "Vaelen/Politics/PoliticsHistory.h"
#include "Vaelen/Politics/Factions.h"
#include "Vaelen/Politics/Succession.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Society/SocietyHistory.h"
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
#define VAELEN_ANNALS_FROZEN_128 0xb79d0190886f022dull
#define VAELEN_ANNALS_RECORDS_128 100u
#define VAELEN_ANNALS_LINES_128 3708u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogAnnals);

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
			PersonRecords = PersonChronicleTypes::Declare(Instance);
			SocietyRecords = SocietyChronicleTypes::Declare(Instance);
			Books = EconomyChronicleTypes::Declare(Instance);
			Annals = PoliticsChronicleTypes::Declare(Instance);
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
			Words->ObserveContest(Treaties.Contested);
			SocietyCtx = SocietyContext{Persons, Families, Organizations};
			EconomyCtx = EconomyContext{Persons, Families, Trade, Markets, MarketRules{}, &SocietyCtx};
			PoliticsCtx =
				PoliticsContext{Persons, Polities, Laws, LawRules{}, Reaches, Heirs, Parties, Treaties, &EconomyCtx};
			Persons_ = std::make_unique<PersonChronicle>(Instance, Ages.Types(), Persons, Families, PersonRecords,
														 PersonChronicleRules{});
			Society_ = std::make_unique<SocietyChronicle>(Instance, Ages.Types(), SocietyCtx, SocietyRecords,
														  SocietyChronicleRules{});
			Economy_ =
				std::make_unique<EconomyChronicle>(Instance, Ages.Types(), EconomyCtx, Books, EconomyChronicleRules{});
			Annalist = std::make_unique<PoliticsChronicle>(Instance, Ages.Types(), PoliticsCtx, Annals,
														   PoliticsChronicleRules{});
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
			Persons_->Attach();
			Society_->Attach();
			Economy_->Attach();
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
		PoliticsChronicleStats Annals_() const
		{
			return CheckPoliticsChronicle(Instance, Ages.Types(), PoliticsCtx, Annals);
		}
		std::string Describe(const Event& E) const
		{
			std::string Line;
			DescribePoliticsEvent(Instance, Ages.Types(), PoliticsCtx, E, Line);
			return Line;
		}
		uint32 Chronicle(std::string& Out, uint32 MaxLines = 0) const
		{
			return ExportChronicleWithPolitics(Instance, Ages.Types(), PoliticsCtx, Out, MaxLines);
		}
		uint32 Why(PersistentId Id, std::string& Out) const
		{
			return ExportWhyWithPolitics(Instance, Ages.Types(), PoliticsCtx, Id, Out);
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
		PersonChronicleTypes PersonRecords;
		SocietyChronicleTypes SocietyRecords;
		EconomyChronicleTypes Books;
		PoliticsChronicleTypes Annals;
		SocietyContext SocietyCtx;
		EconomyContext EconomyCtx;
		PoliticsContext PoliticsCtx;
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
		std::unique_ptr<PersonChronicle> Persons_;
		std::unique_ptr<SocietyChronicle> Society_;
		std::unique_ptr<EconomyChronicle> Economy_;
		std::unique_ptr<PoliticsChronicle> Annalist;
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

	/// A world with two powers, grown into each other, over the given years.
	/// Returns false when the world could not be raised, so the caller checks it.
	bool TwoPowers(Run& W, uint32 Years)
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
			for (const uint32 P : W.Powers())
			{
				W.Endow(P, 20000);
			}
			W.Ages.Run(1);
		}
		return true;
	}

	bool IsPolitical(const Event& E)
	{
		return E.Is(PolityFoundedEvent) || E.Is(PolityDissolvedEvent) || E.Is(RulerSeatedEvent) ||
			   E.Is(RegionClaimedEvent) || E.Is(RegionLostEvent) || E.Is(LawChangedEvent) || E.Is(DuesPaidEvent) ||
			   E.Is(DuesUnpaidEvent) || E.Is(RegionTakenEvent) || E.Is(RegionAnnexedEvent) ||
			   E.Is(RegionSlippedEvent) || E.Is(UpkeepUnpaidEvent) || E.Is(SeatFellVacantEvent) ||
			   E.Is(SuccessionSettledEvent) || E.Is(SuccessionDisputedEvent) || E.Is(FactionFormedEvent) ||
			   E.Is(FactionRevoltedEvent) || E.Is(FactionFadedEvent) || E.Is(ContactMadeEvent) ||
			   E.Is(StanceChangedEvent) || E.Is(RegionContestedEvent);
	}
} // namespace

VAELEN_TEST(PoliticsHistory, EveryPoliticalEventHasALineOfItsOwn)
{
	Run W(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(TwoPowers(W, 40));

	uint32 Political = 0;
	uint32 Generic = 0;
	uint32 Unprefixed = 0;
	uint32 Bare = 0;
	std::vector<std::string> Samples;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!IsPolitical(E))
		{
			continue;
		}
		++Political;
		const std::string Line = W.Describe(E);
		VT_REQUIRE(!Line.empty());
		// Every line opens with the year and the age, as every other layer's does.
		if (Line.rfind("Year ", 0) != 0 || Line.find(", age of ") == std::string::npos)
		{
			++Unprefixed;
		}
		// None of them falls back to the plain "event 12 of type ..." text.
		if (Line.find("event ") != std::string::npos || Line.find(" of type ") != std::string::npos)
		{
			++Generic;
		}
		// None of them names a bare index where a name was meant.
		if (Line.find("polity 0") != std::string::npos || Line.find("region 0") != std::string::npos ||
			Line.find("person 0") != std::string::npos)
		{
			++Bare;
		}
		if (Samples.size() < 12)
		{
			Samples.push_back(Line);
		}
	}
	VT_CHECK(Political > 0);
	VT_CHECK_EQ(Unprefixed, 0u);
	VT_CHECK_EQ(Generic, 0u);
	VT_CHECK_EQ(Bare, 0u);
	VAELEN_LOG_INFO(LogAnnals, "%u political events, all with a line of their own", Political);
	for (const std::string& S : Samples)
	{
		VAELEN_LOG_INFO(LogAnnals, "  %s", S.c_str());
	}

	// The layers under it keep their own words: an economy event still gets its
	// economy line through this describer, not the plainer person one.
	uint32 Economic = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (!E.Is(Economy::RouteOpenedEvent) && !E.Is(Economy::SettlementFoundedEvent))
		{
			continue;
		}
		const std::string Line = W.Describe(E);
		VT_REQUIRE(!Line.empty());
		VT_CHECK(Line.find(" was opened.") != std::string::npos || Line.find("town") != std::string::npos ||
				 Line.find(" rose") != std::string::npos);
		++Economic;
	}
	VT_CHECK(Economic > 0);
	VAELEN_LOG_INFO(LogAnnals, "%u economy events still speak their own layer's words", Economic);
}

VAELEN_TEST(PoliticsHistory, OnlyWhatACenturyWouldRememberIsRecorded)
{
	Run W(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(TwoPowers(W, 40));

	uint32 Political = 0;
	uint32 Settled = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Political += IsPolitical(E) ? 1u : 0u;
		Settled += E.Is(SuccessionSettledEvent) ? 1u : 0u;
	}
	const PoliticsChronicleStats S = W.Annals_();
	VT_CHECK(S.Records > 0);
	VT_CHECK(S.Records < Political / 2u); // history is narrower than the log, by a lot
	VT_CHECK_EQ(S.Described, S.Records);
	VT_CHECK_EQ(S.EraConsistent, S.Records);
	VAELEN_LOG_INFO(LogAnnals,
					"%u political events, %u records (%u foundings, %u successions, %u laws, %u revolts, %u wars, %u "
					"annexations), %u dropped",
					Political, S.Records, S.ByType[0], S.ByType[1], S.ByType[2], S.ByType[3], S.ByType[4], S.ByType[5],
					S.Dropped);

	// A settled succession is never history; a disputed one is.
	VT_CHECK(Settled > 0);
	uint32 RecordedSettled = 0;
	uint32 RecordedLaws = 0;
	uint32 LawsAtABound = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().History.Record)
		.ForEach(
			[&](EntityHandle, const RecordInfo& R)
			{
				const Event* E = FindEvent(W.Instance.Log(), PersistentId{R.Event});
				if (E == nullptr)
				{
					return;
				}
				RecordedSettled += E->Is(SuccessionSettledEvent) ? 1u : 0u;
				if (E->Is(LawChangedEvent))
				{
					++RecordedLaws;
					const uint32 Share = E->Get<PolityPayload>().Value;
					LawsAtABound += Share == LawRules{}.TaxFloor || Share == LawRules{}.TaxCeiling ? 1u : 0u;
				}
			});
	VT_CHECK_EQ(RecordedSettled, 0u);
	// Every law that is history stood at one of its own bounds.
	VT_CHECK_EQ(RecordedLaws, LawsAtABound);

	// The chronicle reads as text, every line ending in a full stop.
	std::string Text;
	const uint32 Lines = W.Chronicle(Text, 0);
	VT_CHECK(Lines > 0);
	VT_CHECK(!Text.empty());
	uint32 Unfinished = 0;
	usize At = 0;
	while (At < Text.size())
	{
		const usize End = Text.find('\n', At);
		const std::string Line = Text.substr(At, (End == std::string::npos ? Text.size() : End) - At);
		if (!Line.empty() && Line.back() != '.')
		{
			++Unfinished;
		}
		if (End == std::string::npos)
		{
			break;
		}
		At = End + 1;
	}
	VT_CHECK_EQ(Unfinished, 0u);

	// The why of a political event runs back through the causes each system wrote.
	PersistentId Annexation{0};
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(RegionAnnexedEvent) || E.Is(FactionRevoltedEvent))
		{
			Annexation = E.Id;
		}
	}
	if (Annexation.Value != 0)
	{
		std::string Chain;
		const uint32 Deep = W.Why(Annexation, Chain);
		VT_CHECK(Deep >= 1);
		VAELEN_LOG_INFO(LogAnnals, "why, %u deep:\n%s", Deep, Chain.c_str());
	}
}

VAELEN_TEST(PoliticsHistory, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	Run B(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
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
		const PoliticsChronicleStats S = A.Annals_();
		if (S.Described != S.Records || S.EraConsistent != S.Records)
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u records, %u described, %u era-consistent", Year, S.Records, S.Described,
						 S.EraConsistent);
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	std::string TextA;
	std::string TextB;
	const uint32 LinesA = A.Chronicle(TextA, 0);
	const uint32 LinesB = B.Chronicle(TextB, 0);
	VT_CHECK_EQ(LinesA, LinesB);
	VT_CHECK(TextA == TextB);
	const PoliticsChronicleStats S = A.Annals_();
	const Hash64 Digest = HashBytes(TextA.data(), TextA.size());
	VAELEN_LOG_INFO(LogAnnals, "frozen: annals128=%016llx records=%u lines=%u (%u dropped)",
					static_cast<unsigned long long>(Digest), S.Records, LinesA, S.Dropped);
	VT_CHECK_EQ(Digest, Hash64{VAELEN_ANNALS_FROZEN_128});
	VT_CHECK_EQ(S.Records, uint32{VAELEN_ANNALS_RECORDS_128});
	VT_CHECK_EQ(LinesA, uint32{VAELEN_ANNALS_LINES_128});
	// A world restored from a snapshot and run on tells the same story.
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed, DiplomacyRules{}, FactionRules{}, SuccessionRules{}, WideReach());
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	std::string TextR;
	R.Chronicle(TextR, 0);
	VT_CHECK(TextR == TextA);
}
