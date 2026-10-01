// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Components/ExponentialHeightFogComponent.h.
#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"

class UExponentialHeightFogComponent : public USceneComponent
{
public:
	void SetFogDensity(float Value);
	// 23.03: the air lit by the sun it stands under (BELIEFS until a sitting builds them).
	void SetFogHeightFalloff(float Value);
	void SetFogInscatteringColor(FLinearColor Value);
	void SetFogMaxOpacity(float Value);
	void SetStartDistance(float Value);
	void SetDirectionalInscatteringExponent(float Value);
	void SetDirectionalInscatteringColor(FLinearColor Value);
	void SetVolumetricFog(bool bNewValue);
};
