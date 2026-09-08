// VAELEN - Tests/Infrastructure
// Phase 09.01: buildings - things raised out of a region's common stock and
// the hands it can spare, counted, and paid for to the unit.
//
// STATUS: PROTOTYPE (Phase 09)

#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Infrastructure/Buildings.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Organizations.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Infrastructure;
using namespace Vaelen::Politics;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-08 (09.01): AELVOR 128 at
// year 300, the busiest region detailed, 120 years with every Phase 04, 05,
// 06 and 07 system this task stands on.
#define VAELEN_WORKS_FROZEN_128 0xd7336afc0a7a3951ull
#define VAELEN_WORKS_STANDING_128 93u
#define VAELEN_WORKS_RAISED_128 93u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogWorks);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BuildingRules InWorks = BuildingRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Economy = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Polities = PolityTypes::Declare(Instance);
			Works = InfrastructureTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy, Production,
														 ProductionRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Rulers =
				std::make_unique<PolitySystem>(Instance, Ages.Types(), Persons, Organizations, Polities, PolityRules{});
			Masons =
				std::make_unique<BuildingSystem>(Instance, Ages.Types(), Families, Economy, Polities, Works, InWorks);
			Houses->RunAfter("Lod");
			Stocks->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Rulers->RunAfter("Lod");
			Harvest->ObserveTraits(Traits.Traits);
			// A year's building comes after that year's harvest and after the
			// polity that may have just taken a seat.
			Masons->RunAfter("Polities");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Rulers.get());
			Instance.Systems().Add(Masons.get());
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
		/// Regions by how many live in them, most peopled first.
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
		uint32 People(uint32 Region) const
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
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						Out = P != nullptr ? static_cast<uint32>(P->Total) : 0u;
					});
			return Out;
		}
		BuildingStats Stats(BuildingRules R = BuildingRules{}) const
		{
			return MeasureBuildings(Instance, Ages.Types(), Works, R);
		}
		const BuildingInfo* Building(uint32 Index) const { return BuildingOf(Instance, Works, Index); }
		const RegionWorks* Held(uint32 Region) const { return WorksOf(Instance, Ages.Types(), Works, Region); }
		uint32 Kept(uint32 Region, Work Kind) const { return KeptSize(Instance, Ages.Types(), Works, Region, Kind); }
		/// Every good of every region's common stock moved by Delta (a gift, or a drain).
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
		uint32 Common(uint32 Region, Good G) const
		{
			const RegionStock* S = StockOf(Instance, Ages.Types(), Economy, Region);
			return S != nullptr ? S->Amount[static_cast<uint32>(G)] : 0u;
		}
		/// Units taken out of a common stock BECAUSE something was built, by good.
		void PaidForBuilding(uint32 Out[GoodCount]) const
		{
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				Out[g] = 0;
			}
			std::vector<PersistentId> Raisings;
			const std::vector<Event>& All = Instance.Log().All();
			for (const Event& E : All)
			{
				if (E.Is(BuildingRaisedEvent) || E.Is(BuildingEnlargedEvent))
				{
					Raisings.push_back(E.Id);
				}
			}
			std::sort(Raisings.begin(), Raisings.end(),
					  [](PersistentId A, PersistentId B) { return A.Value < B.Value; });
			for (const Event& E : All)
			{
				if (!E.Is(StockTakenEvent))
				{
					continue;
				}
				const bool Ours = std::binary_search(Raisings.begin(), Raisings.end(), E.Cause,
													 [](PersistentId A, PersistentId B) { return A.Value < B.Value; });
				if (!Ours)
				{
					continue;
				}
				const StockPayload P = E.Get<StockPayload>();
				if (P.Good < GoodCount)
				{
					Out[P.Good] += P.Amount;
				}
			}
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		EconomyTypes Economy;
		ProductionTypes Production;
		OrganizationTypes Organizations;
		PolityTypes Polities;
		InfrastructureTypes Works;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<PolitySystem> Rulers;
		std::unique_ptr<BuildingSystem> Masons;
	};

	/// A world grown to 300 years, its busiest region detailed, then Years more
	/// years with a gift of goods every year so that the common stocks are not
	/// the thing under test. Returns false when the world would not generate.
	bool Grown(Run& W, uint32 Years, int32 Gift = 200)
	{
		if (!W.Ages.Generate(Run::Square(128), 300))
		{
			return false;
		}
		const std::vector<uint32> Ranked = W.Ranked();
		if (Ranked.empty() || !RequestDetail(W.Instance, W.Lod, Ranked[0]))
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

VAELEN_TEST(Buildings, ARegionRaisesWhatItCanPayFor)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Grown(W, 120));
	const BuildingStats S = W.Stats();
	VAELEN_LOG_INFO(LogWorks, "standing=%u raised=%u enlarged=%u regions=%u bad=%u", S.Standing, S.Raised, S.Enlarged,
					S.Regions, S.Bad);
	VT_CHECK_MSG(S.Bad == 0, "every building must keep its invariants");
	VT_CHECK_MSG(S.Standing > 0, "a world of 128 with goods to spare must have built something");
	VT_CHECK_MSG(S.Raised == S.Standing + S.Ruined, "one raising per building that exists");
	// A granary is the first thing a region wants, so nothing outnumbers it.
	for (uint32 K = 1; K < WorkCount; ++K)
	{
		VT_CHECK(S.Of[K] <= S.Of[static_cast<uint32>(Work::Granary)]);
	}
	// Every region that holds something says so, and says the same thing the
	// buildings on it say (MeasureBuildings checks the second half).
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(!Ranked.empty());
	uint32 WithWorks = 0;
	for (const uint32 R : Ranked)
	{
		const RegionWorks* Held = W.Held(R);
		if (Held != nullptr && Held->Standing > 0)
		{
			++WithWorks;
			VT_CHECK(W.Kept(R, Work::Granary) <= BuildingRules{}.MostOfAKind);
		}
	}
	VT_CHECK_EQ(WithWorks, S.Regions);
}

VAELEN_TEST(Buildings, NothingIsRaisedForFree)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Grown(W, 120));
	const BuildingStats S = W.Stats();
	VT_REQUIRE(S.Standing > 0);

	// What the buildings say they cost is what the raisings took out of the
	// common stocks, to the unit - the log is the proof, not the summary.
	uint32 Paid[GoodCount] = {};
	W.PaidForBuilding(Paid);
	VT_CHECK_EQ(Paid[static_cast<uint32>(Good::Timber)], S.Timber);
	VT_CHECK_EQ(Paid[static_cast<uint32>(Good::Tools)], S.Tools);
	VT_CHECK_EQ(Paid[static_cast<uint32>(Good::Grain)], S.Grain);

	// And the cost is exactly the rule, once per size raised.
	const BuildingRules R;
	const uint32 Sizes = S.Raised + S.Enlarged;
	VT_CHECK_EQ(S.Timber, Sizes * R.TimberPerSize);
	VT_CHECK_EQ(S.Tools, Sizes * R.ToolsPerSize);
	VT_CHECK_EQ(S.Grain, Sizes * R.GrainPerSize);
	VT_CHECK_EQ(S.Hands, Sizes * R.HandsPerSize);

	// And nothing a region cannot pay for is ever raised: a world where no
	// common stock will ever hold the timber builds nothing, for all its people.
	BuildingRules Costly;
	Costly.TimberPerSize = 100000000u;
	Run Poor(AelvorSeed, Costly);
	VT_REQUIRE(Grown(Poor, 40));
	const BuildingStats None = Poor.Stats(Costly);
	VT_CHECK_MSG(None.Raised == 0, "a region that cannot pay for a thing does not get it");
	VT_CHECK_EQ(None.Standing, 0u);
	VT_CHECK_EQ(None.Bad, 0u);
}

VAELEN_TEST(Buildings, RulesAndEdges)
{
	// Nobody builds where nobody lives.
	{
		BuildingRules Never;
		Never.PeopleToBuild = 4000000000u;
		Run W(AelvorSeed, Never);
		VT_REQUIRE(Grown(W, 40));
		VT_CHECK_EQ(W.Stats(Never).Raised, 0u);
	}
	// A region that wants nothing raises nothing, however rich it is.
	{
		BuildingRules Content;
		Content.MostOfAKind = 0;
		Run W(AelvorSeed, Content);
		VT_REQUIRE(Grown(W, 40));
		VT_CHECK_EQ(W.Stats(Content).Raised, 0u);
	}
	// Walls stand where a polity sits, and nowhere else.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 120));
		std::vector<uint32> Seats;
		W.Instance.Components()
			.GetPool(W.Polities.Polity)
			.ForEach(
				[&](EntityHandle, const PolityInfo& P)
				{
					if (P.Dissolved == 0 && P.Seat != 0)
					{
						Seats.push_back(P.Seat);
					}
				});
		std::sort(Seats.begin(), Seats.end());
		uint32 Walls = 0;
		W.Instance.Components()
			.GetPool(W.Works.Building)
			.ForEach(
				[&](EntityHandle, const BuildingInfo& B)
				{
					if (B.Kind != static_cast<uint32>(Work::Wall))
					{
						return;
					}
					++Walls;
					VT_CHECK_MSG(std::binary_search(Seats.begin(), Seats.end(), B.Region) || B.Polity != 0,
								 "a wall stands on ground a polity held when it went up");
				});
		VAELEN_LOG_INFO(LogWorks, "walls=%u seats=%u", Walls, static_cast<uint32>(Seats.size()));
	}
	// A work never grows past what its region wants.
	{
		BuildingRules Small;
		Small.MostOfAKind = 1;
		Run W(AelvorSeed, Small);
		VT_REQUIRE(Grown(W, 120));
		const BuildingStats S = W.Stats(Small);
		VT_CHECK_EQ(S.Enlarged, 0u);
		for (uint32 K = 0; K < WorkCount; ++K)
		{
			VT_CHECK_EQ(S.Size[K], S.Of[K]);
		}
		VT_CHECK_EQ(S.Bad, 0u);
	}
	// Nothing is known about what does not exist.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(W.Ages.Generate(Run::Square(64), 60));
		VT_CHECK(W.Building(0) == nullptr);
		VT_CHECK(W.Building(999999) == nullptr);
		VT_CHECK(W.Held(0) == nullptr);
		VT_CHECK(W.Held(999999) == nullptr);
		VT_CHECK_EQ(W.Kept(999999, Work::Granary), 0u);
		VT_CHECK_EQ(W.Kept(1, Work::Count), 0u);
		std::vector<uint32> In;
		BuildingsIn(W.Instance, W.Works, 0, In);
		VT_CHECK(In.empty());
		VT_CHECK(std::string_view(WorkName(Work::Granary)) == "granary");
		VT_CHECK(std::string_view(WorkName(Work::Count)) == "work");
	}
}

VAELEN_TEST(Buildings, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(Grown(A, 120));
	VT_REQUIRE(Grown(B, 120));
	const BuildingStats SA = A.Stats();
	const BuildingStats SB = B.Stats();
	VT_CHECK_EQ(SA.Digest, SB.Digest);
	VT_CHECK_EQ(SA.Standing, SB.Standing);
	VT_CHECK_EQ(SA.Raised, SB.Raised);
	VT_CHECK_EQ(SA.Bad, 0u);

	// A world saved, reloaded and run on is the same world.
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	VT_REQUIRE(!Image.empty());
	Run C(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(C.Stats().Digest, SA.Digest);
	for (uint32 Year = 0; Year < 20; ++Year)
	{
		A.FillEvery(200);
		A.Ages.Run(1);
		C.FillEvery(200);
		C.Ages.Run(1);
	}
	VT_CHECK_EQ(ComputeStateDigest(C.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(C.Stats().Digest, A.Stats().Digest);

	VAELEN_LOG_INFO(LogWorks, "digest=%016llx standing=%u raised=%u", static_cast<unsigned long long>(SA.Digest),
					SA.Standing, SA.Raised);
#if VAELEN_WORKS_FROZEN_128 != 0x0ull
	VT_CHECK_EQ(SA.Digest, Hash64{VAELEN_WORKS_FROZEN_128});
	VT_CHECK_EQ(SA.Standing, VAELEN_WORKS_STANDING_128);
	VT_CHECK_EQ(SA.Raised, VAELEN_WORKS_RAISED_128);
#endif
}
