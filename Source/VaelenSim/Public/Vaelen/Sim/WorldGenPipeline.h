// VAELEN - VaelenSim
// World generation stage 02.08: the whole pipeline in one call, with the
// setup every side of a snapshot must run identically.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// Order: Reset -> elevation -> hydrology (edits elevation, reclassifies) ->
// climate -> regions -> deposits. Each stage is pure in (seed, config, earlier
// layers); the pipeline's result is a function of the seed and the config
// alone, frozen at three sizes by Tests/Sim/Test_WorldPipeline.cpp.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/Deposits.h"
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
	/// Every layer and component type of Phase 02, declared in a fixed order.
	struct WorldSetup
	{
		WorldLayers Layers;
		HydroLayers Hydro;
		RegionLayers Regions;
		DepositLayers Deposits;
		WorldTypes Types;
		RegionTypes RegionTypes_;
		DepositTypes DepositTypes_;

		/// Declares layers and component types; call once per World before Build().
		static WorldSetup Declare(World& W);
	};

	/// Stages a pipeline run can stop after (for tests and tools).
	enum class WorldGenStage : uint8
	{
		Elevation = 0,
		Hydrology,
		Climate,
		Regions,
		Deposits,
		Count
	};
	VAELEN_SIM_API const char* WorldGenStageName(WorldGenStage S) noexcept;

	/// Runs the pipeline up to and including Last. Returns false (with a
	/// report) on an invalid config or a failed stage; the world's map is then
	/// unspecified. Re-running replaces the generated entities.
	VAELEN_SIM_API bool GenerateWorld(World& W, const WorldSetup& Setup, const WorldGenConfig& Config,
									  WorldGenStage Last = WorldGenStage::Deposits);

	struct WorldGenReport
	{
		ElevationStats Elevation;
		ClimateStats Climate;
		HydrologyStats Hydrology;
		RegionStats Regions;
		DepositStats Deposits;
		uint32 Entities = 0;
	};
	VAELEN_SIM_API WorldGenReport ReportWorld(const World& W, const WorldSetup& Setup);
} // namespace Vaelen::WorldGen
