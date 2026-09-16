// VAELEN - VaelenUI
// Phase 14 task 14.09: what names the HUD and the controller.
//
// STATUS: BUILT (Phase 14) - compiled and linked by UnrealBuildTool 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor); NOT yet run, so nothing here is VALIDATED until 14.10's lines come out of a log
#include "VaelenGameMode.h"

#include "VaelenHUD.h"
#include "VaelenPlayerController.h"

AVaelenGameMode::AVaelenGameMode()
{
	HUDClass = AVaelenHUD::StaticClass();
	PlayerControllerClass = AVaelenPlayerController::StaticClass();
}
