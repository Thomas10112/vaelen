// VAELEN - VaelenGame
// The module's Unreal entry point. Phase 14 task 14.08.
//
// STATUS: VALIDATED (Phase 14) - built by UnrealBuildTool and RUN on
// 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor): eighty-three days
// played at the keyboard, and Tools/Atlas replayed the stream headlessly to the
// same four digests, byte for byte. Tests/Run/Streams/README.md has the lines.
// BUILD: b0921 - Tools/engine_builds.txt; its code is what that build compiled (19.01).
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
