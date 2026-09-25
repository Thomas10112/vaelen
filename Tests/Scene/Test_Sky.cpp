// VAELEN - Phase 19 task 19.09: the cold and the heat can be seen.
//
// Snow, grass, the sun and the body's weather are read from the climate leaf
// and the life (Sky.h). These cases hold them to the leaf by independent
// counts: snow exactly where today is below the frost line, a winter whiter
// than a summer and a summer greener than a winter, a sun that rises and sets
// within the day's hours and stands higher at mid-summer, breath in the cold -
// and a world without a climate painted exactly as 19.05 painted it.
#include "Vaelen/Scene/Sky.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/View/Take.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <utility>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Scene;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogSceneSky);

	struct Taken
	{
		Ground G;
		View::ClimateView Climate;
		uint32 FirstCentroidRow = 0; ///< the row of region 1's centroid: where the Atlas puts the sun
	};

	/// AELVOR 128 on its first day of history, in the climate world.
	const Taken& World128()
	{
		static const Taken T = []
		{
			Taken Out;
			View::MapView Map;
			Run::Options O;
			O.Size = 128;
			O.PreHistory = 1;
			O.Years = 0;
			Run::Aelvor A(O);
			if (A.Begin())
			{
				View::TakeMapView(A.Instance(), A.Sources(), Map);
				View::TakeClimateView(A.Instance(), A.Sources(), Out.Climate);
				View::WorldView Frame;
				View::TakeView(A.Instance(), A.Sources(), Frame);
				Out.FirstCentroidRow = Frame.Regions.empty() ? 0u : Frame.Regions[0].CentroidTile / 128u;
			}
			BuildGround(Map, SceneScale{}, Out.G);
			return Out;
		}();
		return T;
	}

	/// The same world on another day of the year, rebuilt from the leaf's own
	/// mid-winter or mid-summer bytes with the kernel's lines (Sim/Climate.h:
	/// frost below 0, growing at 5 and above) - the second instrument.
	View::ClimateView DayOf(const View::ClimateView& V, bool Winter)
	{
		View::ClimateView Out = V;
		Out.Day = Winter ? 315u : 135u;
		for (View::TileClimate& T : Out.Tiles)
		{
			T.Now = Winter ? T.Winter : T.Summer;
			T.Flags = static_cast<uint8>((T.Now < 0 ? View::ClimateFlag::Frost : 0u) |
										 (T.Now >= 5 ? View::ClimateFlag::Growing : 0u));
		}
		return Out;
	}

	/// Land, lake or river tiles below zero today, counted from the bytes, not the flags.
	uint32 ColdLand(const Ground& G, const View::ClimateView& V)
	{
		uint32 N = 0;
		for (usize i = 0; i < V.Tiles.size(); ++i)
		{
			N += G.Kind[i] != GroundKind::Sea && V.Tiles[i].Now < 0 ? 1u : 0u;
		}
		return N;
	}

	View::TileClimate Tile(int8 Now, uint8 Flags)
	{
		View::TileClimate T;
		T.Now = Now;
		T.Flags = Flags;
		return T;
	}
} // namespace

VAELEN_TEST(Sky, SnowIsTodaysFrostAndNothingElse)
{
	// One tile at a time: none without the flag, whatever the byte says.
	VT_CHECK_EQ(SnowOf(Tile(-20, 0u)), 0u);
	VT_CHECK_EQ(SnowOf(Tile(-5, View::ClimateFlag::Frost)), 255u);
	VT_CHECK_EQ(SnowOf(Tile(-30, View::ClimateFlag::Frost)), 255u);
	const uint8 Thin = SnowOf(Tile(-1, View::ClimateFlag::Frost));
	const uint8 Thicker = SnowOf(Tile(-4, View::ClimateFlag::Frost));
	VT_CHECK(Thin > 0u && Thin < Thicker && Thicker < 255u);
	VT_CHECK_EQ(GrassOf(Tile(12, View::ClimateFlag::Growing)), 24u);
	VT_CHECK_EQ(GrassOf(Tile(12, 0u)), 0u);

	const Taken& W = World128();
	VT_REQUIRE(W.G.Width == 128u);
	VT_REQUIRE(W.Climate.Tiles.size() == 128u * 128u);
	View::LifeView Nobody;
	const SkyStats Today = MeasureSky(W.G, W.Climate, Nobody, 64);
	VT_CHECK_EQ(Today.Snow, ColdLand(W.G, W.Climate));

	// A winter's day is whiter than a summer's, and a summer's greener.
	const View::ClimateView Winter = DayOf(W.Climate, true);
	const View::ClimateView Summer = DayOf(W.Climate, false);
	const SkyStats Cold = MeasureSky(W.G, Winter, Nobody, 64);
	const SkyStats Warm = MeasureSky(W.G, Summer, Nobody, 64);
	VT_CHECK_EQ(Cold.Snow, ColdLand(W.G, Winter));
	VT_CHECK_EQ(Warm.Snow, ColdLand(W.G, Summer));
	VT_CHECK(Cold.Snow > Warm.Snow);
	VT_CHECK(Warm.Growing > Cold.Growing);
	VT_CHECK(Cold.Digest != Warm.Digest);

	// The Atlas's line (Atlas.SceneSky128: one year on, the same day of the
	// year, noon of sixteen hours) by this wiring: Run::Aelvor, not the Atlas's.
	View::LifeView Noon;
	Noon.Awake = 16;
	Noon.Spent = 8;
	VT_CHECK_DIGEST_EQ(MeasureSky(W.G, W.Climate, Noon, W.FirstCentroidRow).Digest, 0xb6f8dce3d92162c3ull);

	char Line[SkyLineBytes];
	for (const auto& [Day, S] : {std::pair<uint32, SkyStats>{W.Climate.Day + 1u, Today}, {316u, Cold}, {136u, Warm}})
	{
		VT_CHECK(SkyLine(128, 0x41454c564f52ull, Day, S, Line, SkyLineBytes) > 0u);
		VAELEN_LOG_INFO(LogSceneSky, "%s", Line);
	}
}

VAELEN_TEST(Sky, TheMeshTurnsWhiteWhereItSnowsAndOnlyThere)
{
	const Taken& W = World128();
	const View::ClimateView Winter = DayOf(W.Climate, true);
	uint32 Whitened = 0, Unchanged = 0, SeaFrozen = 0;
	for (uint32 CY = 0; CY < ChunksDown(W.G); ++CY)
	{
		for (uint32 CX = 0; CX < ChunksAcross(W.G); ++CX)
		{
			TerrainMesh Before;
			VT_REQUIRE(BuildChunk(W.G, CX, CY, static_cast<uint32>(W.G.Scale.Steps), Before));
			TerrainMesh After = Before;
			ApplyClimate(W.G, Winter, After);
			VT_REQUIRE(After.Vertices.size() == Before.Vertices.size());
			for (usize i = 0; i < Before.Vertices.size(); ++i)
			{
				const TerrainVertex& B = Before.Vertices[i];
				const TerrainVertex& A = After.Vertices[i];
				// Only the colour: the ground under the walker is the same ground.
				VT_CHECK(A.X == B.X && A.Y == B.Y && A.Z == B.Z && A.NX == B.NX && A.NY == B.NY && A.NZ == B.NZ);
				uint32 T = 0;
				const bool On = TileOfPoint(W.G, A.X, A.Y, T);
				if (On && W.G.Kind[T] == GroundKind::Sea)
				{
					// The sea is not painted: no snow lies on it, no grass grows.
					VT_CHECK(A.R == B.R && A.G == B.G && A.B == B.B);
					SeaFrozen += SnowOf(Winter.Tiles[T]) != 0u ? 1u : 0u;
					continue;
				}
				if (!On || SnowOf(Winter.Tiles[T]) != 255u)
				{
					continue;
				}
				// Full snow is the snow's colour, whatever the biome under it.
				VT_CHECK(A.R == 236u && A.G == 240u && A.B == 246u);
				Whitened += 1u;
			}
			// CONTROL: a world without a climate is painted exactly as 19.05 painted it.
			TerrainMesh Bare = Before;
			ApplyClimate(W.G, View::ClimateView{}, Bare);
			TerrainStats X, Y;
			MeasureTerrain(Before, X);
			MeasureTerrain(Bare, Y);
			VT_CHECK_DIGEST_EQ(X.Digest, Y.Digest);
			Unchanged += X.Digest == Y.Digest ? 1u : 0u;
		}
	}
	VT_CHECK(Whitened > 1000u);
	VT_CHECK(SeaFrozen > 0u); // the case is not empty: there is sea below freezing
	VT_CHECK_EQ(Unchanged, ChunksAcross(W.G) * ChunksDown(W.G));
	VAELEN_LOG_INFO(LogSceneSky, "mid-winter: %u vertices under full snow; %u chunks unchanged without a climate",
					Whitened, Unchanged);
}

VAELEN_TEST(Sky, TheSunRisesAndSetsWithinTheDaysHours)
{
	const uint32 Height = 128;
	const uint32 Row = 32; // half-way to the pole
	// Nobody awake, no sun.
	VT_CHECK_EQ(SunOf(Row, Height, 135, 0, 0).Elevation, 0);
	// Over sixteen waking hours: from the horizon in the east, up to the south
	// at the middle, back to the horizon in the west.
	int32 Last = -1;
	for (uint32 Spent = 0; Spent <= 16u; ++Spent)
	{
		const Sun S = SunOf(Row, Height, 135, Spent, 16);
		VT_CHECK(S.Azimuth >= 900 && S.Azimuth <= 2700);
		if (Spent <= 8u)
		{
			VT_CHECK(S.Elevation > Last);
		}
		else
		{
			VT_CHECK(S.Elevation < Last);
		}
		Last = S.Elevation;
	}
	VT_CHECK_EQ(SunOf(Row, Height, 135, 0, 16).Elevation, 0);
	VT_CHECK_EQ(SunOf(Row, Height, 135, 16, 16).Elevation, 0);
	VT_CHECK_EQ(SunOf(Row, Height, 135, 8, 16).Azimuth, 1800);
	// Mid-summer's noon stands no lower than mid-winter's on any row, and on
	// every row farther from the equator than the tropic (23.4 deg) higher by
	// the declination twice, 46.8 deg; a row nearer the pole has it lower.
	uint32 Beyond = 0;
	for (uint32 R = 0; R < Height; ++R)
	{
		const int32 High = SunOf(R, Height, 135, 8, 16).Elevation;
		const int32 Low = SunOf(R, Height, 315, 8, 16).Elevation;
		VT_CHECK(High >= Low);
		const int32 Twice = 2 * static_cast<int32>(R) - static_cast<int32>(Height - 1u);
		if ((Twice < 0 ? -Twice : Twice) * 90 >= 24 * static_cast<int32>(Height - 1u))
		{
			VT_CHECK_EQ(High - Low, 468);
			++Beyond;
		}
	}
	VT_CHECK(Beyond > Height / 2u);
	VT_CHECK(SunOf(0, Height, 135, 8, 16).Elevation < SunOf(60, Height, 135, 8, 16).Elevation);
	// The equinoxes are halfway between.
	VT_CHECK_EQ(SunOf(Row, Height, 45, 8, 16).Elevation * 2,
				SunOf(Row, Height, 135, 8, 16).Elevation + SunOf(Row, Height, 315, 8, 16).Elevation);
}

VAELEN_TEST(Sky, TheSunMovesWithTheHoursSpentAndOnlyWithThem)
{
	// The world has no clock inside a day (ADR-0138): the sky is a function
	// of the hours the life has spent, never of a tick or a frame.
	const Taken& W = World128();
	View::LifeView Morning;
	Morning.Person = 7;
	Morning.Alive = 1;
	Morning.Awake = 16;
	Morning.Spent = 2;
	View::LifeView Later = Morning;
	Later.Tick = 999999;
	Later.DaysLived = 40;
	Later.Year = 3;
	View::LifeView Noon = Morning;
	Noon.Spent = 8;
	const SkyStats A = MeasureSky(W.G, W.Climate, Morning, 64);
	const SkyStats B = MeasureSky(W.G, W.Climate, Later, 64);
	const SkyStats C = MeasureSky(W.G, W.Climate, Noon, 64);
	VT_CHECK_DIGEST_EQ(A.Digest, B.Digest);
	VT_CHECK(A.Elevation < C.Elevation);
	VT_CHECK(A.Digest != C.Digest);
	// CONTROL: the tiles' snow is the day's, not the hour's.
	VT_CHECK_EQ(A.Snow, C.Snow);
}

VAELEN_TEST(Sky, BreathInTheColdAndAShiverFromTheChill)
{
	View::LifeView Life;
	Life.Person = 7;
	Life.Alive = 1;
	Life.Degrees = -1;
	Life.Chill = 40;
	VT_CHECK_EQ(BodyOf(Life).Breath, 1u);
	VT_CHECK_EQ(BodyOf(Life).Shiver, 40u);
	Life.Degrees = 0;
	Life.Chill = 900;
	VT_CHECK_EQ(BodyOf(Life).Breath, 0u);
	VT_CHECK_EQ(BodyOf(Life).Shiver, 255u);
	// CONTROL: nobody played, or the played person dead - no body to breathe.
	Life.Degrees = -100;
	Life.Alive = 0;
	VT_CHECK_EQ(BodyOf(Life).Breath, 0u);
	Life.Person = 0;
	Life.Alive = 1;
	VT_CHECK_EQ(BodyOf(Life).Breath, 0u);
}
