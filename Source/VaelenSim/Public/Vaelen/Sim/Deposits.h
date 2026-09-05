// VAELEN - VaelenSim
// World generation stage 02.07: resource deposits - placed from biome,
// elevation, slope and hydrology with a hashed draw, spaced by cells,
// tiered by rarity, one entity each. Nothing is placed by hand.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/FixedPoint.h"
#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/WorldGen.h"

namespace Vaelen
{
	class World;
}

namespace Vaelen::WorldGen
{
	enum class ResourceKind : uint8
	{
		Stone = 0,
		Timber,
		Clay,
		FertileSoil,
		Salt,
		IronOre,
		CopperOre,
		Gold,
		Count
	};
	VAELEN_SIM_API const char* ResourceKindName(ResourceKind K) noexcept;

	/// Rarity tier: 1 common, 2 uncommon, 3 rare.
	inline constexpr uint32 RarityTiers = 3;

	/// Component of a deposit entity (ids of kind ResourceDeposit). Plain data.
	struct DepositInfo
	{
		uint32 Index = 0; ///< value in the deposit-index layer (1-based)
		uint32 Tile = 0;
		uint32 Kind = 0;	 ///< ResourceKind
		uint32 Tier = 0;	 ///< 1..RarityTiers
		uint32 Richness = 0; ///< 1..1000
		uint32 Region = 0;	 ///< region index of the tile (0 when regions were not generated)
		uint32 Reserved0 = 0;
		uint32 Reserved1 = 0;
	};

	struct DepositTypes
	{
		ComponentType<DepositInfo> Deposit;

		static DepositTypes Declare(World& W);
	};

	struct DepositLayers
	{
		TileLayerId<uint16> DepositIndex; ///< 0 none, else DepositInfo::Index

		static DepositLayers Declare(WorldMap& Map);
	};

	namespace ParamIndex
	{
		// 02.07 deposits
		inline constexpr uint32 DepositDensity = 27; ///< draw probability scale, Fix64 in (0, 1]
		inline constexpr uint32 DepositSpacing =
			28; ///< cell size in tiles for the one-per-kind-per-cell rule (integer)
	} // namespace ParamIndex

	struct DepositParams
	{
		Fix64 Density = Fix64::FromRatio(1, 1);
		uint32 Spacing = 6;

		static DepositParams Resolve(const WorldGenConfig& Config) noexcept;
	};

	/// Suitability of a tile for a kind, 0 (impossible) to 1000, from the
	/// layers; pure and public so tests can check the rules.
	VAELEN_SIM_API uint32 DepositSuitability(ResourceKind Kind, bool Land, bool Coast, Biome B, Fix64 ElevationAboveSea,
											 Fix64 Slope, bool River, bool LakeAdjacent, bool RiverAdjacent) noexcept;

	/// Stage 02.07. Requires elevation, hydrology, climate; uses regions when
	/// generated (RegionLayers may point to an all-zero layer). Destroys the
	/// deposit entities of a previous run first.
	VAELEN_SIM_API bool GenerateDeposits(World& W, const WorldLayers& Layers, const HydroLayers& Hydro,
										 const RegionLayers& Regions, const DepositLayers& Deposits,
										 const DepositTypes& Types);

	struct DepositStats
	{
		uint32 Total = 0;
		uint32 ByKind[static_cast<uint32>(ResourceKind::Count)] = {};
		uint32 ByTier[RarityTiers + 1] = {};
		uint32 OnSea = 0;
		uint32 RegionsWithDeposits = 0;
	};
	VAELEN_SIM_API DepositStats MeasureDeposits(const World& W, const WorldLayers& Layers,
												const DepositLayers& Deposits, const DepositTypes& Types);
} // namespace Vaelen::WorldGen
