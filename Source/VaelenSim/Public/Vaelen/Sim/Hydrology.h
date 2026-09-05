// VAELEN - VaelenSim
// World generation stage 02.05: hydrology - depression filling, D8 flow,
// accumulation, rivers and lakes as entities.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// The stage reads the elevation and terrain layers, writes its own layers
// (filled elevation, flow direction, accumulation, river and lake indices) and
// creates one entity per river and per lake with a plain-data component.
// Every step is deterministic: the priority flood breaks ties by tile index,
// the D8 choice by the fixed neighbour order, accumulation runs in
// (filled elevation, index) order, and rivers are traced from their sources
// in scan order.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/FixedPoint.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/WorldGen.h"

#include <string>

namespace Vaelen
{
	class World;
}

namespace Vaelen::WorldGen
{
	/// Flow direction values: 0..7 the neighbour slot (N, NE, E, SE, S, SW, W,
	/// NW), FlowNone for sea tiles and tiles without a lower neighbour (none
	/// exist on land after filling).
	inline constexpr uint8 FlowNone = 8;

	/// Component of a river entity (ids of kind River).
	struct RiverInfo
	{
		uint32 SourceTile = 0;
		uint32 MouthTile = 0; ///< first tile past the river: sea, lake or another river
		uint32 Length = 0;	  ///< river tiles owned by this river
		uint32 MouthFlow = 0; ///< accumulation at the last owned tile
		uint32 Index = 0;	  ///< value in the river-index layer (1-based)
		uint32 Reserved = 0;
	};
	/// Component of a lake entity (ids of kind Lake).
	struct LakeInfo
	{
		int64 Surface = 0;	   ///< Fix64 raw filled level of the lake
		uint32 OutletTile = 0; ///< tile the lake spills into
		uint32 Tiles = 0;
		uint32 Index = 0; ///< value in the lake-index layer (1-based)
		uint32 Reserved = 0;
	};

	/// Component types of the world-generation entities, registered by setup
	/// code (the same function on every side of a snapshot).
	struct WorldTypes
	{
		ComponentType<RiverInfo> River;
		ComponentType<LakeInfo> Lake;

		static WorldTypes Declare(World& W);
	};

	/// Hydrology layers, declared after WorldLayers by the same setup code.
	struct HydroLayers
	{
		TileLayerId<int64> Filled;		  ///< Fix64 raw elevation after depression filling
		TileLayerId<uint8> FlowDirection; ///< neighbour slot or FlowNone
		TileLayerId<uint32> Accumulation; ///< tiles draining through (self included), 0 on sea
		TileLayerId<uint16> RiverIndex;	  ///< 0 none, else RiverInfo::Index
		TileLayerId<uint16> LakeIndex;	  ///< 0 none, else LakeInfo::Index

		static HydroLayers Declare(WorldMap& Map);
	};

	namespace ParamIndex
	{
		// 02.05 hydrology
		inline constexpr uint32 RiverThreshold = 19; ///< accumulation as a fraction of the tile count
		inline constexpr uint32 MinRiverLength = 20; ///< integer: shorter traces are dropped
		inline constexpr uint32 LakeMinDepth = 21;	 ///< Fix64 units: shallower fills are terrain, not lakes
		inline constexpr uint32 LakeMinTiles = 22;	 ///< integer: smaller raised patches are ponds, not lakes
	} // namespace ParamIndex

	struct HydrologyParams
	{
		Fix64 RiverThreshold = Fix64::FromRatio(1, 400);
		uint32 MinRiverLength = 4;
		Fix64 LakeMinDepth = Fix64::FromInt(160); ///< deepest point of a basin, else sediment fill
		uint32 LakeMinTiles = 12;

		static HydrologyParams Resolve(const WorldGenConfig& Config) noexcept;
	};

	/// Stage 02.05. Requires a generated elevation; runs BEFORE the climate
	/// stage because shallow closed basins are filled with sediment (their
	/// elevation rises to the water surface) and ClassifyTerrain is rerun.
	/// Destroys the river and lake entities of a previous run first, so the
	/// stage can be re-run; ids are fresh each time (regeneration is
	/// deterministic only in a fresh world).
	VAELEN_SIM_API bool GenerateHydrology(World& W, const WorldLayers& Layers, const HydroLayers& Hydro,
										  const WorldTypes& Types);

	struct HydrologyStats
	{
		uint32 Rivers = 0;
		uint32 Lakes = 0;
		uint32 RiverTiles = 0;
		uint32 LakeTiles = 0;
		uint32 LongestRiver = 0;
		uint32 LargestLake = 0;
		uint32 MaxAccumulation = 0;
		uint32 RaisedTiles = 0; ///< land tiles whose filled level is above their elevation (lakes included)
	};
	VAELEN_SIM_API HydrologyStats MeasureHydrology(const World& W, const WorldLayers& Layers, const HydroLayers& Hydro,
												   const WorldTypes& Types);

	/// Follows the flow from a tile; returns the number of steps to a sea tile,
	/// or 0xffffffff when the walk exceeds the tile count (a cycle) or stops
	/// on land.
	VAELEN_SIM_API uint32 StepsToSea(const WorldMap& Map, const WorldLayers& Layers, const HydroLayers& Hydro,
									 uint32 Tile);

	/// Elevation picture with rivers ('=') and lakes ('o') overlaid.
	VAELEN_SIM_API void ExportHydroAscii(const WorldMap& Map, const WorldLayers& Layers, const HydroLayers& Hydro,
										 uint32 Columns, std::string& Out);
} // namespace Vaelen::WorldGen
