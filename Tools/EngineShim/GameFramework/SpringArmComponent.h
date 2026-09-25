// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's GameFramework/SpringArmComponent.h.
#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"

class USpringArmComponent : public USceneComponent
{
public:
	float TargetArmLength = 300.0f;
	FVector SocketOffset;
	uint32 bUsePawnControlRotation : 1;
	uint32 bDoCollisionTest : 1;

	static const FName SocketName;
};
