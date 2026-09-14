// VAELEN - VaelenUI
// The module's Unreal entry point. Phase 14 task 14.09.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, not yet built by UnrealBuildTool
#include "Modules/ModuleManager.h"

class FVaelenUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	/// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenUIModule, VaelenUI)
