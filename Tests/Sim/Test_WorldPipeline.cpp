// VAELEN - VaelenSim tests
// WorldGen 02.08: the Phase 02 gate - the whole pipeline frozen at three sizes,
// regeneration byte for byte, snapshot re-hash, partial runs, edge cases
// (no land, non-square), simulation over a generated world, full baseline.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <chrono>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (02.08). These are the
// digests of the whole world state (layers and entities) after the full
// pipeline; gcc, MSVC and AppleClang must reproduce them.
#define VAELEN_WORLD_FROZEN_64 0x31bd627b7440357aull
#define VAELEN_WORLD_FROZEN_256 0x044cf4cce94d3853ull
#define VAELEN_WORLD_FROZEN_1024 0x1211462eedf18e82ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogWorldPipeline);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct GenWorld
	{
		explicit GenWorld(uint64 Seed) : Instance(Config(Seed))
		{
			Setup = WorldSetup::Declare(Instance);
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool Generate(uint32 W, uint32 H, WorldGenStage Last = WorldGenStage::Deposits,
					  WorldGenConfig Gen = WorldGenConfig{})
		{
			Gen.Width = W;
			Gen.Height = H;
			return GenerateWorld(Instance, Setup, Gen, Last);
		}
		World Instance;
		WorldSetup Setup;
	};

	void LogReport(const char* Label, const WorldGenReport& R)
	{
		VAELEN_LOG_INFO(LogWorldPipeline,
						"%s: land %u, %u landmasses, %u coast; %u land biomes; %u rivers, %u lakes; %u regions; %u "
						"deposits; %u entities",
						Label, R.Elevation.LandTiles, R.Elevation.LandmassCount, R.Elevation.CoastTiles,
						R.Climate.DistinctLandBiomes, R.Hydrology.Rivers, R.Hydrology.Lakes, R.Regions.Regions,
						R.Deposits.Total, R.Entities);
	}
} // namespace

VAELEN_TEST(WorldPipeline, RegenerationIsByteIdenticalAndSnapshotsReHash)
{
	GenWorld A(AelvorSeed);
	GenWorld B(AelvorSeed);
	VT_REQUIRE(A.Generate(128, 128) && B.Generate(128, 128));
	std::vector<uint8> ImageA;
	std::vector<uint8> ImageB;
	SaveSnapshot(A.Instance, ImageA);
	SaveSnapshot(B.Instance, ImageB);
	VT_CHECK(ImageA == ImageB);
	const WorldGenReport R = ReportWorld(A.Instance, A.Setup);
	LogReport("128", R);
	VT_CHECK_EQ(R.Entities, R.Hydrology.Rivers + R.Hydrology.Lakes + R.Regions.Regions + R.Deposits.Total);
	VT_CHECK(R.Entities > 50);
	// Restore into a fresh world: identical digest, identical re-save.
	GenWorld C(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(C.Instance, ImageA.data(), ImageA.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(ComputeStateDigest(C.Instance), ComputeStateDigest(A.Instance));
	std::vector<uint8> ImageC;
	SaveSnapshot(C.Instance, ImageC);
	VT_CHECK(ImageC == ImageA);
	// The restored world reports the same numbers.
	const WorldGenReport RC = ReportWorld(C.Instance, C.Setup);
	VT_CHECK_EQ(RC.Entities, R.Entities);
	VT_CHECK_EQ(RC.Deposits.Total, R.Deposits.Total);
	// Regeneration in the same world (ids move on) still yields the same layers.
	VT_REQUIRE(A.Generate(128, 128));
	VT_CHECK_EQ(A.Instance.Map().StateDigest(), C.Instance.Map().StateDigest());
	VT_CHECK_EQ(A.Instance.Entities().GetAliveCount(), R.Entities);
	// Another seed differs.
	GenWorld D(AelvorSeed + 1);
	VT_REQUIRE(D.Generate(128, 128));
	VT_CHECK_NE(D.Instance.Map().StateDigest(), A.Instance.Map().StateDigest());
}

VAELEN_TEST(WorldPipeline, PartialRunsAndEdgeCasesSucceed)
{
	// Stop after each stage: the layers of later stages stay zero, earlier ones match a full run.
	GenWorld Full(5);
	VT_REQUIRE(Full.Generate(64, 64));
	for (uint32 S = 0; S < static_cast<uint32>(WorldGenStage::Count); ++S)
	{
		GenWorld Partial(5);
		VT_REQUIRE(Partial.Generate(64, 64, static_cast<WorldGenStage>(S)));
		VT_CHECK(WorldGenStageName(static_cast<WorldGenStage>(S))[0] != 'U');
		if (S >= static_cast<uint32>(WorldGenStage::Hydrology))
		{
			// Elevation after hydrology is final (later stages never touch it).
			VT_CHECK_EQ(LayerDigest(Partial.Instance.Map(), Partial.Setup.Layers.Elevation.Index),
						LayerDigest(Full.Instance.Map(), Full.Setup.Layers.Elevation.Index));
		}
		if (S < static_cast<uint32>(WorldGenStage::Deposits))
		{
			VT_CHECK_EQ(MeasureDeposits(Partial.Instance, Partial.Setup.Layers, Partial.Setup.Deposits,
										Partial.Setup.DepositTypes_)
							.Total,
						0u);
		}
	}
	VT_CHECK_STREQ(WorldGenStageName(WorldGenStage::Count), "Unknown");
	// Non-square map.
	GenWorld Wide(7);
	VT_REQUIRE(Wide.Generate(160, 48));
	const WorldGenReport RW = ReportWorld(Wide.Instance, Wide.Setup);
	VT_CHECK(RW.Elevation.LandTiles > 0 && RW.Regions.Regions > 0);
	VT_CHECK_EQ(RW.Elevation.BorderLandTiles, 0u);
	// A drowned world: sea level above every peak, every stage succeeds with nothing to do.
	GenWorld Drowned(9);
	WorldGenConfig Flood;
	Flood.SeaLevel = Fix64::FromInt(100000).Raw;
	VT_REQUIRE(Drowned.Generate(64, 64, WorldGenStage::Deposits, Flood));
	const WorldGenReport RD = ReportWorld(Drowned.Instance, Drowned.Setup);
	VT_CHECK_EQ(RD.Elevation.LandTiles, 0u);
	VT_CHECK_EQ(RD.Entities, 0u);
	VT_CHECK_EQ(RD.Regions.Regions, 0u);
	// The smallest legal map.
	GenWorld Tiny(11);
	VT_REQUIRE(Tiny.Generate(1, 1));
	VT_CHECK_EQ(ReportWorld(Tiny.Instance, Tiny.Setup).Entities, 0u);
	// Invalid config and stage are refused.
	VaelenTest::ScopedAssertCapture Capture;
	GenWorld Bad(1);
	VT_CHECK(!Bad.Generate(0, 64));
	VT_CHECK(!Bad.Generate(64, 64, WorldGenStage::Count));
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 2);
#endif
}

VAELEN_TEST(WorldPipeline, GeneratedWorldSimulatesAndReplays)
{
	// The Phase 01 kernel over a generated world: ticks advance, the state
	// digest changes only by the clock, a snapshot mid-run continues identically.
	GenWorld A(21);
	VT_REQUIRE(A.Generate(64, 64));
	const Hash64 Map0 = A.Instance.Map().StateDigest();
	A.Instance.TickMany(50);
	VT_CHECK_EQ(A.Instance.Now(), SimTick{50});
	VT_CHECK_EQ(A.Instance.Map().StateDigest(), Map0);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	GenWorld B(21);
	VT_REQUIRE(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	A.Instance.TickMany(50);
	B.Instance.TickMany(50);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(B.Instance.Now(), SimTick{100});
}

VAELEN_TEST(WorldPipeline, FrozenWorldDigestsAreReproducedByEveryCompilerAndPlatform)
{
	Hash64 D[3] = {};
	const uint32 Sizes[3] = {64, 256, 1024};
	double Seconds[3] = {};
	WorldGenReport Reports[3];
	for (uint32 K = 0; K < 3; ++K)
	{
		GenWorld W(AelvorSeed);
		const auto Start = std::chrono::steady_clock::now();
		VT_REQUIRE(W.Generate(Sizes[K], Sizes[K]));
		Seconds[K] = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
		D[K] = ComputeStateDigest(W.Instance);
		Reports[K] = ReportWorld(W.Instance, W.Setup);
	}
	VAELEN_LOG_INFO(LogWorldPipeline, "frozen: world64=%016llx world256=%016llx world1024=%016llx",
					static_cast<unsigned long long>(D[0]), static_cast<unsigned long long>(D[1]),
					static_cast<unsigned long long>(D[2]));
	LogReport("64", Reports[0]);
	LogReport("256", Reports[1]);
	LogReport("1024", Reports[2]);
	VAELEN_LOG_INFO(LogWorldPipeline, "baseline: full pipeline 64 in %.3f s, 256 in %.3f s, 1024 in %.3f s%s",
					Seconds[0], Seconds[1], Seconds[2], VAELEN_ASSERTS_ENABLED ? " [asserts on]" : " [asserts off]");
	VT_CHECK_EQ(D[0], Hash64{VAELEN_WORLD_FROZEN_64});
	VT_CHECK_EQ(D[1], Hash64{VAELEN_WORLD_FROZEN_256});
	VT_CHECK_EQ(D[2], Hash64{VAELEN_WORLD_FROZEN_1024});
	// The default world is a continent with everything on it.
	VT_CHECK(Reports[2].Elevation.LandTiles > 1024u * 1024u / 5u);
	VT_CHECK(Reports[2].Climate.DistinctLandBiomes >= 8);
	VT_CHECK(Reports[2].Hydrology.Rivers > 20 && Reports[2].Regions.Regions > 50 && Reports[2].Deposits.Total > 1000);
}
