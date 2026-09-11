// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
//
// The console command is not decoration: it is how 13.07c was checked under
// -nullrhi when the editor could not be opened, so its registration has to keep
// parsing. CreateStatic takes the address of a function whose parameters must
// match the delegate's, which is a real signature check and worth keeping.
#pragma once

#include "CoreMinimal.h"

class UWorld;

class FConsoleCommandWithWorldAndArgsDelegate
{
public:
	using FnType = void (*)(const TArray<FString>&, UWorld*);
	static FConsoleCommandWithWorldAndArgsDelegate CreateStatic(FnType Fn)
	{
		FConsoleCommandWithWorldAndArgsDelegate Made;
		Made.Bound = Fn;
		return Made;
	}
	FnType Bound = nullptr;
};

class FAutoConsoleCommandWithWorldAndArgs
{
public:
	FAutoConsoleCommandWithWorldAndArgs(const TCHAR* Name, const TCHAR* Help,
										const FConsoleCommandWithWorldAndArgsDelegate& Command)
	{
	}
};
