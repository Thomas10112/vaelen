// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

struct FPaths
{
	static FString ProjectSavedDir();
	/// 23.07 BELIEF: Saved/Screenshots/<platform>/, where Vaelen.Shot writes.
	static FString ScreenShotDir();
	static FString Combine(const FString& First, const FString& Second);
	// 16.14: the leaf of a path, for a listing that must not care whether
	// FindFiles handed back names or paths.
	static FString GetCleanFilename(const FString& Path);
};
