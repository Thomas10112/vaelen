// VAELEN - VaelenSim
// World generation pipeline.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_WorldPipeline.cpp
#include "Vaelen/Sim/WorldGenPipeline.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/World.h"

namespace Vaelen::WorldGen
{
	WorldSetup WorldSetup::Declare(World& W)
	{
		WorldSetup S;
		S.Layers = WorldLayers::Declare(W.Map());
		S.Hydro = HydroLayers::Declare(W.Map());
		S.Regions = RegionLayers::Declare(W.Map());
		S.Deposits = DepositLayers::Declare(W.Map());
		S.Types = WorldTypes::Declare(W);
		S.RegionTypes_ = RegionTypes::Declare(W);
		S.DepositTypes_ = DepositTypes::Declare(W);
		return S;
	}

	const char* WorldGenStageName(WorldGenStage S) noexcept
	{
		switch (S)
		{
		case WorldGenStage::Elevation:
			return "Elevation";
		case WorldGenStage::Hydrology:
			return "Hydrology";
		case WorldGenStage::Climate:
			return "Climate";
		case WorldGenStage::Regions:
			return "Regions";
		case WorldGenStage::Deposits:
			return "Deposits";
		case WorldGenStage::Count:
			break;
		}
		return "Unknown";
	}

	bool GenerateWorld(World& W, const WorldSetup& Setup, const WorldGenConfig& Config, WorldGenStage Last)
	{
		if (!Config.IsValid() || Last >= WorldGenStage::Count)
		{
			VAELEN_CHECKF(false, "GenerateWorld: invalid config (%u x %u) or stage", Config.Width, Config.Height);
			return false;
		}
		const uint64 Seed = W.Config().Seed;
		if (!W.Map().Reset(Config) || !GenerateElevation(W.Map(), Setup.Layers, Seed))
		{
			return false;
		}
		if (Last == WorldGenStage::Elevation)
		{
			return true;
		}
		if (!GenerateHydrology(W, Setup.Layers, Setup.Hydro, Setup.Types))
		{
			return false;
		}
		if (Last == WorldGenStage::Hydrology)
		{
			return true;
		}
		if (!GenerateClimate(W.Map(), Setup.Layers, Seed))
		{
			return false;
		}
		if (Last == WorldGenStage::Climate)
		{
			return true;
		}
		if (!GenerateRegions(W, Setup.Layers, Setup.Hydro, Setup.Regions, Setup.RegionTypes_))
		{
			return false;
		}
		if (Last == WorldGenStage::Regions)
		{
			return true;
		}
		return GenerateDeposits(W, Setup.Layers, Setup.Hydro, Setup.Regions, Setup.Deposits, Setup.DepositTypes_);
	}

	WorldGenReport ReportWorld(const World& W, const WorldSetup& Setup)
	{
		WorldGenReport R;
		R.Elevation = MeasureElevation(W.Map(), Setup.Layers);
		R.Climate = MeasureClimate(W.Map(), Setup.Layers);
		R.Hydrology = MeasureHydrology(W, Setup.Layers, Setup.Hydro, Setup.Types);
		R.Regions = MeasureRegions(W, Setup.Layers, Setup.Regions, Setup.RegionTypes_);
		R.Deposits = MeasureDeposits(W, Setup.Layers, Setup.Deposits, Setup.DepositTypes_);
		R.Entities = W.Entities().GetAliveCount();
		return R;
	}
} // namespace Vaelen::WorldGen
