// VAELEN - VaelenView
// Phase 18 task 18.04: the climate of every tile, as a renderer needs it.
//
// A LEAF, like Land.h: numbers, no handle, no way back into the world. Four
// bytes a tile - today's temperature, mid-winter's and mid-summer's in whole
// degrees, and two flags - taken once a day and never on a frame, because
// 65536 tiles at 256 is a quarter of a megabyte and the temperature moves
// once a day. Phase 19 colours the ground by it through the same
// per-instance path the biome takes. TileView (Land.h) has no spare byte,
// which is why this is a leaf of its own and not a field of that one.
//
// STATUS: PROTOTYPE (Phase 18 task 18.04)
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/View/ViewApi.h"

#include <vector>

namespace Vaelen::View
{
	namespace ClimateFlag
	{
		inline constexpr uint8 Frost = 1u << 0;	  ///< today is below freezing here
		inline constexpr uint8 Growing = 1u << 1; ///< today is at or above the growing line here
	} // namespace ClimateFlag

	/// One tile's climate. Four bytes, and the static_assert keeps it so.
	struct TileClimate
	{
		int8 Now = 0;	 ///< today, whole degrees, clamped to the byte
		int8 Winter = 0; ///< mid-winter's (day 315)
		int8 Summer = 0; ///< mid-summer's (day 135)
		uint8 Flags = 0; ///< ClimateFlag bits
	};
	static_assert(sizeof(TileClimate) == 4, "TileClimate must be four bytes: MeasureClimateView hashes it");

	/// The climate of one world on one day, row-major, Width * Height tiles -
	/// the same order as MapView and WorldGrid. Empty when the sources carry no
	/// climate: then Season is 0 and Tiles has nothing in it.
	struct ClimateView
	{
		uint64 Tick = 0;
		uint32 Year = 0;
		uint32 Day = 0;	   ///< of the year, 0-based
		uint32 Season = 0; ///< 0 no climate, 1 spring, 2 summer, 3 autumn, 4 winter
		uint32 Width = 0;
		uint32 Height = 0;
		uint32 Reserved[3] = {};
		std::vector<TileClimate> Tiles;
	};

	/// The tile at (X, Y), or nullptr when it is off the view.
	VAELEN_VIEW_API const TileClimate* TileClimateIn(const ClimateView& V, uint32 X, uint32 Y);

	struct ClimateViewStats
	{
		uint32 Tiles = 0;	///< in the view
		uint32 Frost = 0;	///< of them, below freezing today
		uint32 Growing = 0; ///< of them, at or above the growing line today
		int32 Coldest = 0;	///< the coldest tile today
		int32 Warmest = 0;	///< the warmest
		uint32 Bytes = 0;	///< what the climate weighs
		uint32 Reserved = 0;
		Hash64 Digest = 0; ///< the header fields, then every tile in index order
	};
	VAELEN_VIEW_API ClimateViewStats MeasureClimateView(const ClimateView& V);
} // namespace Vaelen::View
