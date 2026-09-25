// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Components/DirectionalLightComponent.h.
#pragma once

#include "Components/LightComponent.h"
#include "CoreMinimal.h"

/// The sun. Its direction is the component's rotation, set from the world's
/// hours (19.09), never from a frame clock.
class UDirectionalLightComponent : public ULightComponent
{
public:
	void SetAtmosphereSunLight(bool bNewValue);
};
