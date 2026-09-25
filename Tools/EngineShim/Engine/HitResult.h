// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Engine/HitResult.h.
#pragma once

#include "CoreMinimal.h"

struct FHitResult
{
	FVector Location;
	FVector ImpactPoint;
	FVector ImpactNormal;
	float Distance = 0.0f;
	uint8 bBlockingHit : 1;
};
