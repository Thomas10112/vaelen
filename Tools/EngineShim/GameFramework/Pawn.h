// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's GameFramework/Pawn.h.
#pragma once

#include "Components/InputComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class AController;

class APawn : public AActor
{
public:
	uint32 bUseControllerRotationPitch : 1;
	uint32 bUseControllerRotationYaw : 1;
	uint32 bUseControllerRotationRoll : 1;

	AController* GetController() const;
	FRotator GetControlRotation() const;
	/// The engine's input to its movement component: a WORLD direction and a
	/// scale, consumed on the component's tick - which is the engine's and not
	/// ours (ADR-0157).
	void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false);
	void AddControllerYawInput(float Val);
	void AddControllerPitchInput(float Val);
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);
};
