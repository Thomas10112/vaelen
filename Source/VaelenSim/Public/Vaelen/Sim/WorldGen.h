// VAELEN - VaelenSim
// World generation pipeline: the layers every stage reads and writes, the
// stage functions, their parameters, measurements and the ASCII export.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// Every stage is a pure function of (seed, config, previous layers): it takes
// its own derived stream name, writes only its own layers, and the tests freeze
// its digest. Stages present: 02.03 elevation and coastline. Later stages
// (climate, hydrology, regions, deposits) extend WorldLayers and the parameter
// indices without changing WorldGenConfig's layout.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/FixedPoint.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/WorldMap.h"

#include <string>

namespace Vaelen::WorldGen
{
	/// Terrain flags per tile (layer "terrain").
	namespace TerrainFlag
	{
		inline constexpr uint8 Land = 1u << 0;	 ///< elevation above sea level
		inline constexpr uint8 Coast = 1u << 1;	 ///< land with a sea tile among its 4 neighbours
		inline constexpr uint8 Shore = 1u << 2;	 ///< sea with a land tile among its 4 neighbours
		inline constexpr uint8 Border = 1u << 3; ///< on the grid edge
	} // namespace TerrainFlag

	/// The layer set of Phase 02, declared once by setup code (the same
	/// function on every side of a snapshot).
	struct WorldLayers
	{
		TileLayerId<int64> Elevation; ///< Fix64 raw, 0 = sea level by default
		TileLayerId<uint8> Terrain;	  ///< TerrainFlag bits
		TileLayerId<int64> Slope;	  ///< Fix64 raw, max |dz| over the 8 neighbours

		static WorldLayers Declare(WorldMap& Map);
	};

	/// Indices into WorldGenConfig::Params for the elevation stage. Values are
	/// Fix64 raw unless stated; zero selects the default in ElevationParams.
	namespace ParamIndex
	{
		inline constexpr uint32 ContinentFrequency = 0; ///< lattice cells across the map width
		inline constexpr uint32 ContinentWarp = 1;		///< domain warp strength
		inline constexpr uint32 ReliefFrequency = 2;	///< lattice cells across the map width
		inline constexpr uint32 ReliefAmplitude = 3;	///< elevation units
		inline constexpr uint32 RidgeAmplitude = 4;		///< elevation units
		inline constexpr uint32 ContinentAmplitude = 5; ///< elevation units
		inline constexpr uint32 EdgeFalloff = 6;		///< fraction of the half-size where the sea begins
		inline constexpr uint32 ContinentBias = 7;		///< added to the mask (positive = more land)
		inline constexpr uint32 ElevationOctaves = 8;	///< integer
	} // namespace ParamIndex

	/// Resolved parameters of the elevation stage (defaults where the config says 0).
	struct ElevationParams
	{
		Fix64 ContinentFrequency = Fix64::FromInt(3);
		Fix64 ContinentWarp = Fix64::FromRatio(3, 4);
		Fix64 ReliefFrequency = Fix64::FromInt(24);
		Fix64 ReliefAmplitude = Fix64::FromInt(900);
		Fix64 RidgeAmplitude = Fix64::FromInt(1800);
		Fix64 ContinentAmplitude = Fix64::FromInt(2400);
		Fix64 EdgeFalloff = Fix64::FromRatio(7, 10);
		Fix64 ContinentBias = Fix64::FromRatio(1, 10);
		uint32 Octaves = 6;

		static ElevationParams Resolve(const WorldGenConfig& Config) noexcept;
	};

	/// Stage 02.03: fills Elevation, then Terrain flags and Slope from it.
	/// Requires Map.IsReady(). Returns false (with a report) otherwise.
	VAELEN_SIM_API bool GenerateElevation(WorldMap& Map, const WorldLayers& Layers, uint64 Seed);

	/// Recomputes Terrain flags and Slope from the Elevation layer and the
	/// config's sea level (used by GenerateElevation; public for tests and
	/// later stages that edit elevation, such as depression filling).
	VAELEN_SIM_API void ClassifyTerrain(WorldMap& Map, const WorldLayers& Layers);

	struct ElevationStats
	{
		uint32 LandTiles = 0;
		uint32 SeaTiles = 0;
		uint32 CoastTiles = 0;
		uint32 BorderLandTiles = 0;
		uint32 LargestLandmassTiles = 0;
		uint32 LandmassCount = 0;
		Fix64 MinElevation;
		Fix64 MaxElevation;
		Fix64 MaxSlope;
	};
	VAELEN_SIM_API ElevationStats MeasureElevation(const WorldMap& Map, const WorldLayers& Layers);

	/// Digest of one layer's bytes (name-seeded), for frozen-value tests.
	VAELEN_SIM_API Hash64 LayerDigest(const WorldMap& Map, uint32 LayerIndex);

	/// Downsampled ASCII picture (Columns wide, aspect-corrected rows):
	/// '~' deep sea, '-' shallow sea, '.' lowland, ':' upland, '^' hills, 'A' mountains.
	VAELEN_SIM_API void ExportAscii(const WorldMap& Map, const WorldLayers& Layers, uint32 Columns, std::string& Out);
} // namespace Vaelen::WorldGen
