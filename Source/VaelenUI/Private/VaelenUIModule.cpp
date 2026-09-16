// VAELEN - VaelenUI
// The module's Unreal entry point. Phase 14 task 14.09.
//
// STATUS: BUILT (Phase 14) - compiled and linked by UnrealBuildTool 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor); NOT yet run, so nothing here is VALIDATED until 14.10's lines come out of a log
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
