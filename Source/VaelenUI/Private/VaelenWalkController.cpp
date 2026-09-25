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
#include "Vaelen/Scene/Layout.h"
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
			++Walked.Fence;
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

bool AVaelenWalkController::Feet(Vaelen::int64& X, Vaelen::int64& Y, Vaelen::int64& DirX, Vaelen::int64& DirY) const
{
	const APawn* Body = GetPawn();
	if (Body == nullptr)
	{
		return false;
	}
	const FVector At = Body->GetActorLocation();
	const FRotator Yaw(0.0, GetControlRotation().Yaw, 0.0);
	const FVector Ahead = Yaw.Vector();
	X = static_cast<Vaelen::int64>(At.X);
	Y = static_cast<Vaelen::int64>(At.Y);
	// The direction as integers: a thousandth of a unit is enough for AimAt's
	// 30 degrees and CrossingOf's one step ahead.
	DirX = static_cast<Vaelen::int64>(Ahead.X * 1000.0);
	DirY = static_cast<Vaelen::int64>(Ahead.Y * 1000.0);
	return true;
}

uint32 AVaelenWalkController::TargetFor(Vaelen::Player::Intent Kind, uint32 TabTarget)
{
	const UVaelenWorldSubsystem* World = HeldWorld(GetWorld());
	Vaelen::int64 X = 0, Y = 0, DirX = 0, DirY = 0;
	if (World == nullptr || !World->Begun() || !Feet(X, Y, DirX, DirY))
	{
		return TabTarget;
	}
	if (Kind == Vaelen::Player::Intent::Move)
	{
		// One step ahead of the body: the region there, if the life lists it
		// as Near, is what M means at the fence (ADR-0155). Anything else -
		// home, water, the map's edge, a region not Near - falls back to Tab.
		const Vaelen::Scene::Crossing C = Vaelen::Scene::CrossingOf(
			World->Scene(), World->Life(), X + DirX * static_cast<Vaelen::int64>(StepCm) / 1000,
			Y + DirY * static_cast<Vaelen::int64>(StepCm) / 1000);
		if (C.Why == Vaelen::Scene::CrossingWhy::Crossing && C.Region != 0u)
		{
			++Walked.Aimed;
			return C.Region;
		}
		return TabTarget;
	}
	if (Kind == Vaelen::Player::Intent::Speak || Kind == Vaelen::Player::Intent::Give ||
		Kind == Vaelen::Player::Intent::Take)
	{
		// The company figure in front, from the one layout the scenery draws.
		const uint32 Person = Vaelen::Scene::AimAt(World->Layout(), X, Y, DirX, DirY);
		if (Person != 0u)
		{
			++Walked.Aimed;
			return Person;
		}
	}
	return TabTarget;
}

void AVaelenWalkController::AfterTheDay(int32 Looked)
{
	UVaelenWorldSubsystem* World = HeldWorld(GetWorld());
	APawn* Body = GetPawn();
	if (World == nullptr || !World->Begun() || Body == nullptr)
	{
		return;
	}
	const Vaelen::View::LifeView& Life = World->Life();
	const Vaelen::Scene::Ground& G = World->Scene();
	++Walked.Days;
	Walked.Crossings += RegionBefore != 0u && Life.Region != RegionBefore ? 1u : 0u;
	Walked.Refused +=
		Life.Refused > RefusedBefore && Life.LastRefusal == static_cast<uint32>(Vaelen::Player::Refusal::TooFar) ? 1u
																												 : 0u;
	RegionBefore = Life.Region;
	RefusedBefore = Life.Refused;
	// Put back inside the region the life is in now - the same tile-centre
	// rule Run.Walk proves headless (PlaceAfterDay), and counted.
	const FVector At = Body->GetActorLocation();
	Vaelen::int64 X = static_cast<Vaelen::int64>(At.X);
	Vaelen::int64 Y = static_cast<Vaelen::int64>(At.Y);
	if (Vaelen::Scene::PlaceAfterDay(G, Life.Region, X, Y))
	{
		++Walked.PutBack;
		const double Z = static_cast<double>(Vaelen::Scene::HeightAt(G, X, Y)) + 106.0;
		Body->SetActorLocation(FVector(static_cast<double>(X), static_cast<double>(Y), Z));
	}
	uint32 Tile = 0;
	Vaelen::Scene::TileOfPoint(G, X, Y, Tile);
	UE_LOG(LogVaelenWalkKeys, Log, TEXT("LogVaelenWalk: day %u tile %u region %u life %u looked %d"),
		   static_cast<unsigned>(Life.DaysLived), static_cast<unsigned>(Tile),
		   static_cast<unsigned>(Vaelen::Scene::RegionAt(G, X, Y)), static_cast<unsigned>(Life.Region), Looked);
}

void AVaelenWalkController::AfterStreamWritten()
{
	UE_LOG(LogVaelenWalkKeys, Log,
		   TEXT("LogVaelenWalk: %u days on foot, %u crossings, %u refused, %u aimed, %u fence contacts, %u put back"),
		   static_cast<unsigned>(Walked.Days), static_cast<unsigned>(Walked.Crossings),
		   static_cast<unsigned>(Walked.Refused), static_cast<unsigned>(Walked.Aimed),
		   static_cast<unsigned>(Walked.Fence), static_cast<unsigned>(Walked.PutBack));
}
