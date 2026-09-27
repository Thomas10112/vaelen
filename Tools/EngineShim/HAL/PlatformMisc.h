// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 22.01 BELIEF: stands for UE 5.6's HAL/PlatformMisc.h - the one verb the
// front end's Quit needs. FPlatformMisc is a platform typedef in the engine;
// one struct of that name is what a parse needs.
#pragma once

#include "CoreMinimal.h"

struct FPlatformMisc
{
	static void RequestExit(bool Force);
};
