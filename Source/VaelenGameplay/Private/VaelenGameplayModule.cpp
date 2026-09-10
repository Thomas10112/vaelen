// VAELEN - VaelenGameplay
// The ONLY Unreal-facing translation unit of the colony module.
// Excluded from the headless CMake build (see Source/VaelenGameplay/CMakeLists.txt).
//
// STATUS: VALIDATED (UE 5.6, 2026-09-10) - compiled and linked by UnrealBuildTool in 13.06;
// not run in the editor, and not covered by the headless CI.
#include "Modules/ModuleManager.h"

class FVaelenGameplayModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenGameplayModule, VaelenGameplay)
