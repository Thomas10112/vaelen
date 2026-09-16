// VAELEN - VaelenUI
// Phase 14 task 14.09: what names the HUD and the controller.
//
// No asset, no Blueprint, no UMG: the mode is the one place the two classes
// of the first screen are said out loud, and DefaultEngine.ini's
// GlobalDefaultGameMode names this - one line of config the parse cannot see.
//
// STATUS: BUILT (Phase 14) - compiled and linked by UnrealBuildTool 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor); NOT yet run, so nothing here is VALIDATED until 14.10's lines come out of a log
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
