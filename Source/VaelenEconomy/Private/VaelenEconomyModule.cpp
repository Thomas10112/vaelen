// VAELEN - VaelenEconomy
// The ONLY Unreal-facing translation unit of the economy module.
// Excluded from the headless CMake build (see Source/VaelenEconomy/CMakeLists.txt).
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
#include "Modules/ModuleManager.h"

class FVaelenEconomyModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenEconomyModule, VaelenEconomy)
