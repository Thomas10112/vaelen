// VAELEN - VaelenSim tests
// History 03.02: RegionPopulation bookkeeping, seeding on the best regions,
// growth bounded by capacity, migration conserving people, assimilation,
// culture splits over centuries, determinism and snapshot continuation,
// frozen digests, ASCII export.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (03.02).
#define VAELEN_POPULATION_FROZEN_128 0xf2afaa068c0f717dull
#define VAELEN_POPULATION_PEOPLE_128 47587ull
#define VAELEN_POPULATION_CULTURES_128 18u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPopulation);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;
	constexpr uint64 Year = 8640;

	struct PopWorld
	{
		explicit PopWorld(uint64 Seed, PopulationRules InRules = PopulationRules{})
			: Instance(Config(Seed)), Rules(InRules)
		{
			Setup = WorldSetup::Declare(Instance);
			Types = PopulationTypes::Declare(Instance);
			Growth = std::make_unique<PopulationSystem>(Instance, Setup, Types, Rules);
			Move = std::make_unique<MigrationSystem>(Instance, Setup, Types, Rules);
			Instance.Systems().Add(Growth.get());
			Instance.Systems().Add(Move.get());
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool Start(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			if (!GenerateWorld(Instance, Setup, Gen))
			{
				return false;
			}
			return SeedCultures(Instance, Setup, Types, Rules, Instance.Now()) > 0;
		}
		World Instance;
		PopulationRules Rules;
		WorldSetup Setup;
		PopulationTypes Types;
		std::unique_ptr<PopulationSystem> Growth;
		std::unique_ptr<MigrationSystem> Move;
	};

	/// Everything that must hold at any tick. Returns failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, const PopWorld& W)
	{
		uint32 Failures = 0;
		auto Fail = [&](const char* What)
		{
			++Failures;
			Ctx.ReportFailure(__FILE__, __LINE__, What);
		};
		uint32 Cultures = 0;
		std::vector<uint32> Homes(1, 0);
		W.Instance.Components()
			.GetPool(W.Types.Culture)
			.ForEach(
				[&](EntityHandle H, const CultureInfo& C)
				{
					++Cultures;
					if (!W.Instance.Entities().GetId(H).IsKind(IdKind::Culture) || C.Index == 0 || C.HomeRegion == 0)
					{
						Fail("culture entity malformed");
					}
					if (Homes.size() <= C.Index)
					{
						Homes.resize(C.Index + 1u, 0);
					}
					Homes[C.Index] = C.HomeRegion;
					if (C.Parent != 0 && C.Generation == 0)
					{
						Fail("split culture with generation 0");
					}
				});
		uint32 Regions = 0;
		W.Instance.Components()
			.GetPool(W.Types.Population)
			.ForEach(
				[&](EntityHandle H, const RegionPopulation& P)
				{
					++Regions;
					if (W.Instance.Components().GetPool(W.Setup.RegionTypes_.Region).TryGet(H) == nullptr)
					{
						Fail("population on a non-region entity");
					}
					uint64 Sum = 0;
					uint32 Best = 0;
					uint32 BestCount = 0;
					for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
					{
						if (P.Culture[S] == 0 && P.Count[S] != 0)
						{
							Fail("count on a free slot");
						}
						if (P.Culture[S] != 0 && (P.Culture[S] >= Homes.size() || Homes[P.Culture[S]] == 0))
						{
							Fail("slot refers to an unknown culture");
						}
						Sum += P.Count[S];
						if (P.Count[S] > BestCount ||
							(P.Count[S] == BestCount && P.Culture[S] != 0 && P.Culture[S] < Best))
						{
							Best = P.Culture[S];
							BestCount = P.Count[S];
						}
					}
					if (Sum != P.Total || (BestCount == 0 ? P.Majority != 0 : P.Majority != Best))
					{
						Fail("region totals or majority inconsistent");
					}
					if (P.Total > 0 && P.SettledSince == 0)
					{
						Fail("settled region with zero settlement years");
					}
				});
		if (Regions != W.Instance.Components().GetPool(W.Setup.RegionTypes_.Region).Size())
		{
			Fail("not every region has a population component");
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(Population, RegionPopulationBookkeepingIsExact)
{
	RegionPopulation P;
	VT_CHECK(P.Add(3, 100) && P.Add(5, 40) && P.Add(3, 20));
	VT_CHECK_EQ(P.Total, 160u);
	VT_CHECK_EQ(P.Majority, 3u);
	VT_CHECK_EQ(P.SlotOf(5), 1u);
	VT_CHECK_EQ(P.Remove(5, 100), 40u); // clamped, slot freed
	VT_CHECK_EQ(P.SlotOf(5), RegionPopulation::MaxCultures);
	VT_CHECK_EQ(P.Total, 120u);
	VT_CHECK(!P.Add(0, 5));			 // culture 0 is not a culture
	VT_CHECK(P.Add(7, 0));			 // adding nobody is fine
	VT_CHECK_EQ(P.Remove(9, 1), 0u); // unknown culture
	for (uint32 C = 10; C < 10 + RegionPopulation::MaxCultures - 1; ++C)
	{
		VT_CHECK(P.Add(C, 1));
	}
	VT_CHECK(!P.Add(99, 1)); // slots full
	// Ties in the majority go to the lower culture index.
	RegionPopulation Q;
	Q.Add(8, 10);
	Q.Add(2, 10);
	VT_CHECK_EQ(Q.Majority, 2u);
	Q.Remove(2, 10);
	Q.Remove(8, 10);
	VT_CHECK_EQ(Q.Majority, 0u);
	VT_CHECK_EQ(Q.Total, 0u);
}

VAELEN_TEST(Population, SeedingPicksTheBestSpreadRegions)
{
	PopWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
	const PopulationStats S = MeasurePopulation(W.Instance, W.Types);
	VT_CHECK_EQ(S.Cultures, W.Rules.SeedCultures);
	VT_CHECK_EQ(S.SettledRegions, W.Rules.SeedCultures);
	VT_CHECK_EQ(S.People, uint64{W.Rules.SeedPeople} * W.Rules.SeedCultures);
	VT_CHECK(S.Capacity > S.People * 20);
	// Seeded regions are pairwise non-adjacent and have high capacity.
	const RegionGraph Graph = BuildRegionGraph(W.Instance.Map(), W.Setup.Regions);
	std::vector<uint32> Homes;
	W.Instance.Components()
		.GetPool(W.Types.Culture)
		.ForEach(
			[&](EntityHandle, const CultureInfo& C)
			{
				Homes.push_back(C.HomeRegion);
				VT_CHECK(C.Parent == 0 && C.Generation == 0 && C.Identity != 0);
			});
	for (usize A = 0; A < Homes.size(); ++A)
	{
		for (usize B = A + 1; B < Homes.size(); ++B)
		{
			VT_CHECK(!Graph.AreAdjacent(static_cast<uint16>(Homes[A]), static_cast<uint16>(Homes[B])));
		}
	}
	// Events: one CultureFounded and one RegionSettled (caused by it) per culture.
	uint32 Founded = 0;
	uint32 Settled = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		Founded += E.Is(CultureFoundedEvent) ? 1u : 0u;
		if (E.Is(RegionSettledEvent))
		{
			++Settled;
			VT_CHECK(E.Cause.IsValid());
		}
	}
	VT_CHECK_EQ(Founded, W.Rules.SeedCultures);
	VT_CHECK_EQ(Settled, W.Rules.SeedCultures);
	// Seeding twice is refused.
	VaelenTest::ScopedAssertCapture Capture;
	VT_CHECK_EQ(SeedCultures(W.Instance, W.Setup, W.Types, W.Rules, 0), 0u);
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
}

VAELEN_TEST(Population, GrowthIsBoundedMigrationConservesAndCulturesSpread)
{
	PopWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	uint64 Previous = 0;
	uint32 Failures = 0;
	for (uint32 Century = 0; Century < 5; ++Century)
	{
		W.Instance.TickMany(Year * 100);
		Failures += CheckInvariants(Ctx, W);
		const PopulationStats S = MeasurePopulation(W.Instance, W.Types);
		// Never above capacity by more than the yearly growth could add.
		VT_CHECK(S.People <= S.Capacity + S.Capacity / 10);
		VT_CHECK(S.People >= Previous / 2); // no collapse without a cause
		Previous = S.People;
		VAELEN_LOG_INFO(LogPopulation, "year %u: %llu people of %llu capacity, %u cultures, %u/%u regions settled",
						(Century + 1) * 100, static_cast<unsigned long long>(S.People),
						static_cast<unsigned long long>(S.Capacity), S.Cultures, S.SettledRegions, S.Regions);
	}
	VT_CHECK_EQ(Failures, 0u);
	const PopulationStats S = MeasurePopulation(W.Instance, W.Types);
	VT_CHECK(S.SettledRegions * 2 > S.Regions);	 // the continent fills up
	VT_CHECK(S.Cultures > W.Rules.SeedCultures); // splits happened
	VT_CHECK(S.People * 2 > S.Capacity);		 // near capacity after 500 years
	// Splits are events with the region as subject and a parent in the payload.
	uint32 Splits = 0;
	uint32 Waves = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(CultureSplitEvent))
		{
			++Splits;
			VT_CHECK(E.Get<CulturePayload>().Parent != 0 && E.Subject.IsValid());
		}
		Waves += E.Is(MigrationWaveEvent) ? 1u : 0u;
	}
	VT_CHECK_EQ(Splits, S.Cultures - W.Rules.SeedCultures);
	VT_CHECK(Waves > 100);
	// Migration alone conserves people: run the migration system by hand on a copy.
	{
		const PopulationStats Before = MeasurePopulation(W.Instance, W.Types);
		RandomStream Stream(1);
		TickContext C;
		C.Tick = W.Instance.Now();
		C.Entities = &W.Instance.Entities();
		C.Components = &W.Instance.Components();
		C.Random = &Stream;
		C.Events = &W.Instance.Events();
		W.Move->Tick(C);
		const PopulationStats After = MeasurePopulation(W.Instance, W.Types);
		VT_CHECK_EQ(After.People, Before.People);
	}
	std::string Picture;
	ExportCultureAscii(W.Instance, W.Setup, W.Types, 64, Picture);
	VT_CHECK_EQ(Picture.size(), 64u * 32u + 32u);
	for (usize Row = 0; Row < 32; Row += 8)
	{
		const std::string Slice = Picture.substr(Row * 65, 8 * 65);
		VAELEN_LOG_INFO(LogPopulation, "AELVOR cultures after 500 years at 128, rows %zu-%zu:\n%s", Row, Row + 7,
						Slice.c_str());
	}
}

VAELEN_TEST(Population, DeterministicAndSnapshotSafe)
{
	PopWorld A(11);
	PopWorld B(11);
	VT_REQUIRE(A.Start(64) && B.Start(64));
	A.Instance.TickMany(Year * 60);
	B.Instance.TickMany(Year * 60);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	PopWorld R(11);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	A.Instance.TickMany(Year * 60 + 5);
	R.Instance.TickMany(Year * 60 + 5);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	VT_CHECK_EQ(CheckInvariants(Ctx, R), 0u);
	// Rules matter: a faster growth reaches more people after the same time.
	PopulationRules Fast;
	Fast.GrowthPerMille = 80;
	PopWorld F(11, Fast);
	VT_REQUIRE(F.Start(64));
	F.Instance.TickMany(Year * 60);
	VT_CHECK(MeasurePopulation(F.Instance, F.Types).People > MeasurePopulation(B.Instance, B.Types).People);
	// A drowned world seeds nothing and ticks harmlessly.
	PopWorld Drowned(12);
	WorldGenConfig Flood;
	Flood.Width = 32;
	Flood.Height = 32;
	Flood.SeaLevel = Fix64::FromInt(100000).Raw;
	VT_REQUIRE(GenerateWorld(Drowned.Instance, Drowned.Setup, Flood));
	VT_CHECK_EQ(SeedCultures(Drowned.Instance, Drowned.Setup, Drowned.Types, Drowned.Rules, 0), 0u);
	Drowned.Instance.TickMany(Year * 2);
	VT_CHECK_EQ(MeasurePopulation(Drowned.Instance, Drowned.Types).People, 0u);
}

VAELEN_TEST(Population, FrozenDigestsAreReproducedByEveryCompilerAndPlatform)
{
	PopWorld W(AelvorSeed);
	VT_REQUIRE(W.Start(128));
	// 500 years: the first splits happen after year 300, so the frozen state
	// covers seeding, growth, migration and culture splits.
	W.Instance.TickMany(Year * 500);
	const Hash64 D = ComputeStateDigest(W.Instance);
	const PopulationStats S = MeasurePopulation(W.Instance, W.Types);
	VAELEN_LOG_INFO(LogPopulation, "frozen: population128=%016llx people=%llu cultures=%u",
					static_cast<unsigned long long>(D), static_cast<unsigned long long>(S.People), S.Cultures);
	VT_CHECK_EQ(D, Hash64{VAELEN_POPULATION_FROZEN_128});
	VT_CHECK_EQ(S.People, uint64{VAELEN_POPULATION_PEOPLE_128});
	VT_CHECK_EQ(S.Cultures, uint32{VAELEN_POPULATION_CULTURES_128});
}
