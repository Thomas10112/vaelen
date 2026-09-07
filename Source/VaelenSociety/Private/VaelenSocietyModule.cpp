// VAELEN - VaelenSociety
// The ONLY Unreal-facing translation unit of the society module.
// Excluded from the headless CMake build (see Source/VaelenSociety/CMakeLists.txt).
//
// STATUS: VALIDATED (UE 5.6, 2026-09-07) - compiled and run in the editor; not covered by the headless CI.
#include "Modules/ModuleManager.h"

class FVaelenSocietyModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenSocietyModule, VaelenSociety)
