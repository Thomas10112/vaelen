// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

struct FPaths
{
	static FString ProjectSavedDir();
	static FString Combine(const FString& First, const FString& Second);
};
