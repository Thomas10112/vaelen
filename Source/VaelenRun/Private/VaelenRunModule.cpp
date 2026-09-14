// VAELEN - VaelenRun
// The ONLY Unreal-facing translation unit of the run module.
// Excluded from the headless CMake build (see Source/VaelenRun/CMakeLists.txt).
//
// STATUS: UNVERIFIED (engine-only) - not yet compiled by UnrealBuildTool; the
// first build is 14.08's, on the machine that has the engine.
#include "Modules/ModuleManager.h"

class FVaelenRunModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenRunModule, VaelenRun)
