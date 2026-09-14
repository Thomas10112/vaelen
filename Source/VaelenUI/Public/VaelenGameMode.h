// VAELEN - VaelenUI
// Phase 14 task 14.09: what names the HUD and the controller.
//
// No asset, no Blueprint, no UMG: the mode is the one place the two classes
// of the first screen are said out loud, and DefaultEngine.ini's
// GlobalDefaultGameMode names this - one line of config the parse cannot see.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, not yet built by UnrealBuildTool
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "VaelenGameMode.generated.h"

UCLASS()
class VAELENUI_API AVaelenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaelenGameMode();
};
