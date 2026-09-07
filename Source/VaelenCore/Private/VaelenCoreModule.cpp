// VAELEN - VaelenCore
// The ONLY Unreal-facing translation unit of the kernel module.
// Excluded from the headless CMake build (see Source/VaelenCore/CMakeLists.txt).
//
// STATUS: VALIDATED (UE 5.6, 2026-09-07) - compiled and run in the editor; not covered by the headless CI.
#include "Modules/ModuleManager.h"

class FVaelenCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenCoreModule, VaelenCore)
