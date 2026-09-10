// VAELEN - VaelenPresentation
// The module's Unreal entry point. Phase 13 task 13.07c.
//
// STATUS: PROTOTYPE - compiled and linked by UnrealBuildTool on UE 5.6.1 with
// MSVC 14.44 on 2026-09-10 (14 modules, 167 actions, Result: Succeeded), and
// NOT YET RUN. Nothing here has been dropped in a level or looked at, so every
// claim about what it DRAWS remains unmeasured. What compiling proves is only
// that it is the shape of a program.
//
// Unlike the twelve kernel modules, this is not the module's ONLY Unreal-facing
// translation unit; every file here is Unreal-facing. That is the difference
// the whole module exists to draw, and Tools/kernel_modules.txt does not list
// VaelenPresentation for exactly that reason.
#include "Modules/ModuleManager.h"

class FVaelenPresentationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
	// A project-owned module: hot-reloadable like the game module.
	virtual bool IsGameModule() const override { return true; }
};

IMPLEMENT_MODULE(FVaelenPresentationModule, VaelenPresentation)
