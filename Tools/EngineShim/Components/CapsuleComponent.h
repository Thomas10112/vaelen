// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Components/CapsuleComponent.h.
#pragma once

#include "Components/PrimitiveComponent.h"
#include "CoreMinimal.h"

/// UShapeComponent stands between in the engine.
class UCapsuleComponent : public UPrimitiveComponent
{
public:
	void InitCapsuleSize(float InRadius, float InHalfHeight);
	float GetScaledCapsuleHalfHeight() const;
	float GetScaledCapsuleRadius() const;
};
