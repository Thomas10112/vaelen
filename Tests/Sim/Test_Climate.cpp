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
#include "Vaelen/Sim/Climate.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

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

// ---------------------------------------------------------------------------
// Phase 18 task 18.03: the current temperature as a function, the year as a
// shape. ADR-0151.

VAELEN_TEST(Climate, DayOffsetIsSeasonalOffsetAtTheMidpointsAndLinearBetween)
{
	// EXACT ON THE RAW VALUE at the four midpoints, for latitudes on both
	// sides of the equator and at the pole: the day wave and the season
	// samples are two spellings of one thing, and this is where they are held
	// together. A wave that drifted from the samples by one raw unit would
	// still draw fine and would be a different climate from 02.04's.
	const Fix64 Lats[] = {Fix64::Zero(), Fix64::FromRatio(1, 2), Fix64::FromRatio(-1, 2), Fix64::FromInt(1),
						  Fix64::FromInt(-1)};
	uint32 Held = 0;
	for (const Fix64 Lat : Lats)
	{
		for (uint32 Season = 0; Season < 4u; ++Season)
		{
			const Fix64 Day = DayOffset(Lat, 45u + 90u * Season);
			const Fix64 Sample = SeasonalOffset(Lat, Season);
			VT_CHECK_MSG(Day.Raw == Sample.Raw, "lat %.3f season %u: DayOffset %.6f, SeasonalOffset %.6f", D(Lat),
						 Season, D(Day), D(Sample));
			Held += Day.Raw == Sample.Raw ? 1u : 0u;
		}
		// Linear between: consecutive days differ by A / 90 in magnitude, to
		// the fixed point's rounding of one raw unit.
		const Fix64 Step = SeasonalAmplitude(Lat) / Fix64::FromInt(90);
		uint32 Off = 0;
		for (uint32 Day = 0; Day < 360u; ++Day)
		{
			const Fix64 Delta = Fix64::Abs(DayOffset(Lat, Day + 1u) - DayOffset(Lat, Day));
			const int64 Gap = Delta.Raw > Step.Raw ? Delta.Raw - Step.Raw : Step.Raw - Delta.Raw;
			Off += Gap > 1 ? 1u : 0u;
		}
		VT_CHECK_MSG(Off == 0u, "lat %.3f: %u day steps are not A/90", D(Lat), Off);
		// And a year sums to nothing: the winter half is the summer half's
		// negation, day for day.
		Fix64 Sum = Fix64::Zero();
		for (uint32 Day = 0; Day < 360u; ++Day)
		{
			Sum += DayOffset(Lat, Day);
		}
		VT_CHECK_MSG(Sum.Raw == 0, "lat %.3f: the year's offsets sum to %.6f, not 0", D(Lat), D(Sum));
	}
	VT_CHECK_EQ(Held, 20u);
	// The amplitude is SeasonalOffset's: 4 at the equator, 20 at the pole.
	VT_CHECK(SeasonalAmplitude(Fix64::Zero()) == Fix64::FromInt(4));
	VT_CHECK(SeasonalAmplitude(Fix64::FromInt(-1)) == Fix64::FromInt(20));
	// A day past the year wraps, and a year with no quarters offers nothing.
	VT_CHECK(DayOffset(Fix64::FromInt(1), 405u).Raw == DayOffset(Fix64::FromInt(1), 45u).Raw);
	VT_CHECK(DayOffset(Fix64::FromInt(1), 10u, 3u).Raw == 0);
}

VAELEN_TEST(Climate, TheYearShapeIsPinned)
{
	// THE PANEL'S FIGURES, recomputed here in Fix64 from the wave above and
	// pinned as the DISCRETE sums (ADR-0151 planned an arithmetic series and
	// this is the sum itself, which is the definition). Frost days, growing
	// days, and the cold sum floored to the degree-day; the panel's cold sums
	// were rounded to the nearest, which is why 5513 reads 5512 here.
	struct Pin
	{
		const char* Where;
		Fix64 Mean;
		Fix64 Lat;
		uint32 Frost;
		uint32 Growing;
		int32 ColdSumFloor;
	};
	const Pin Pins[] = {
		{"the pole", Fix64::FromInt(-15), Fix64::FromInt(1), 315u, 1u, 5512},
		{"row 32 of 128", Fix64::FromRatio(768, 100), Fix64::FromRatio(63, 127), 65u, 221u, 136},
		{"the equator", Fix64::FromRatio(2965, 100), Fix64::FromRatio(1, 127), 0u, 360u, 0},
		{"tundra at |lat| 0.7", Fix64::FromInt(-2), Fix64::FromRatio(7, 10), 203u, 97u, 1751},
		{"grassland at |lat| 0.489", Fix64::FromInt(8), Fix64::FromRatio(489, 1000), 59u, 225u, 111},
	};
	const ClimateRules Rules;
	for (const Pin& P : Pins)
	{
		const YearShape Y = ShapeYear(P.Mean, P.Lat, Rules);
		VT_CHECK_MSG(Y.FrostDays == P.Frost && Y.GrowingDays == P.Growing && Y.ColdSum.FloorToInt() == P.ColdSumFloor,
					 "%s: frost %u growing %u cold %d, pinned %u / %u / %d", P.Where, Y.FrostDays, Y.GrowingDays,
					 Y.ColdSum.FloorToInt(), P.Frost, P.Growing, P.ColdSumFloor);
		VT_CHECK_MSG(Y.FrostDays + Y.GrowingDays <= 360u, "%s: a day cannot both freeze and grow", P.Where);
	}
	// The pole's extremes exactly: -15 -+ 20.
	const YearShape Pole = ShapeYear(Fix64::FromInt(-15), Fix64::FromInt(1), Rules);
	VT_CHECK(Pole.Coldest == Fix64::FromInt(-35) && Pole.Warmest == Fix64::FromInt(5));

	// THE SATURATIONS, the self-check of ADR-0149 rule 1: a place far above
	// both lines freezes never and grows always with no cold at all; a place
	// far below freezes always, grows never, and its cold sum is the mean's
	// deficit times the year EXACTLY, because the offsets sum to zero.
	const YearShape Hot = ShapeYear(Fix64::FromInt(100), Fix64::FromInt(1), Rules);
	VT_CHECK(Hot.FrostDays == 0u && Hot.GrowingDays == 360u && Hot.ColdSum.Raw == 0);
	VT_CHECK(Hot.Coldest == Fix64::FromInt(80) && Hot.Warmest == Fix64::FromInt(120));
	const YearShape Dead = ShapeYear(Fix64::FromInt(-100), Fix64::FromInt(1), Rules);
	VT_CHECK(Dead.FrostDays == 360u && Dead.GrowingDays == 0u);
	VT_CHECK_MSG(Dead.ColdSum == Fix64::FromInt(36000), "cold sum %.3f, wanted 36000 exactly", D(Dead.ColdSum));
	// The lines are read: raise the cold line to +100 and every day is frost.
	ClimateRules Cruel;
	Cruel.ColdLine = Fix64::FromInt(100);
	VT_CHECK(ShapeYear(Fix64::FromInt(8), Fix64::FromRatio(489, 1000), Cruel).FrostDays == 360u);
	// And a zero-day year shapes to nothing rather than to the type's extremes.
	const YearShape None = ShapeYear(Fix64::FromInt(8), Fix64::Zero(), Rules, 0u);
	VT_CHECK(None.FrostDays == 0u && None.Coldest.Raw == 0 && None.Warmest.Raw == 0);
}

VAELEN_TEST(Climate, TileTemperatureOnAelvorInMidwinterAndMidsummer)
{
	GenWorld W(AelvorSeed);
	VT_REQUIRE(W.Generate(256));
	const WorldMap& Map = W.Instance.Map();
	const WorldGrid& Grid = Map.Grid();
	const TileLayer<uint8>& B = Map.GetLayer(W.Layers.Biome);
	const uint32 Tiles = Grid.Width * Grid.Height;

	// Day 315 is mid-winter. Every Ice and Tundra tile is below freezing
	// (their means are below 0, the winter takes at least 4 more); no
	// Tropical, Savanna or Desert tile is (means of 20 and up, within |lat|
	// 0.27 of the equator, so the winter takes 8.3 at most).
	uint32 Ice = 0, Tropic = 0, Frost = 0, Differ = 0, WrongCold = 0, WrongWarm = 0;
	for (uint32 Tile = 0; Tile < Tiles; ++Tile)
	{
		const Fix64 Winter = TileTemperatureOn(Map, W.Layers, Tile, 315u);
		const Fix64 Summer = TileTemperatureOn(Map, W.Layers, Tile, 135u);
		const Biome Kind = static_cast<Biome>(B[Tile]);
		if (Kind == Biome::Ice || Kind == Biome::Tundra)
		{
			++Ice;
			WrongCold += Winter < Fix64::Zero() ? 0u : 1u;
		}
		if (Kind == Biome::TropicalForest || Kind == Biome::Savanna || Kind == Biome::Desert)
		{
			++Tropic;
			WrongWarm += Winter < Fix64::Zero() ? 1u : 0u;
		}
		Frost += Winter < Fix64::Zero() ? 1u : 0u;
		Differ += Summer.Raw != Winter.Raw ? 1u : 0u;
	}
	VT_CHECK_MSG(Ice > 0u && Tropic > 0u,
				 "the map must hold both cold and warm biomes to say anything: %u ice, %u tropic", Ice, Tropic);
	VT_CHECK_MSG(WrongCold == 0u, "%u of %u Ice/Tundra tiles are above freezing in mid-winter", WrongCold, Ice);
	VT_CHECK_MSG(WrongWarm == 0u, "%u of %u Tropical/Savanna/Desert tiles freeze in mid-winter", WrongWarm, Tropic);
	VT_CHECK_MSG(Differ == Tiles, "summer and winter differ on %u of %u tiles - every tile has a latitude", Differ,
				 Tiles);
	VT_CHECK_MSG(Frost > Ice, "mid-winter freezes %u tiles, more than the %u that are ice or tundra all year", Frost,
				 Ice);
	// Off the map: zero, not a read past the layer.
	VT_CHECK(TileTemperatureOn(Map, W.Layers, Tiles, 315u).Raw == 0);
	VT_CHECK(TileTemperatureOn(Map, W.Layers, Tiles + 1000u, 135u).Raw == 0);
	// The generation digests of 02.04 did not move: this task READS the
	// layer, it does not write it.
	VT_CHECK_EQ(LayerDigest(Map, W.Layers.Temperature.Index), VAELEN_TEMPERATURE_FROZEN_256);
	VT_CHECK_EQ(LayerDigest(Map, W.Layers.Biome.Index), VAELEN_BIOME_FROZEN_256);
}

VAELEN_TEST(Climate, YearVariationIsAHashAndNotADraw)
{
	const Fix64 Amp = Fix64::FromInt(3);
	// Reproduces itself, is bounded, and differs between years and regions.
	VT_CHECK(YearVariation(AelvorSeed, 7u, 3u, Amp).Raw == YearVariation(AelvorSeed, 7u, 3u, Amp).Raw);
	uint32 Distinct = 0, Bounded = 0;
	Fix64 Last = YearVariation(AelvorSeed, 0u, 1u, Amp);
	for (uint64 Year = 1; Year <= 10u; ++Year)
	{
		const Fix64 V = YearVariation(AelvorSeed, Year, 1u, Amp);
		Bounded += Fix64::Abs(V) <= Amp ? 1u : 0u;
		Distinct += V.Raw != Last.Raw ? 1u : 0u;
		Last = V;
	}
	VT_CHECK_EQ(Bounded, 10u);
	VT_CHECK_MSG(Distinct >= 2u, "ten years of region 1 vary in only %u places", Distinct);
	VT_CHECK(YearVariation(AelvorSeed, 5u, 1u, Amp).Raw != YearVariation(AelvorSeed, 5u, 2u, Amp).Raw);
	VT_CHECK(YearVariation(AelvorSeed, 5u, 1u, Amp).Raw != YearVariation(AelvorSeed + 1u, 5u, 1u, Amp).Raw);
	// Zero amplitude is zero variation, whatever the hash says.
	VT_CHECK(YearVariation(AelvorSeed, 5u, 1u, Fix64::Zero()).Raw == 0);
}

VAELEN_TEST(Climate, RegionYearsAreTheCentroidsYearsAndAgreeInOnePass)
{
	// A world with regions, through the pipeline, so RegionYear has a pool to
	// read. 64 tiles a side is enough rows to hold a polar and a temperate
	// region.
	WorldConfig C;
	C.Seed = AelvorSeed;
	World W(C);
	const WorldSetup Setup = WorldSetup::Declare(W);
	W.Build();
	WorldGenConfig Gen;
	Gen.Width = 64;
	Gen.Height = 64;
	VT_REQUIRE(GenerateWorld(W, Setup, Gen, WorldGenStage::Regions));
	const ClimateRules Rules;

	std::vector<YearShape> All;
	ShapeRegionYears(W, Setup, 3u, Rules, All);
	VT_REQUIRE(All.size() > 2u);
	uint32 Agree = 0, Peopled = 0;
	Fix64 ColdestSum = Fix64::Zero(), WarmestSum = Fix64::Zero();
	for (uint32 Region = 1; Region < All.size(); ++Region)
	{
		const YearShape One = RegionYear(W, Setup, Region, 3u, Rules);
		const bool Same = One.FrostDays == All[Region].FrostDays && One.GrowingDays == All[Region].GrowingDays &&
						  One.ColdSum.Raw == All[Region].ColdSum.Raw && One.Coldest.Raw == All[Region].Coldest.Raw;
		Agree += Same ? 1u : 0u;
		++Peopled;
		ColdestSum = Fix64::MinOf(ColdestSum, One.Coldest);
		WarmestSum = Fix64::MaxOf(WarmestSum, One.Warmest);
	}
	VT_CHECK_MSG(Agree == Peopled, "%u of %u regions agree between the pass and the single call", Agree, Peopled);
	VT_CHECK_MSG(ColdestSum < Fix64::FromInt(-5) && WarmestSum > Fix64::FromInt(20),
				 "the map's regions span the year: coldest %.1f, warmest %.1f", D(ColdestSum), D(WarmestSum));
	// The year is READ: two years of one region differ, by the variation and
	// by nothing else (the mean and the row are the same).
	const YearShape Y3 = RegionYear(W, Setup, 1u, 3u, Rules);
	const YearShape Y4 = RegionYear(W, Setup, 1u, 4u, Rules);
	VT_CHECK_MSG(Y3.Coldest.Raw != Y4.Coldest.Raw, "region 1 is the same in years 3 and 4 (%.3f)", D(Y3.Coldest));
	ClimateRules Still = Rules;
	Still.YearAmplitude = Fix64::Zero();
	VT_CHECK(RegionYear(W, Setup, 1u, 3u, Still).Coldest.Raw == RegionYear(W, Setup, 1u, 4u, Still).Coldest.Raw);
	// Region 0 and a region the world does not have: an all-zero shape.
	const YearShape Nowhere = RegionYear(W, Setup, 0u, 3u, Rules);
	const YearShape Beyond = RegionYear(W, Setup, static_cast<uint32>(All.size()) + 5u, 3u, Rules);
	VT_CHECK(Nowhere.FrostDays == 0u && Nowhere.GrowingDays == 0u && Nowhere.Coldest.Raw == 0);
	VT_CHECK(Beyond.FrostDays == 0u && Beyond.GrowingDays == 0u && Beyond.Warmest.Raw == 0);
	// And the shape is the centroid tile's: recomputed here by hand for one
	// region, from the layer, the row and the variation.
	{
		const WorldMap& Map = W.Map();
		uint32 Centroid = 0;
		W.Components()
			.GetPool(Setup.RegionTypes_.Region)
			.ForEach(
				[&Centroid](EntityHandle, const RegionInfo& R)
				{
					if (R.Index == 1u)
					{
						Centroid = R.CentroidTile;
					}
				});
		const TileLayer<int64>& Mean = Map.GetLayer(Setup.Layers.Temperature);
		const Fix64 ByHand = Fix64::FromRaw(Mean[Centroid]) + YearVariation(AelvorSeed, 3u, 1u, Rules.YearAmplitude);
		const YearShape Hand = ShapeYear(ByHand, LatitudeOfRow(Map.Grid(), Centroid / Map.Grid().Width), Rules);
		VT_CHECK(Hand.FrostDays == Y3.FrostDays && Hand.ColdSum.Raw == Y3.ColdSum.Raw &&
				 Hand.Warmest.Raw == Y3.Warmest.Raw);
	}
}
