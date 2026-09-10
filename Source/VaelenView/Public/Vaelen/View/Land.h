// VAELEN - VaelenView
// Phase 13 task 13.07a: the ground itself, as a renderer needs it.
//
// 13.01 gave the view a frame: regions, people, roads, borders. Drawing that
// alone puts sixty-odd coloured cells on the screen and calls them a continent.
// The one image AELVOR has ever produced was not drawn from a view at all - the
// atlas actor read `Map.GetLayer(Layers.Biome)` straight out of the world,
// tile by tile, because that is where the coastline lives and the view had no
// coastline in it.
//
// That is the finding this file answers, and it is worth stating plainly:
// PRESENTATION cannot be made to stop reading the world until the view carries
// what presentation was reading the world FOR. A rule with nothing behind it is
// just a rule everybody breaks.
//
// So: the same promise as 13.01, one level down. A `MapView` is a flat block of
// numbers - no pointer, no handle, no layer id, nothing that reaches back - and
// a renderer holding one can draw the ground and nothing else.
//
// WHAT IT COSTS, because this one is not free. A `WorldView` is sixty regions
// and weighs a few kilobytes; a `MapView` is every tile, and at 256 x 256 that
// is 65536 tiles at eight bytes = 512 KB. It is NOT a per-frame structure. The
// ground changes when the world is generated and (later) when a river moves or
// a coast erodes - take it then, keep it, and take a `WorldView` for the things
// that change every year.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/deterministic tests in Tests/View/Test_Land.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/ViewApi.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::View
{
	/// What a tile is, as far as anything drawing it is concerned. The first
	/// four bits carry the same meaning as WorldGen::TerrainFlag and Land.cpp
	/// asserts that they carry the same VALUES, so the two cannot drift apart
	/// silently; the last two are hydrology folded in, because a renderer wants
	/// one byte per tile and not three layers to cross-reference.
	namespace GroundFlag
	{
		inline constexpr uint8 Land = 1u << 0;	 ///< above sea level
		inline constexpr uint8 Coast = 1u << 1;	 ///< land with sea among its 4 neighbours
		inline constexpr uint8 Shore = 1u << 2;	 ///< sea with land among its 4 neighbours
		inline constexpr uint8 Border = 1u << 3; ///< on the grid edge
		inline constexpr uint8 River = 1u << 4;	 ///< a river runs through it
		inline constexpr uint8 Lake = 1u << 5;	 ///< it is under a lake
	} // namespace GroundFlag

	/// One tile. Eight bytes, and the static_assert below is what keeps it
	/// eight: MeasureMapView hashes these, so a byte nobody writes is a byte
	/// that decides whether two identical maps compare equal (the lesson
	/// RegionView paid in 13.01, and it is not being paid twice).
	struct TileView
	{
		int32 Elevation = 0; ///< Fix64 raw shifted right 16: units in Q16.16, saturated
		uint16 Region = 0;	 ///< region index, 0 on sea and on unassigned land
		uint8 Biome = 0;	 ///< WorldGen::Biome
		uint8 Ground = 0;	 ///< GroundFlag bits
	};
	static_assert(sizeof(TileView) == sizeof(int32) + sizeof(uint16) + 2 * sizeof(uint8),
				  "TileView must have no padding: MeasureMapView hashes it");

	/// The ground of one world, row-major, Width * Height tiles. Index of a
	/// tile is Y * Width + X - the same order WorldGrid uses, so a tile index
	/// out of the kernel addresses the same tile here.
	struct MapView
	{
		uint64 Tick = 0;			 ///< the frame this was taken at
		uint32 Year = 0;			 //
		uint32 Width = 0;			 //
		uint32 Height = 0;			 //
		uint32 Reserved = 0;		 //
		std::vector<TileView> Tiles; ///< empty when the world has no map yet
	};

	/// Takes the ground. Const world in, numbers out - the same signature and
	/// the same promise as TakeView, and the reason both live in this module.
	/// Out is left empty when the world's map has not been generated.
	VAELEN_VIEW_API void TakeMapView(const World& W, const ViewSources& From, MapView& Out);

	/// The tile at (X, Y), or nullptr when it is off the map.
	VAELEN_VIEW_API const TileView* TileIn(const MapView& V, uint32 X, uint32 Y);

	struct MapStats
	{
		uint32 Tiles = 0;	///< in the view
		uint32 Land = 0;	///< of them, above sea level
		uint32 Coast = 0;	///< of them, land touching the sea
		uint32 Water = 0;	///< of them, carrying a river or under a lake
		uint32 Regions = 0; ///< distinct region indices the ground mentions
		uint32 Bytes = 0;	///< what the ground weighs
		Hash64 Digest = 0;	///< every tile in index order
	};
	VAELEN_VIEW_API MapStats MeasureMapView(const MapView& V);
} // namespace Vaelen::View
