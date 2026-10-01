// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 23.03 (2026-10-01), a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Components/PostProcessComponent.h - the grade of the whole
// screen, as a component so that the sky actor owns it and no second actor is spawned.
#pragma once
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Engine/Scene.h"

class UPostProcessComponent : public USceneComponent
{
public:
	FPostProcessSettings Settings;
	float Priority = 0.0f;
	float BlendWeight = 1.0f;
	uint32 bEnabled : 1;
	uint32 bUnbound : 1;
};
