// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's GameFramework/CharacterMovementComponent.h.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

/// An actor component, not a scene component: it has no transform of its own.
class UCharacterMovementComponent : public UObject
{
public:
	float MaxWalkSpeed = 600.0f;
	float JumpZVelocity = 420.0f;
	FRotator RotationRate;
	uint8 bOrientRotationToMovement : 1;

	/// Degrees. The engine keeps the angle and its cosine together, which is
	/// why it is a setter and not a field.
	void SetWalkableFloorAngle(float InWalkableFloorAngle);
	float GetWalkableFloorAngle() const;
};
