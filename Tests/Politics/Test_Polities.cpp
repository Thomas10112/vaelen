// VAELEN - Tests/Politics
// Phase 07.01: polities - founded from a council, ruled by its head, holding
// the region that remembers whose it is, dissolved when it rules nothing.
//
// STATUS: VALIDATED (Phase 07)

#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
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

// Recorded on clang 18 / Linux x86_64 on 2026-09-07 (07.01): AELVOR 128 at
// year 300, the busiest region detailed, 100 years with every Phase 04, 05,
// 06 and 07 system so far.
#define VAELEN_POLITIES_FROZEN_128 0xab11336dba47da54ull
#define VAELEN_POLITIES_STANDING_128 1u
#define VAELEN_POLITIES_SEATINGS_128 9u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPolities);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, PolityRules InRules = PolityRules{})
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
	};
} // namespace

VAELEN_TEST(Polities, APolityIsFoundedOnACouncilAndRuledByItsHead)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK_EQ(W.Stats().Standing, 0u); // nothing rules anything before a region is detailed
	VT_CHECK(W.Rule(Region) == nullptr);
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(2);
	const OrganizationInfo* Council = W.Council(Region);
	VT_REQUIRE(Council != nullptr && Council->Members >= PolityRules{}.FoundFromSeats);
	const PolityStats S = W.Stats();
	VT_CHECK_EQ(S.Standing, 1u);
	VT_CHECK_EQ(S.Founded, 1u);
	VT_CHECK_EQ(S.Bad, 0u);
	const PolityInfo* P = W.Polity(1);
	VT_REQUIRE(P != nullptr);
	VAELEN_LOG_INFO(LogPolities, "polity %u seated in region %u: council %u, ruler %u, %u region(s), culture %u",
					P->Index, P->Seat, P->Council, P->Ruler, P->Regions, P->Culture);
	VT_CHECK_EQ(P->Seat, Region);
	VT_CHECK_EQ(P->Council, Council->Index);
	VT_CHECK_EQ(P->Ruler, Council->Head); // authority runs through the council, not beside it
	VT_CHECK_EQ(P->Regions, 1u);
	VT_CHECK(P->Culture != 0 && P->Identity != 0 && P->Founded != 0 && P->Dissolved == 0);
	// The region remembers whose it is, and says so from its own side.
	const RegionRule* Rule = W.Rule(Region);
	VT_REQUIRE(Rule != nullptr);
	VT_CHECK_EQ(Rule->Polity, P->Index);
	VT_CHECK(Rule->Since != 0);
	std::vector<uint32> Held;
	RegionsOf(W.Instance, W.Ages.Types(), W.Polities, P->Index, Held);
	VT_CHECK(Held.size() == 1 && Held[0] == Region);
	// Only one polity: the seat is taken, and no other region is detailed.
	W.Ages.Run(20);
	const PolityStats After = W.Stats();
	VT_CHECK_EQ(After.Standing, 1u);
	VT_CHECK_EQ(After.Founded, 1u);
	VT_CHECK_EQ(After.Bad, 0u);
	VT_CHECK_EQ(After.Ruled, 1u);
	// The ruler follows the council's head across the deaths of twenty years.
	const OrganizationInfo* Now = W.Council(Region);
	VT_REQUIRE(Now != nullptr);
	VT_CHECK_EQ(W.Polity(1)->Ruler, Now->Head);
	VT_CHECK(After.Seatings >= 1);
	VAELEN_LOG_INFO(LogPolities, "after twenty years: ruler %u, %u seatings, %u headless", W.Polity(1)->Ruler,
					After.Seatings, After.Headless);
	// The lookups refuse what does not exist.
	VT_CHECK(W.Polity(0xfffffff0u) == nullptr);
	VT_CHECK(W.Rule(0xfffffff0u) == nullptr);
	std::vector<uint32> None;
	RegionsOf(W.Instance, W.Ages.Types(), W.Polities, 0, None);
	VT_CHECK(None.empty());
}

VAELEN_TEST(Polities, RuleOutlivesTheGrainAndEndsWhenThereIsNothingToRule)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(2);
	VT_REQUIRE(W.Stats().Standing == 1u);
	const uint64 Since = W.Rule(Region)->Since;
	// Coarse again: the region still knows whose it is, and the polity stands.
	VT_CHECK(ReleaseDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(2);
	VT_CHECK(!IsDetailed(W.Instance, W.Ages.Types(), W.Persons, Region));
	const RegionRule* Rule = W.Rule(Region);
	VT_REQUIRE(Rule != nullptr);
	VT_CHECK_EQ(Rule->Polity, 1u);
	VT_CHECK_EQ(Rule->Since, Since); // belonging did not restart
	const PolityStats Coarse = W.Stats();
	VT_CHECK_EQ(Coarse.Standing, 1u);
	VT_CHECK_EQ(Coarse.Ruled, 1u);
	VT_CHECK_EQ(Coarse.Bad, 0u);
	VAELEN_LOG_INFO(LogPolities, "coarse: %u standing, %u ruled, ruler %u (the council is a memory of counts)",
					Coarse.Standing, Coarse.Ruled, W.Polity(1)->Ruler);
	// Detailed again: the same polity, never founded twice.
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	W.Ages.Run(3);
	VT_CHECK_EQ(W.Stats().Founded, 1u);
	VT_CHECK_EQ(W.Stats().Standing, 1u);
	VT_CHECK_EQ(W.Polity(1)->Seat, Region);
	// A polity that rules nothing ends: its region is taken from it by hand.
	RegionRule* Live = nullptr;
	W.Instance.Components()
		.GetPool(W.Ages.Types().World.RegionTypes_.Region)
		.ForEach(
			[&](EntityHandle H, const RegionInfo& R)
			{
				if (R.Index == Region && Live == nullptr)
				{
					Live = W.Instance.Components().GetPool(W.Polities.Rule).TryGet(H);
				}
			});
	VT_REQUIRE(Live != nullptr);
	Live->Polity = 0;
	W.Ages.Run(1);
	const PolityStats Ended = W.Stats();
	VAELEN_LOG_INFO(LogPolities, "after the last region is taken: %u standing, %u dissolved, %u ended, %u founded",
					Ended.Standing, Ended.Dissolved, Ended.Ended, Ended.Founded);
	VT_CHECK_EQ(Ended.Dissolved, 1u);
	VT_CHECK_EQ(Ended.Ended, 1u);
	VT_CHECK_EQ(Ended.Bad, 0u);
	const PolityInfo* Gone = W.Polity(1);
	VT_REQUIRE(Gone != nullptr);
	VT_CHECK(Gone->Dissolved != 0 && Gone->Ruler == 0 && Gone->Regions == 0);
	VT_CHECK(Gone->Founded != 0); // a dissolved polity stays in the world as history
	// A seat whose council still sits does not stay masterless: the same tick
	// that ends the old polity founds a new one on the same council.
	VT_CHECK_EQ(Ended.Standing, 1u);
	VT_CHECK_EQ(Ended.Founded, 2u);
	const PolityInfo* Heir = W.Polity(2);
	VT_REQUIRE(Heir != nullptr);
	VT_CHECK_EQ(Heir->Seat, Region);
	VT_CHECK(Heir->Founded > Gone->Founded);
	VT_CHECK_EQ(W.Rule(Region)->Polity, 2u);
	W.Ages.Run(3);
	const PolityStats Again = W.Stats();
	VT_CHECK_EQ(Again.Standing, 1u);
	VT_CHECK_EQ(Again.Founded, 2u); // and not again after that
	VT_CHECK_EQ(Again.Bad, 0u);
}

VAELEN_TEST(Polities, RulesAndEdges)
{
	// A rule nobody can meet founds nothing.
	PolityRules Never;
	Never.FoundFromPeople = 100000;
	Run X(AelvorSeed, Never);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = X.Busiest();
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	X.Ages.Run(10);
	VT_CHECK_EQ(X.Stats().Standing, 0u);
	VT_CHECK_EQ(X.Stats().Founded, 0u);
	VT_CHECK(X.Rule(Region) == nullptr);
	// A ruler must be of age: a rule nobody satisfies leaves the seat empty
	// while the polity still stands and still holds its region.
	PolityRules Ancient;
	Ancient.RulerFromAge = 200;
	Run Y(AelvorSeed, Ancient);
	VT_REQUIRE(Y.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(Y.Instance, Y.Lod, Region));
	Y.Ages.Run(4);
	const PolityStats S = Y.Stats();
	VAELEN_LOG_INFO(LogPolities, "with no one old enough to rule: %u standing, %u headless, %u seatings, %u ruled",
					S.Standing, S.Headless, S.Seatings, S.Ruled);
	VT_CHECK_EQ(S.Standing, 1u);
	VT_CHECK_EQ(S.Headless, 1u);
	VT_CHECK_EQ(S.Seatings, 0u);
	VT_CHECK_EQ(S.Ruled, 1u);
	VT_CHECK_EQ(S.Bad, 0u);
	VT_CHECK_EQ(Y.Polity(1)->Ruler, 0u);
	// Two worlds of the same seed agree on every polity and every rule.
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(128), 300));
	VT_REQUIRE(B.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(12);
	B.Ages.Run(12);
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
}

VAELEN_TEST(Polities, DeterministicSnapshotSafeAndFrozen)
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
		const PolityStats S = A.Stats();
		if (S.Bad != 0 || ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance))
		{
			++Failures;
			VT_CHECK_MSG(false, "year %u: %u bad polities, worlds %s", Year, S.Bad,
						 ComputeStateDigest(A.Instance) == ComputeStateDigest(B.Instance) ? "same" : "differ");
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	const PolityStats S = A.Stats();
	VAELEN_LOG_INFO(LogPolities,
					"frozen: polities128=%016llx standing=%u seatings=%u (%u founded, %u dissolved, %u ruled)",
					static_cast<unsigned long long>(S.Digest), S.Standing, S.Seatings, S.Founded, S.Dissolved, S.Ruled);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_POLITIES_FROZEN_128});
	VT_CHECK_EQ(S.Standing, uint32{VAELEN_POLITIES_STANDING_128});
	VT_CHECK_EQ(S.Seatings, uint32{VAELEN_POLITIES_SEATINGS_128});
	VT_REQUIRE(!Image.empty());
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	R.Ages.Run(50);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, S.Digest);
}
