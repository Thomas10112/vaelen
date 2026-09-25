// VAELEN - Tools/ShimProbe (19.02). Not built; parsed. README.md says why.
#include "ShimProbeWalker.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AShimProbeWalker::AShimProbeWalker()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Legs = GetCharacterMovement();
	Legs->MaxWalkSpeed = 500.0f;
	Legs->SetWalkableFloorAngle(44.76f);
	Legs->bOrientRotationToMovement = true;
	Legs->RotationRate = FRotator(0.0, 540.0, 0.0);

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(GetCapsuleComponent());
	Arm->TargetArmLength = 300.0f;
	Arm->bUsePawnControlRotation = true;
	Arm->SocketOffset = FVector(0.0, 60.0, 40.0);

	Eye = CreateDefaultSubobject<UCameraComponent>(TEXT("Eye"));
	Eye->SetupAttachment(Arm, USpringArmComponent::SocketName);
	Eye->bUsePawnControlRotation = false;
}

void AShimProbeWalker::Jump()
{
	Super::Jump();
}

void AShimProbeWalker::Walk(const FVector2D& Axis, const FRotator& Facing)
{
	const FRotator Yaw(0.0, Facing.Yaw, 0.0);
	const FRotator Side(0.0, Facing.Yaw + 90.0, 0.0);
	AddMovementInput(Yaw.Vector(), static_cast<float>(Axis.Y));
	AddMovementInput(Side.Vector(), static_cast<float>(Axis.X));
}
