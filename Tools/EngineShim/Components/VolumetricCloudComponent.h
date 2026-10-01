// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 23.03 (2026-10-01), a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Components/VolumetricCloudComponent.h. Drawn with the
// material it is given; shown at Vaelen.Look 2 alone (the T400 pays for it).
#pragma once
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"

class UMaterialInterface;

class UVolumetricCloudComponent : public USceneComponent
{
public:
	void SetMaterial(UMaterialInterface* NewValue);
	void SetLayerBottomAltitude(float NewValue);
	void SetLayerHeight(float NewValue);
};
