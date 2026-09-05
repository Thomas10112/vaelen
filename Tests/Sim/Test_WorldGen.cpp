// VAELEN - VaelenSim tests
// WorldGen 02.03: elevation and coastline - invariants, shape measurements,
// parameter and seed sensitivity, snapshot round trip, frozen digests, ASCII
// export, 1024 x 1024 baseline.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"

#include <chrono>
#include <cstdio>
#include <string>

using namespace Vaelen;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (02.03).
#define VAELEN_ELEVATION_FROZEN_64 0xd60a6e03b595c384ull
#define VAELEN_ELEVATION_FROZEN_256 0xccb1b28371d1fbbbull
#define VAELEN_TERRAIN_FROZEN_256 0x7676272e3fcdf6beull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogWorldGen);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct GenWorld
	{
		explicit GenWorld(uint64 Seed) : Instance(Config(Seed)) { Layers = WorldLayers::Declare(Instance.Map()); }
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		bool Generate(uint32 Size, WorldGenConfig Gen = WorldGenConfig{})
		{
			Gen.Width = Size;
			Gen.Height = Size;
			return Instance.Map().Reset(Gen) && GenerateElevation(Instance.Map(), Layers, Instance.Config().Seed);
		}
		World Instance;
		WorldLayers Layers;
	};

	/// Everything that must hold for any generated map. Returns failures.
	uint32 CheckInvariants(VaelenTest::Context& Ctx, const GenWorld& W)
	{
		uint32 Failures = 0;
		auto Fail = [&](const char* What)
		{
			++Failures;
			Ctx.ReportFailure(__FILE__, __LINE__, What);
		};
		const WorldMap& Map = W.Instance.Map();
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<int64>& E = Map.GetLayer(W.Layers.Elevation);
		const TileLayer<uint8>& T = Map.GetLayer(W.Layers.Terrain);
		const TileLayer<int64>& S = Map.GetLayer(W.Layers.Slope);
		const int64 Sea = Map.Config().SeaLevel;
		for (uint32 i = 0; i < Grid.TileCount(); ++i)
		{
			const bool Land = (T[i] & TerrainFlag::Land) != 0;
			if (Land != (E[i] > Sea))
			{
				Fail("land flag disagrees with elevation");
				break;
			}
			if ((T[i] & TerrainFlag::Coast) != 0 && !Land)
			{
				Fail("coast flag on sea");
				break;
			}
			if ((T[i] & TerrainFlag::Shore) != 0 && Land)
			{
				Fail("shore flag on land");
				break;
			}
			if (S[i] < 0)
			{
				Fail("negative slope");
				break;
			}
			const TileCoord C = Grid.CoordOf(i);
			const bool OnBorder = C.X == 0 || C.Y == 0 || static_cast<uint32>(C.X) + 1 == Grid.Width ||
								  static_cast<uint32>(C.Y) + 1 == Grid.Height;
			if (OnBorder != ((T[i] & TerrainFlag::Border) != 0))
			{
				Fail("border flag wrong");
				break;
			}
			// Slope is the max |dz| over the 8 neighbours: recompute for a sample.
			if (i % 97 == 0)
			{
				int64 Expected = 0;
				Grid.ForEachNeighbour(C, 8,
									  [&](TileCoord N, uint32)
									  {
										  const int64 D = E[Grid.IndexOf(N)] - E[i];
										  const int64 M = D < 0 ? -D : D;
										  Expected = M > Expected ? M : Expected;
									  });
				if (Expected != S[i])
				{
					Fail("slope mismatch");
					break;
				}
			}
			// Coast tiles have a sea 4-neighbour; non-coast land tiles have none.
			if (Land && i % 13 == 0)
			{
				bool SeaNext = false;
				Grid.ForEachNeighbour(C, 4, [&](TileCoord N, uint32)
									  { SeaNext = SeaNext || (T[Grid.IndexOf(N)] & TerrainFlag::Land) == 0; });
				if (SeaNext != ((T[i] & TerrainFlag::Coast) != 0))
				{
					Fail("coast flag disagrees with neighbours");
					break;
				}
			}
		}
		return Failures;
	}
} // namespace

VAELEN_TEST(WorldGen, DefaultWorldHasAContinentSurroundedBySea)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	VT_CHECK_EQ(CheckInvariants(Ctx, W), 0u);
	const ElevationStats S = MeasureElevation(W.Instance.Map(), W.Layers);
	const double LandFraction = static_cast<double>(S.LandTiles) / (256.0 * 256.0);
	const double Largest = S.LandTiles > 0 ? static_cast<double>(S.LargestLandmassTiles) / S.LandTiles : 0.0;
	VAELEN_LOG_INFO(LogWorldGen,
					"256: land %.3f, largest landmass %.3f of land, %u landmasses, %u coast tiles, "
					"elevation [%d, %d], max slope %d",
					LandFraction, Largest, S.LandmassCount, S.CoastTiles, S.MinElevation.FloorToInt(),
					S.MaxElevation.FloorToInt(), S.MaxSlope.FloorToInt());
	VT_CHECK(LandFraction > 0.2 && LandFraction < 0.6);
	VT_CHECK(Largest > 0.6);							 // a continent, not confetti
	VT_CHECK_EQ(S.BorderLandTiles, 0u);					 // the sea surrounds the world
	VT_CHECK(S.CoastTiles > 200 && S.CoastTiles < 8000); // neither a blob nor confetti
	VT_CHECK(S.MaxElevation > Fix64::FromInt(1500));	 // mountains exist
	VT_CHECK(S.MinElevation < Fix64::FromInt(-1000));	 // deep sea exists
	VT_CHECK(S.MaxSlope < Fix64::FromInt(1500));		 // no cliffs taller than the relief scale

	std::string Picture;
	ExportAscii(W.Instance.Map(), W.Layers, 64, Picture);
	VT_CHECK_EQ(Picture.size(), 64u * 32u + 32u);
	// The log line is capped at 2048 bytes: print the picture in slices of 8 rows.
	for (usize Row = 0; Row < 32; Row += 8)
	{
		const std::string Slice = Picture.substr(Row * 65, 8 * 65);
		VAELEN_LOG_INFO(LogWorldGen, "AELVOR seed %llx at 256, rows %zu-%zu:\n%s",
						static_cast<unsigned long long>(AelvorSeed), Row, Row + 7, Slice.c_str());
	}
}

VAELEN_TEST(WorldGen, SeedsAndParametersChangeTheWorldDeterministically)
{
	GenWorld A(1);
	GenWorld B(1);
	GenWorld C(2);
	VT_REQUIRE(A.Generate(64) && B.Generate(64) && C.Generate(64));
	VT_CHECK_EQ(A.Instance.Map().StateDigest(), B.Instance.Map().StateDigest());
	VT_CHECK_NE(A.Instance.Map().StateDigest(), C.Instance.Map().StateDigest());
	VT_CHECK_EQ(CheckInvariants(Ctx, C), 0u);

	// More continent bias: more land, never less.
	WorldGenConfig Wet;
	Wet.Params[ParamIndex::ContinentBias] = Fix64::FromRatio(1, 2).Raw;
	GenWorld D(1);
	VT_REQUIRE(D.Generate(64, Wet));
	VT_CHECK(MeasureElevation(D.Instance.Map(), D.Layers).LandTiles >
			 MeasureElevation(A.Instance.Map(), A.Layers).LandTiles);
	// Raising the sea level drowns land.
	WorldGenConfig Flooded;
	Flooded.SeaLevel = Fix64::FromInt(600).Raw;
	GenWorld F(1);
	VT_REQUIRE(F.Generate(64, Flooded));
	VT_CHECK(MeasureElevation(F.Instance.Map(), F.Layers).LandTiles <
			 MeasureElevation(A.Instance.Map(), A.Layers).LandTiles);
	VT_CHECK_EQ(CheckInvariants(Ctx, F), 0u);
	// Elevation itself does not depend on the sea level, only the classification.
	VT_CHECK_EQ(LayerDigest(F.Instance.Map(), F.Layers.Elevation.Index),
				LayerDigest(A.Instance.Map(), A.Layers.Elevation.Index));
	// A non-square map generates and classifies.
	GenWorld R(5);
	WorldGenConfig Wide;
	Wide.Width = 96;
	Wide.Height = 40;
	VT_REQUIRE(R.Instance.Map().Reset(Wide) && GenerateElevation(R.Instance.Map(), R.Layers, 5));
	VT_CHECK_EQ(CheckInvariants(Ctx, R), 0u);
}

VAELEN_TEST(WorldGen, GeneratedMapRoundTripsThroughTheSnapshot)
{
	GenWorld A(77);
	VT_REQUIRE(A.Generate(64));
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	GenWorld B(77);
	VT_REQUIRE(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(B.Instance.Map().StateDigest(), A.Instance.Map().StateDigest());
	VT_CHECK_EQ(CheckInvariants(Ctx, B), 0u);
	// Regenerating in the restored world gives the same map again.
	VT_REQUIRE(GenerateElevation(B.Instance.Map(), B.Layers, 77));
	VT_CHECK_EQ(B.Instance.Map().StateDigest(), A.Instance.Map().StateDigest());
}

VAELEN_TEST(WorldGen, MisuseIsReported)
{
	VaelenTest::ScopedAssertCapture Capture;
	GenWorld W(3);
	VT_CHECK(!GenerateElevation(W.Instance.Map(), W.Layers, 3)); // no grid yet
	std::string Picture;
	ExportAscii(W.Instance.Map(), W.Layers, 32, Picture);
	VT_CHECK(Picture.empty());
	const ElevationStats S = MeasureElevation(W.Instance.Map(), W.Layers);
	VT_CHECK_EQ(S.LandTiles + S.SeaTiles, 0u);
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
}

VAELEN_TEST(WorldGen, FrozenDigestsAreReproducedByEveryCompilerAndPlatform)
{
	GenWorld S64(AelvorSeed);
	VT_REQUIRE(S64.Generate(64));
	GenWorld S256(AelvorSeed);
	VT_REQUIRE(S256.Generate(256));
	const Hash64 E64 = LayerDigest(S64.Instance.Map(), S64.Layers.Elevation.Index);
	const Hash64 E256 = LayerDigest(S256.Instance.Map(), S256.Layers.Elevation.Index);
	const Hash64 T256 = LayerDigest(S256.Instance.Map(), S256.Layers.Terrain.Index);
	VAELEN_LOG_INFO(LogWorldGen, "frozen: elevation64=%016llx elevation256=%016llx terrain256=%016llx",
					static_cast<unsigned long long>(E64), static_cast<unsigned long long>(E256),
					static_cast<unsigned long long>(T256));
	VT_CHECK_EQ(E64, Hash64{VAELEN_ELEVATION_FROZEN_64});
	VT_CHECK_EQ(E256, Hash64{VAELEN_ELEVATION_FROZEN_256});
	VT_CHECK_EQ(T256, Hash64{VAELEN_TERRAIN_FROZEN_256});
}

VAELEN_TEST(WorldGen, DefaultSizeBaseline)
{
	GenWorld W(AelvorSeed);
	const auto Start = std::chrono::steady_clock::now();
	VT_REQUIRE(W.Generate(1024));
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	const ElevationStats S = MeasureElevation(W.Instance.Map(), W.Layers);
	VT_CHECK_EQ(S.BorderLandTiles, 0u);
	VT_CHECK(S.LandTiles > 1024u * 1024u / 5u && S.LandTiles < 1024u * 1024u * 3u / 5u);
	VAELEN_LOG_INFO(LogWorldGen, "baseline: 1024 x 1024 elevation + classification in %.3f s (%.0f tiles/s)%s", Seconds,
					1024.0 * 1024.0 / Seconds, VAELEN_ASSERTS_ENABLED ? " [asserts on]" : " [asserts off]");
}
