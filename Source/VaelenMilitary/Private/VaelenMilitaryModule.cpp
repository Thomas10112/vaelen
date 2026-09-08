// VAELEN - VaelenMilitary
// The ONLY Unreal-facing translation unit of the military module.
// Excluded from the headless CMake build (see Source/VaelenMilitary/CMakeLists.txt).
//
// STATUS: UNVERIFIED - engine-side, newer than the first Unreal build.
#include "Modules/ModuleManager.h"

class FVaelenMilitaryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenMilitaryModule, VaelenMilitary)
