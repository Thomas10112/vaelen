// VAELEN - VaelenSim
// World generation stage 02.06: regions - a partition of the land into
// contiguous regions that later phases (settlement, polities, routes) address
// as entities, plus a derived adjacency graph.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// Seeds sit on a jittered lattice over the land (every landmass gets at least
// one); regions grow from the seeds by a multi-source least-cost search whose
// cost rises with elevation change and river crossings, so ridges and rivers
// become borders; regions below the size floor merge into the neighbour they
// share the longest border with. Every step orders ties by tile index.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/FixedPoint.h"
#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/WorldGen.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::WorldGen
{
	/// Component of a region entity (ids of kind Region). Plain data, no padding.
	struct RegionInfo
	{
		uint32 Index = 0; ///< value in the region-index layer (1-based)
		uint32 Tiles = 0;
		uint32 SeedTile = 0;
		uint32 CentroidTile = 0; ///< region tile nearest to the mean position
		uint32 CoastTiles = 0;
		uint32 RiverTiles = 0;
		uint32 LakeTiles = 0;
		uint32 DominantBiome = 0; ///< Biome value with the most tiles
		int64 MeanElevation = 0;  ///< Fix64 raw
	};

	struct RegionTypes
	{
		ComponentType<RegionInfo> Region;

		static RegionTypes Declare(World& W);
	};

	struct RegionLayers
	{
		TileLayerId<uint16> RegionIndex; ///< 0 on sea, else RegionInfo::Index

		static RegionLayers Declare(WorldMap& Map);
	};

	namespace ParamIndex
	{
		// 02.06 regions
		inline constexpr uint32 RegionSpacing = 23;	  ///< seed spacing as a fraction of the map width
		inline constexpr uint32 RegionMinTiles = 24;  ///< integer floor; smaller regions merge
		inline constexpr uint32 RegionSlopeCost = 25; ///< cost per 1000 units of elevation change
		inline constexpr uint32 RegionRiverCost = 26; ///< cost of stepping onto a river tile
	} // namespace ParamIndex

	struct RegionParams
	{
		Fix64 Spacing = Fix64::FromRatio(1, 12);
		uint32 MinTiles = 24;
		Fix64 SlopeCost = Fix64::FromInt(8);
		Fix64 RiverCost = Fix64::FromInt(6);

		static RegionParams Resolve(const WorldGenConfig& Config) noexcept;
	};

	/// Stage 02.06. Requires elevation, hydrology and climate. Destroys the
	/// region entities of a previous run first.
	VAELEN_SIM_API bool GenerateRegions(World& W, const WorldLayers& Layers, const HydroLayers& Hydro,
										const RegionLayers& Regions, const RegionTypes& Types);

	/// Derived adjacency: Neighbours[Index] lists the neighbouring region
	/// indices (1-based) in increasing order; entry 0 is unused.
	struct RegionGraph
	{
		std::vector<std::vector<uint16>> Neighbours;
		std::vector<std::vector<uint32>> SharedBorder; ///< parallel to Neighbours: shared 4-edges

		uint32 RegionCount() const noexcept
		{
			return Neighbours.empty() ? 0u : static_cast<uint32>(Neighbours.size() - 1);
		}
		bool AreAdjacent(uint16 A, uint16 B) const noexcept;
	};
	VAELEN_SIM_API RegionGraph BuildRegionGraph(const WorldMap& Map, const RegionLayers& Regions);

	struct RegionStats
	{
		uint32 Regions = 0;
		uint32 SmallestTiles = 0;
		uint32 LargestTiles = 0;
		uint32 UnassignedLand = 0;
		uint32 AssignedSea = 0;
		uint32 MaxNeighbours = 0;
	};
	VAELEN_SIM_API RegionStats MeasureRegions(const World& W, const WorldLayers& Layers, const RegionLayers& Regions,
											  const RegionTypes& Types);

	/// Region picture: sea '~', regions cycle through letters and digits by index.
	VAELEN_SIM_API void ExportRegionAscii(const WorldMap& Map, const RegionLayers& Regions, uint32 Columns,
										  std::string& Out);
} // namespace Vaelen::WorldGen
