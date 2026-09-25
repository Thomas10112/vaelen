// VAELEN - Tools/ShimProbe (19.02). Not built; parsed. README.md says why.
#include "ShimProbeController.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "ShimProbeWalker.h"

void AShimProbeController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Made at run time, not loaded: ADR-0157's contract is asset-free.
	MoveAction = NewObject<UInputAction>(this);
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this);
	LookAction->ValueType = EInputActionValueType::Axis2D;
	Keys = NewObject<UInputMappingContext>(this);

	// Z forward, S back, Q left, D right: one Axis2D, four keys.
	FEnhancedActionKeyMapping& Forward = Keys->MapKey(MoveAction, EKeys::Z);
	Forward.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	FEnhancedActionKeyMapping& Back = Keys->MapKey(MoveAction, EKeys::S);
	Back.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	Back.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	FEnhancedActionKeyMapping& Left = Keys->MapKey(MoveAction, EKeys::Q);
	Left.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	Keys->MapKey(MoveAction, EKeys::D);
	Keys->MapKey(LookAction, EKeys::Mouse2D);
	Keys->MapKey(MoveAction, FKey(FName(TEXT("Up"))));

	if (ULocalPlayer* Local = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Local))
		{
			Input->AddMappingContext(Keys, 0);
		}
	}
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AShimProbeController::OnMove);
		Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &AShimProbeController::OnLook);
	}
}

void AShimProbeController::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (AShimProbeWalker* Walker = Cast<AShimProbeWalker>(GetPawn()))
	{
		Walker->Walk(Axis, GetControlRotation());
	}
}

void AShimProbeController::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (APawn* Body = GetPawn())
	{
		Body->AddControllerYawInput(static_cast<float>(Axis.X));
		Body->AddControllerPitchInput(static_cast<float>(-Axis.Y));
	}
}

void AShimProbeController::Aim(int32 Which)
{
	(void)Which;
}
