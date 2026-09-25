// VAELEN - VaelenUI
// Phase 14 task 14.09: what names the HUD and the controller.
//
// No asset, no Blueprint, no UMG: the mode is the one place the two classes
// of the first screen are said out loud, and DefaultEngine.ini's
// GlobalDefaultGameMode names this - one line of config the parse cannot see.
//
// STATUS: VALIDATED (Phase 14) - built by UnrealBuildTool and RUN on
// 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor): eighty-three days
// played at the keyboard, and Tools/Atlas replayed the stream headlessly to the
// same four digests, byte for byte. Tests/Run/Streams/README.md has the lines.
// BUILD: b0921 - Tools/engine_builds.txt; its code is what that build compiled (19.01).
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
