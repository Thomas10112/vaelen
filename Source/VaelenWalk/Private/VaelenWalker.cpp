// VAELEN - VaelenWalk
// Phase 19 task 19.06: the body that walks. See VaelenWalker.h.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#include "VaelenWalker.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AVaelenWalker::AVaelenWalker()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Legs = GetCharacterMovement();
	Legs->MaxWalkSpeed = 500.0f;		 // 5 m/s: a brisk walk, and a tile in fifty seconds
	Legs->SetWalkableFloorAngle(44.76f); // Terrain.h's IsSteep is this angle, and no triangle of the map is steeper
	Legs->bOrientRotationToMovement = true;
	Legs->RotationRate = FRotator(0.0, 540.0, 0.0);

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(GetCapsuleComponent());
	Arm->TargetArmLength = 300.0f;
	Arm->bUsePawnControlRotation = true;
	Arm->bDoCollisionTest = true;
	Arm->SocketOffset = FVector(0.0, 60.0, 40.0);

	Eye = CreateDefaultSubobject<UCameraComponent>(TEXT("Eye"));
	Eye->SetupAttachment(Arm, USpringArmComponent::SocketName);
	Eye->bUsePawnControlRotation = false;
}

float AVaelenWalker::StandingHalfHeight() const
{
	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}
