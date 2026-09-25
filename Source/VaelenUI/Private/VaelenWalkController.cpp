// VAELEN - VaelenUI
// Phase 19 task 19.06: the keyboard of the walk. See VaelenWalkController.h.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#include "VaelenWalkController.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Vaelen/Scene/Fence.h"
#include "VaelenWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaelenWalkKeys, Log, All);

namespace
{
	UVaelenWorldSubsystem* HeldWorld(const UWorld* From)
	{
		if (From == nullptr || From->GetGameInstance() == nullptr)
		{
			return nullptr;
		}
		return From->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>();
	}
} // namespace

void AVaelenWalkController::SetupInputComponent()
{
	// The verbs, Tab, Space and F9 first, from the host's table (19.10).
	Super::SetupInputComponent();

	// Made at run time, not loaded: ADR-0157's contract is asset-free. One
	// Axis2D for the four letters, one for the mouse.
	MoveAction = NewObject<UInputAction>(this);
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this);
	LookAction->ValueType = EInputActionValueType::Axis2D;
	Keys = NewObject<UInputMappingContext>(this);

	// Z forward (+Y of the axis), S back, Q left (-X), D right; the arrows the
	// same way, for a keyboard that is not AZERTY.
	FEnhancedActionKeyMapping& Forward = Keys->MapKey(MoveAction, EKeys::Z);
	Forward.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	FEnhancedActionKeyMapping& Back = Keys->MapKey(MoveAction, EKeys::S);
	Back.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	Back.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	FEnhancedActionKeyMapping& Left = Keys->MapKey(MoveAction, EKeys::Q);
	Left.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	Keys->MapKey(MoveAction, EKeys::D);
	FEnhancedActionKeyMapping& Up = Keys->MapKey(MoveAction, EKeys::Up);
	Up.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	FEnhancedActionKeyMapping& Down = Keys->MapKey(MoveAction, EKeys::Down);
	Down.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	Down.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	FEnhancedActionKeyMapping& LeftArrow = Keys->MapKey(MoveAction, EKeys::Left);
	LeftArrow.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	Keys->MapKey(MoveAction, EKeys::Right);
	Keys->MapKey(LookAction, EKeys::Mouse2D);

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
		Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AVaelenWalkController::OnMove);
		Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &AVaelenWalkController::OnLook);
	}
	// The walk looks down the road, not at a cursor.
	bShowMouseCursor = false;
}

void AVaelenWalkController::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	APawn* Body = GetPawn();
	UVaelenWorldSubsystem* World = HeldWorld(GetWorld());
	if (Body == nullptr)
	{
		return;
	}
	const FRotator Facing = GetControlRotation();
	const FRotator Yaw(0.0, Facing.Yaw, 0.0);
	const FRotator Side(0.0, Facing.Yaw + 90.0, 0.0);
	const FVector Ahead = Yaw.Vector() * Axis.Y + Side.Vector() * Axis.X;

	// THE FENCE. The step is checked where it would land, against the
	// walkable tiles of the played region; one that leaves them is not
	// taken. A world not begun, or nobody played, fences nothing - the
	// walker wanders a ground that is nobody's.
	if (World != nullptr && World->Begun() && World->Life().Person != 0u)
	{
		const FVector At = Body->GetActorLocation();
		const FVector Next = At + Ahead * static_cast<double>(StepCm);
		const Vaelen::Scene::Ground& G = World->Scene();
		if (!Vaelen::Scene::Inside(G, World->Life().Region, static_cast<Vaelen::int64>(Next.X),
								   static_cast<Vaelen::int64>(Next.Y)))
		{
			UE_LOG(LogVaelenWalkKeys, Verbose, TEXT("LogVaelenWalk: fence at (%.0f, %.0f)"), Next.X, Next.Y);
			return;
		}
	}
	Body->AddMovementInput(Yaw.Vector(), static_cast<float>(Axis.Y));
	Body->AddMovementInput(Side.Vector(), static_cast<float>(Axis.X));
	UE_LOG(LogVaelenWalkKeys, Verbose, TEXT("LogVaelenWalk: move (%.0f, %.0f) facing %.0f"), Axis.X, Axis.Y,
		   Facing.Yaw);
}

void AVaelenWalkController::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (APawn* Body = GetPawn())
	{
		Body->AddControllerYawInput(static_cast<float>(Axis.X));
		Body->AddControllerPitchInput(static_cast<float>(-Axis.Y));
	}
}

int32 AVaelenWalkController::RegionTheCameraIsOver(int32& OutReach)
{
	OutReach = 1;
	const UVaelenWorldSubsystem* World = HeldWorld(GetWorld());
	const APawn* Body = GetPawn();
	if (World == nullptr || Body == nullptr)
	{
		return 0;
	}
	// The feet, in the map's frame: the land stands at the origin, so a
	// world point is a map point (VaelenLand.h). RegionAt is the scene's own
	// answer - the same one the fence uses - and 0 where there is no land.
	const FVector At = Body->GetActorLocation();
	return static_cast<int32>(
		Vaelen::Scene::RegionAt(World->Scene(), static_cast<Vaelen::int64>(At.X), static_cast<Vaelen::int64>(At.Y)));
}
