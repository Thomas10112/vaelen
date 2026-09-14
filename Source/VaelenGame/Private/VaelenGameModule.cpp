// VAELEN - VaelenGame
// The module's Unreal entry point. Phase 14 task 14.08.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, not yet built by UnrealBuildTool
#include "Modules/ModuleManager.h"

class FVaelenGameModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	/// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenGameModule, VaelenGame)
