// VAELEN - VaelenScene
// Phase 19 task 19.09: the cold and the heat can be seen - snow, grass, sun, breath.
//
// « froid chaud et tout doivent s'afficher ». Until 19.09 only the page's text
// row said the weather; the tile climate leaf built for the ground (View/
// Climate.h) had no reader that draws. This turns the leaf and the played life
// into what the scene paints, in integers, once a day:
//   - SNOW on a tile that is below freezing TODAY (the Frost flag, never the
//     winter's figure): full at -5 deg and below, thinner towards the line
//     (91 at -1, 51 at a fraction below zero the byte rounds to 0), never
//     none while it freezes, and never on the sea;
//   - GRASS greener where a LAND tile is above the growing line today (a
//     lake bed or a river bed carries no colour to green);
//   - the SUN from the world's hours - how many of the day's waking hours are
//     spent - and its day of the year, at the played region's row: never a
//     frame clock (ADR-0138: the world has no clock inside a day);
//   - BREATH when the played person's air is below zero, a SHIVER from their chill.
//
// The world's seasons are the same on every row (mid-summer is day 135
// everywhere, Sim/Climate.h), so the sun's declination is too: it is measured
// from each row's distance to the equator, not its signed latitude.
//
// STATUS: VALIDATED headless (Phase 19 task 19.09)
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Scene/SceneApi.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/View/Climate.h"
#include "Vaelen/View/Life.h"

namespace Vaelen::Scene
{
	inline constexpr int32 FullSnowDegrees = -5;

	/// 0..255: how white a tile is today. Zero unless the tile's Frost flag is set.
	VAELEN_SCENE_API uint8 SnowOf(const View::TileClimate& T);
	/// 0..255: how much greener a tile is today. Zero unless its Growing flag is set.
	VAELEN_SCENE_API uint8 GrassOf(const View::TileClimate& T);

	/// Repaints a built mesh with the day's snow and grass, in place, from the
	/// tile each vertex takes its colour from. A view with no tiles (a world
	/// without a climate) leaves the mesh exactly as it was.
	VAELEN_SCENE_API void ApplyClimate(const Ground& G, const View::ClimateView& Climate, TerrainMesh& Mesh);

	/// Tenths of a degree. Azimuth 900 is east, 1800 south, 2700 west.
	struct Sun
	{
		int32 Azimuth = 0;
		int32 Elevation = 0; ///< above the horizon; <= 0 before dawn and after dusk
	};
	/// The sun over row Row of a map Height rows high, on the life's day of the
	/// year, Spent of Awake waking hours into it. Awake 0 is night: elevation 0.
	/// Held to its domain: a row past the map is its last row, a day past the
	/// year its day of the year, Spent past Awake is Awake.
	VAELEN_SCENE_API Sun SunOf(uint32 Row, uint32 Height, uint32 DayOfYear, uint32 Spent, uint32 Awake);

	struct BodyWeather
	{
		uint8 Breath = 0; ///< 1 when the air at the played region is below zero
		uint8 Shiver = 0; ///< 0..255 from the played person's chill
		uint16 Reserved = 0;
	};
	VAELEN_SCENE_API BodyWeather BodyOf(const View::LifeView& Life);

	struct SkyStats
	{
		uint32 Tiles = 0;
		uint32 Snow = 0;	///< tiles with any snow today
		uint32 Growing = 0; ///< tiles greener today
		int32 Azimuth = 0;
		int32 Elevation = 0;
		uint32 Breath = 0;
		uint32 Shiver = 0;
		uint32 Reserved = 0;
		Hash64 Digest = 0;
	};
	/// The day's sky: snow on every tile but the sea, grass on the land alone
	/// (the same rule ApplyClimate paints by), the sun on the climate's day at
	/// CentroidRow, Life.Spent of Life.Awake hours into it, and the played
	/// body's weather. A view that does not fit the ground counts nothing:
	/// Tiles = 0, as ApplyClimate paints nothing.
	VAELEN_SCENE_API SkyStats MeasureSky(const Ground& G, const View::ClimateView& Climate, const View::LifeView& Life,
										 uint32 CentroidRow);

	/// `LogVaelenScene: AELVOR <size> seed <12 hex> sky day D: snow S of T tiles,
	/// growing G, sun azimuth A elevation E, breath B shiver H; sky <16 hex>`.
	/// Day is said as the climate line says it: of the year, counted from 1.
	inline constexpr uint32 SkyLineBytes = 256;
	VAELEN_SCENE_API uint32 SkyLine(uint32 Size, uint64 Seed, uint32 Day, const SkyStats& S, char* Out, uint32 Bytes);
} // namespace Vaelen::Scene
