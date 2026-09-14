// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

struct FFileHelper
{
	static bool SaveStringToFile(const FString& Text, const TCHAR* Path);
	static bool LoadFileToString(FString& Out, const TCHAR* Path);
};
