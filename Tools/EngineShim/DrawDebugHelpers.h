// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

class UWorld;

void DrawDebugLine(const UWorld* World, const FVector& Start, const FVector& End, const FColor& Color,
				   bool bPersistent = false, float Lifetime = -1.0f, uint8 DepthPriority = 0, float Thickness = 0.0f);
void DrawDebugBox(const UWorld* World, const FVector& Center, const FVector& Extent, const FColor& Color,
				  bool bPersistent = false, float Lifetime = -1.0f, uint8 DepthPriority = 0, float Thickness = 0.0f);
void DrawDebugPoint(const UWorld* World, const FVector& Position, float Size, const FColor& Color,
					bool bPersistent = false, float Lifetime = -1.0f, uint8 DepthPriority = 0);
void FlushPersistentDebugLines(const UWorld* World);
