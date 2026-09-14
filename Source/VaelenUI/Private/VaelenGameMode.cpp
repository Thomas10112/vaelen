// VAELEN - VaelenUI
// Phase 14 task 14.09: what names the HUD and the controller.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, not yet built by UnrealBuildTool
#include "VaelenGameMode.h"

#include "VaelenHUD.h"
#include "VaelenPlayerController.h"

AVaelenGameMode::AVaelenGameMode()
{
	HUDClass = AVaelenHUD::StaticClass();
	PlayerControllerClass = AVaelenPlayerController::StaticClass();
}
