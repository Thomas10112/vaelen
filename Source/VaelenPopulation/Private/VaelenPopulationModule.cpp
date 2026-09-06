// VAELEN - VaelenPopulation
// The ONLY Unreal-facing translation unit of the population module.
// Excluded from the headless CMake build (see Source/VaelenPopulation/CMakeLists.txt).
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
#include "Modules/ModuleManager.h"

class FVaelenPopulationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenPopulationModule, VaelenPopulation)
