// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's GameFramework/Character.h.
#pragma once

#include "Components/CapsuleComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

/// The first class of this project that derives from a class which does not
/// derive straight from AActor - the reason 19.02 made Super exact.
class ACharacter : public APawn
{
public:
	UCapsuleComponent* GetCapsuleComponent() const;
	UCharacterMovementComponent* GetCharacterMovement() const;
	virtual void Jump();
	virtual void StopJumping();
};
