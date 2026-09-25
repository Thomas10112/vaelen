// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Components/SkyAtmosphereComponent.h.
#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"

class USkyAtmosphereComponent : public USceneComponent
{
};
