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
		TileLayerId<int64> Elevation;	 ///< Fix64 raw, 0 = sea level by default
		TileLayerId<uint8> Terrain;		 ///< TerrainFlag bits
		TileLayerId<int64> Slope;		 ///< Fix64 raw, max |dz| over the 8 neighbours
		TileLayerId<uint16> SeaDistance; ///< 4-connected tiles to the nearest sea tile (0 on sea), 02.04
		TileLayerId<int64> Temperature;	 ///< Fix64 raw, annual mean in degrees, 02.04
		TileLayerId<int64> Moisture;	 ///< Fix64 raw in [0, 1], 02.04
		TileLayerId<uint8> Biome;		 ///< Biome enum, 02.04

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
		// 02.04 climate
		inline constexpr uint32 EquatorTemperature = 9; ///< degrees at the equator (map middle row)
		inline constexpr uint32 PoleTemperature = 10;	///< degrees at the top and bottom rows
		inline constexpr uint32 LapseRate = 11;			///< degrees lost per 1000 elevation units
		inline constexpr uint32 TemperatureNoise = 12;	///< degrees of local variation
		inline constexpr uint32 RainDecay = 13;		 ///< e-folding distance of a parcel, as a fraction of the map width
		inline constexpr uint32 OrographicRain = 14; ///< extra fraction per 1000 units of climb
		inline constexpr uint32 SeaRecovery = 15;	 ///< humidity regained per sea tile
		inline constexpr uint32 MoistureNoise = 16;	 ///< fraction of local variation
		inline constexpr uint32 ProximityRange = 17; ///< sea-proximity half-range, as a fraction of the map width
		inline constexpr uint32 ProximityWeight = 18; ///< share of moisture that comes from sea proximity
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

	/// Resolved parameters of the climate stage (defaults where the config says 0).
	struct ClimateParams
	{
		Fix64 EquatorTemperature = Fix64::FromInt(30);
		Fix64 PoleTemperature = Fix64::FromInt(-15);
		Fix64 LapseRate = Fix64::FromRatio(13, 2); ///< 6.5 degrees per 1000 units
		Fix64 TemperatureNoise = Fix64::FromInt(2);
		Fix64 RainDecay = Fix64::FromRatio(1, 4); ///< a parcel keeps 1/e of its humidity after a quarter of the width
		Fix64 OrographicRain = Fix64::FromRatio(1, 2);
		Fix64 SeaRecovery = Fix64::FromRatio(1, 6);
		Fix64 MoistureNoise = Fix64::FromRatio(1, 8);
		Fix64 ProximityRange = Fix64::FromRatio(1, 16);
		Fix64 ProximityWeight = Fix64::FromRatio(3, 10);

		static ClimateParams Resolve(const WorldGenConfig& Config) noexcept;
	};

	enum class Biome : uint8
	{
		Ocean = 0,
		Ice,
		Tundra,
		BorealForest,
		ColdSteppe,
		TemperateForest,
		Grassland,
		Scrubland,
		TropicalForest,
		Savanna,
		Desert,
		Alpine,
		Count
	};
	VAELEN_SIM_API const char* BiomeName(Biome B) noexcept;
	VAELEN_SIM_API char BiomeGlyph(Biome B) noexcept;

	/// Latitude in [-1, 1] of a row: -1 at the top row, 0 at the middle, +1 at the bottom.
	VAELEN_SIM_API Fix64 LatitudeOfRow(const WorldGrid& Grid, uint32 Y) noexcept;

	/// Prevailing wind of a latitude: +1 blows west -> east (westerlies), -1 blows
	/// east -> west (trade and polar easterlies). Bands: |lat| < 1/3 easterly,
	/// < 2/3 westerly, else easterly.
	VAELEN_SIM_API int32 PrevailingWind(Fix64 Latitude) noexcept;

	/// Seasonal offset added to the annual mean: season 0..3 = spring, summer,
	/// autumn, winter; amplitude grows with |latitude| (4 + 16 |lat| degrees).
	VAELEN_SIM_API Fix64 SeasonalOffset(Fix64 Latitude, uint32 Season) noexcept;

	/// Biome from the annual mean temperature (degrees), moisture [0, 1],
	/// elevation above sea level (units) and land flag.
	VAELEN_SIM_API Biome ClassifyBiome(Fix64 Temperature, Fix64 Moisture, Fix64 ElevationAboveSea, bool Land) noexcept;

	/// Stage 02.04: fills SeaDistance, Temperature, Moisture and Biome from the
	/// elevation and terrain layers. Requires a generated elevation.
	VAELEN_SIM_API bool GenerateClimate(WorldMap& Map, const WorldLayers& Layers, uint64 Seed);

	struct ClimateStats
	{
		uint32 BiomeTiles[static_cast<uint32>(Biome::Count)] = {};
		uint32 DistinctLandBiomes = 0;
		Fix64 MinTemperature;
		Fix64 MaxTemperature;
		Fix64 MeanLandMoisture;
		uint16 MaxSeaDistance = 0;
	};
	VAELEN_SIM_API ClimateStats MeasureClimate(const WorldMap& Map, const WorldLayers& Layers);

	/// Downsampled ASCII picture of the biomes (BiomeGlyph per cell, majority glyph).
	VAELEN_SIM_API void ExportBiomeAscii(const WorldMap& Map, const WorldLayers& Layers, uint32 Columns,
										 std::string& Out);

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
