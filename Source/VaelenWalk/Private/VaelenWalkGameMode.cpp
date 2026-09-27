// VAELEN - VaelenWalk
// Phase 19 task 19.06: what names the walk's three classes. See the header.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#include "VaelenWalkGameMode.h"

#include "VaelenHUD.h"
#include "VaelenWalkController.h"
#include "VaelenWalker.h"

AVaelenWalkGameMode::AVaelenWalkGameMode()
{
	HUDClass = AVaelenHUD::StaticClass();
	PlayerControllerClass = AVaelenWalkController::StaticClass();
	DefaultPawnClass = AVaelenWalker::StaticClass();
}
