// VAELEN - VaelenSim tests
// WorldGen 02.04: climate and biomes - latitude, winds, seasons and biome table
// helpers, invariants on the AELVOR map, rain shadow on a synthetic ridge,
// sensitivity, snapshot round trip, frozen digests, ASCII biome export.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"

#include <cmath>
#include <string>
#include <string_view>

using namespace Vaelen;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-05 (02.04).
#define VAELEN_TEMPERATURE_FROZEN_256 0xa9c96b39c6085337ull
#define VAELEN_MOISTURE_FROZEN_256 0x871f1b4ad5cfe535ull
#define VAELEN_BIOME_FROZEN_256 0x56503eefd26ec6d5ull

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogClimate);

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
			const uint64 Seed = Instance.Config().Seed;
			return Instance.Map().Reset(Gen) && GenerateElevation(Instance.Map(), Layers, Seed) &&
				   GenerateClimate(Instance.Map(), Layers, Seed);
		}
		World Instance;
		WorldLayers Layers;
	};

	double D(Fix64 F)
	{
		return static_cast<double>(F.Raw) / 4294967296.0;
	}
	double D(int64 Raw)
	{
		return static_cast<double>(Raw) / 4294967296.0;
	}
} // namespace

VAELEN_TEST(Climate, HelpersAreExactAndCoverEveryBiome)
{
	const WorldGrid G{8, 5};
	VT_CHECK(LatitudeOfRow(G, 0) == Fix64::FromInt(-1));
	VT_CHECK(LatitudeOfRow(G, 2) == Fix64::Zero());
	VT_CHECK(LatitudeOfRow(G, 4) == Fix64::FromInt(1));
	VT_CHECK(LatitudeOfRow(G, 1) == Fix64::FromRatio(-1, 2));
	VT_CHECK(LatitudeOfRow(WorldGrid{3, 1}, 0) == Fix64::Zero());

	VT_CHECK_EQ(PrevailingWind(Fix64::Zero()), -1);
	VT_CHECK_EQ(PrevailingWind(Fix64::FromRatio(1, 2)), 1);
	VT_CHECK_EQ(PrevailingWind(Fix64::FromRatio(-1, 2)), 1);
	VT_CHECK_EQ(PrevailingWind(Fix64::FromRatio(9, 10)), -1);
	VT_CHECK_EQ(PrevailingWind(Fix64::FromRatio(1, 3)), 1); // band edges belong to the outer band
	VT_CHECK_EQ(PrevailingWind(Fix64::FromRatio(2, 3)), -1);

	VT_CHECK(SeasonalOffset(Fix64::Zero(), 1) == Fix64::FromInt(4));
	VT_CHECK(SeasonalOffset(Fix64::FromInt(1), 1) == Fix64::FromInt(20));
	VT_CHECK(SeasonalOffset(Fix64::FromInt(-1), 3) == Fix64::FromInt(-20));
	VT_CHECK(SeasonalOffset(Fix64::FromRatio(1, 2), 0) == Fix64::Zero());
	VT_CHECK(SeasonalOffset(Fix64::FromRatio(1, 2), 2) == Fix64::Zero());
	VT_CHECK(SeasonalOffset(Fix64::FromRatio(1, 2), 5) == SeasonalOffset(Fix64::FromRatio(1, 2), 1));

	const Fix64 Wet = Fix64::FromRatio(9, 10);
	const Fix64 Mid = Fix64::FromRatio(2, 5);
	const Fix64 Dry = Fix64::FromRatio(1, 10);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(25), Wet, Fix64::FromInt(100), false) == Biome::Ocean);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(25), Wet, Fix64::FromInt(3000), true) == Biome::Alpine);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(-20), Wet, Fix64::FromInt(100), true) == Biome::Ice);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(-5), Wet, Fix64::FromInt(100), true) == Biome::Tundra);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(4), Wet, Fix64::FromInt(100), true) == Biome::BorealForest);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(4), Dry, Fix64::FromInt(100), true) == Biome::ColdSteppe);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(12), Wet, Fix64::FromInt(100), true) == Biome::TemperateForest);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(12), Mid, Fix64::FromInt(100), true) == Biome::Grassland);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(12), Dry, Fix64::FromInt(100), true) == Biome::Scrubland);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(25), Wet, Fix64::FromInt(100), true) == Biome::TropicalForest);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(25), Mid, Fix64::FromInt(100), true) == Biome::Savanna);
	VT_CHECK(ClassifyBiome(Fix64::FromInt(25), Dry, Fix64::FromInt(100), true) == Biome::Desert);
	for (uint32 B = 0; B < static_cast<uint32>(Biome::Count); ++B)
	{
		VT_CHECK(BiomeGlyph(static_cast<Biome>(B)) != '?');
		VT_CHECK(std::string_view(BiomeName(static_cast<Biome>(B))) != "Unknown");
	}
	VT_CHECK_STREQ(BiomeName(Biome::Count), "Unknown");
}

VAELEN_TEST(Climate, AelvorHasBandsLapseAndManyBiomes)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	const WorldMap& Map = W.Instance.Map();
	const WorldGrid& Grid = Map.Grid();
	const TileLayer<int64>& E = Map.GetLayer(W.Layers.Elevation);
	const TileLayer<uint8>& T = Map.GetLayer(W.Layers.Terrain);
	const TileLayer<uint16>& Dist = Map.GetLayer(W.Layers.SeaDistance);
	const TileLayer<int64>& Temp = Map.GetLayer(W.Layers.Temperature);
	const TileLayer<int64>& Moist = Map.GetLayer(W.Layers.Moisture);
	const TileLayer<uint8>& B = Map.GetLayer(W.Layers.Biome);
	const ClimateParams P = ClimateParams::Resolve(Map.Config());

	uint32 Failures = 0;
	double EquatorSum = 0.0;
	double PoleSum = 0.0;
	uint32 EquatorCount = 0;
	uint32 PoleCount = 0;
	for (uint32 I = 0; I < Grid.TileCount(); ++I)
	{
		const bool Land = (T[I] & TerrainFlag::Land) != 0;
		const TileCoord C = Grid.CoordOf(I);
		if (Land != (Dist[I] != 0) || Land != (B[I] != static_cast<uint8>(Biome::Ocean)))
		{
			++Failures;
		}
		if (D(Moist[I]) < 0.0 || D(Moist[I]) > 1.0)
		{
			++Failures;
		}
		// Temperature never exceeds the latitude band plus the noise amplitude.
		const double Band = D(P.EquatorTemperature) - (D(P.EquatorTemperature) - D(P.PoleTemperature)) *
														  std::abs(D(LatitudeOfRow(Grid, static_cast<uint32>(C.Y))));
		if (D(Temp[I]) > Band + D(P.TemperatureNoise) + 1e-6)
		{
			++Failures;
		}
		// Alpine iff above 2500 units on land.
		const bool HighLand = Land && E[I] - Map.Config().SeaLevel > Fix64::FromInt(2500).Raw;
		if (HighLand != (B[I] == static_cast<uint8>(Biome::Alpine)))
		{
			++Failures;
		}
		if (static_cast<uint32>(C.Y) > 120 && static_cast<uint32>(C.Y) < 136)
		{
			EquatorSum += D(Temp[I]);
			++EquatorCount;
		}
		if (static_cast<uint32>(C.Y) < 16 || static_cast<uint32>(C.Y) > 240)
		{
			PoleSum += D(Temp[I]);
			++PoleCount;
		}
	}
	VT_CHECK_EQ(Failures, 0u);
	VT_CHECK(EquatorSum / EquatorCount > PoleSum / PoleCount + 20.0);

	const ClimateStats S = MeasureClimate(Map, W.Layers);
	const ElevationStats ES = MeasureElevation(Map, W.Layers);
	VT_CHECK_EQ(S.BiomeTiles[0], ES.SeaTiles);
	VT_CHECK(S.DistinctLandBiomes >= 7);
	VT_CHECK(S.MaxSeaDistance > 10 && S.MaxSeaDistance < 200);
	VT_CHECK(D(S.MeanLandMoisture) > 0.15 && D(S.MeanLandMoisture) < 0.85);
	VT_CHECK(D(S.MinTemperature) < -5.0 && D(S.MaxTemperature) > 25.0);
	std::string Report;
	for (uint32 K = 0; K < static_cast<uint32>(Biome::Count); ++K)
	{
		Report += BiomeName(static_cast<Biome>(K));
		Report += "=";
		Report += std::to_string(S.BiomeTiles[K]);
		Report += " ";
	}
	VAELEN_LOG_INFO(LogClimate, "256: %s| temperature [%.1f, %.1f], mean land moisture %.2f, max sea distance %u",
					Report.c_str(), D(S.MinTemperature), D(S.MaxTemperature), D(S.MeanLandMoisture),
					unsigned{S.MaxSeaDistance});
	std::string Picture;
	ExportBiomeAscii(Map, W.Layers, 64, Picture);
	VT_CHECK_EQ(Picture.size(), 64u * 32u + 32u);
	for (usize Row = 0; Row < 32; Row += 8)
	{
		const std::string Slice = Picture.substr(Row * 65, 8 * 65);
		VAELEN_LOG_INFO(LogClimate, "AELVOR biomes at 256, rows %zu-%zu:\n%s", Row, Row + 7, Slice.c_str());
	}
}

VAELEN_TEST(Climate, RainShadowFallsBehindARidge)
{
	// A synthetic 64 x 9 map: sea on both ends, flat land, and a ridge at x = 40.
	// Rows sit at mid latitude (westerlies), so the wind blows from x = 0.
	GenWorld W(5);
	WorldGenConfig Gen;
	Gen.Width = 64;
	Gen.Height = 9;
	Gen.Params[ParamIndex::MoistureNoise] = 1; // ~0 noise (raw 1 = 2^-32), non-zero to override the default
	Gen.Params[ParamIndex::TemperatureNoise] = 1;
	VT_REQUIRE(W.Instance.Map().Reset(Gen));
	WorldMap& Map = W.Instance.Map();
	TileLayer<int64>& E = Map.GetLayer(W.Layers.Elevation);
	for (uint32 Y = 0; Y < 9; ++Y)
	{
		for (uint32 X = 0; X < 64; ++X)
		{
			int64 H = Fix64::FromInt(-500).Raw;
			if (X >= 4 && X < 60)
			{
				H = Fix64::FromInt(X == 40 ? 2000 : 100).Raw;
			}
			E[Y * 64 + X] = H;
		}
	}
	ClassifyTerrain(Map, W.Layers);
	VT_REQUIRE(GenerateClimate(Map, W.Layers, 5));
	const TileLayer<int64>& M = Map.GetLayer(W.Layers.Moisture);
	const TileLayer<uint16>& Dist = Map.GetLayer(W.Layers.SeaDistance);
	const uint32 Row = 4; // latitude 0: trade winds blow east -> west, from x = 63
	VT_CHECK_EQ(PrevailingWind(LatitudeOfRow(Map.Grid(), Row)), -1);
	// Upwind of the ridge (x > 40) is wetter than just behind it (x < 40).
	const double Windward = D(M[Row * 64 + 41]);
	const double Ridge = D(M[Row * 64 + 40]);
	const double Leeward = D(M[Row * 64 + 39]);
	const double FarLee = D(M[Row * 64 + 20]);
	VT_CHECK(Ridge > Windward);	  // the climb wrings the parcel out on the ridge
	VT_CHECK(Leeward < Windward); // rain shadow
	VT_CHECK(FarLee < Windward);
	VT_CHECK(FarLee > 0.0); // a little always falls
	// Moisture decays with distance from the upwind sea before the ridge.
	VT_CHECK(D(M[Row * 64 + 58]) > D(M[Row * 64 + 45]));
	// Sea distance: 0 on sea, 1 on the first land tile, grows inland.
	VT_CHECK_EQ(Dist[Row * 64 + 0], 0u);
	VT_CHECK_EQ(Dist[Row * 64 + 4], 1u);
	VT_CHECK_EQ(Dist[Row * 64 + 32], 28u); // every row is land at x = 32: nearest sea is x = 60, 28 steps away
}

VAELEN_TEST(Climate, SeedsAndParametersMatter)
{
	GenWorld A(9);
	GenWorld B(9);
	GenWorld C(10);
	VT_REQUIRE(A.Generate(64) && B.Generate(64) && C.Generate(64));
	VT_CHECK_EQ(LayerDigest(A.Instance.Map(), A.Layers.Biome.Index),
				LayerDigest(B.Instance.Map(), B.Layers.Biome.Index));
	VT_CHECK_NE(LayerDigest(A.Instance.Map(), A.Layers.Biome.Index),
				LayerDigest(C.Instance.Map(), C.Layers.Biome.Index));

	WorldGenConfig Hot;
	Hot.Params[ParamIndex::EquatorTemperature] = Fix64::FromInt(40).Raw;
	Hot.Params[ParamIndex::PoleTemperature] = Fix64::FromInt(5).Raw;
	GenWorld H(9);
	VT_REQUIRE(H.Generate(64, Hot));
	const ClimateStats Cold = MeasureClimate(A.Instance.Map(), A.Layers);
	const ClimateStats Warm = MeasureClimate(H.Instance.Map(), H.Layers);
	const uint32 ColdTropical = Cold.BiomeTiles[static_cast<uint32>(Biome::TropicalForest)] +
								Cold.BiomeTiles[static_cast<uint32>(Biome::Savanna)] +
								Cold.BiomeTiles[static_cast<uint32>(Biome::Desert)];
	const uint32 WarmTropical = Warm.BiomeTiles[static_cast<uint32>(Biome::TropicalForest)] +
								Warm.BiomeTiles[static_cast<uint32>(Biome::Savanna)] +
								Warm.BiomeTiles[static_cast<uint32>(Biome::Desert)];
	VT_CHECK(WarmTropical > ColdTropical);
	VT_CHECK(Warm.MinTemperature > Cold.MinTemperature);
	// The elevation layer is untouched by the climate stage.
	VT_CHECK_EQ(LayerDigest(H.Instance.Map(), H.Layers.Elevation.Index),
				LayerDigest(A.Instance.Map(), A.Layers.Elevation.Index));
	// Misuse: climate before any grid.
	VaelenTest::ScopedAssertCapture Capture;
	GenWorld Empty(1);
	VT_CHECK(!GenerateClimate(Empty.Instance.Map(), Empty.Layers, 1));
	VT_CHECK_EQ(MeasureClimate(Empty.Instance.Map(), Empty.Layers).DistinctLandBiomes, 0u);
	std::string Picture;
	ExportBiomeAscii(Empty.Instance.Map(), Empty.Layers, 8, Picture);
	VT_CHECK(Picture.empty());
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
}

VAELEN_TEST(Climate, SnapshotRoundTripsAllSevenLayers)
{
	GenWorld A(21);
	VT_REQUIRE(A.Generate(64));
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	GenWorld B(21);
	VT_REQUIRE(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(B.Instance.Map().LayerCount(), 7u);
	VT_CHECK_EQ(B.Instance.Map().StateDigest(), A.Instance.Map().StateDigest());
	VT_CHECK_EQ(LayerDigest(B.Instance.Map(), B.Layers.Biome.Index),
				LayerDigest(A.Instance.Map(), A.Layers.Biome.Index));
}

VAELEN_TEST(Climate, FrozenDigestsAreReproducedByEveryCompilerAndPlatform)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	const Hash64 T = LayerDigest(W.Instance.Map(), W.Layers.Temperature.Index);
	const Hash64 M = LayerDigest(W.Instance.Map(), W.Layers.Moisture.Index);
	const Hash64 B = LayerDigest(W.Instance.Map(), W.Layers.Biome.Index);
	VAELEN_LOG_INFO(LogClimate, "frozen: temperature256=%016llx moisture256=%016llx biome256=%016llx",
					static_cast<unsigned long long>(T), static_cast<unsigned long long>(M),
					static_cast<unsigned long long>(B));
	VT_CHECK_EQ(T, Hash64{VAELEN_TEMPERATURE_FROZEN_256});
	VT_CHECK_EQ(M, Hash64{VAELEN_MOISTURE_FROZEN_256});
	VT_CHECK_EQ(B, Hash64{VAELEN_BIOME_FROZEN_256});
}
