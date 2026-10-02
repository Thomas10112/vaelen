// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 23.07 (2026-10-02), a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's UnrealClient.h - the screenshot request Vaelen.Shot makes.
#pragma once
#include "CoreMinimal.h"

struct FScreenshotRequest
{
	static void RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddFilenameSuffix);
};
