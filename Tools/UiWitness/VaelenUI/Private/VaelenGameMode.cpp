// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
#include "VaelenGameMode.h"

#include "VaelenHUD.h"
#include "VaelenPlayerController.h"

AVaelenGameMode::AVaelenGameMode()
{
	HUDClass = AVaelenHUD::StaticClass();
	PlayerControllerClass = AVaelenPlayerController::StaticClass();
}
