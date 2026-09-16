// VAELEN - VaelenRun
// The ONLY Unreal-facing translation unit of the run module.
// Excluded from the headless CMake build (see Source/VaelenRun/CMakeLists.txt).
//
// STATUS: BUILT (engine-only) - compiled and linked by UnrealBuildTool on
// 2026-09-16 with 14.08's first build, on the machine that has the engine.
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
