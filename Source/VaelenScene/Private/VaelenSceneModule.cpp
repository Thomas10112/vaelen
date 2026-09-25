// VAELEN - VaelenScene
// The ONLY Unreal-facing translation unit of the scene module.
// Excluded from the headless CMake build (see Source/VaelenScene/CMakeLists.txt).
//
// STATUS: UNVERIFIED (engine) - never yet compiled by UnrealBuildTool (sitting S2, 19.06).
#include "Modules/ModuleManager.h"

class FVaelenSceneModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenSceneModule, VaelenScene)
