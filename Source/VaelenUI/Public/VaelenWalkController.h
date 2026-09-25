// VAELEN - VaelenUI
// Phase 19 task 19.06: the keyboard of the walk.
//
// 14.09's controller, and on top of it Z/Q/S/D and the mouse, mapped at run
// time with Enhanced Input - no asset (ADR-0157). The verbs, Tab, Space and
// F9 are the parent's, bound from the host's key table (19.10); S is Speak
// there until 19.06's sitting moves the host to WalkKeys, and until then S
// is BOTH south and Speak, which S2 will see and say.
//
// THE BODY IS NOT AN INPUT (ADR-0155): every step is the host's, fenced to
// the walkable tiles of the played region (Vaelen/Scene/Fence.h) - a step
// that would leave them is not taken - and what reaches the world is the
// look of the day turn, from the region under the walker's feet rather than
// under the camera's ray, and the Move verb at the fence. NO Tick.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
// The mapping context made without an asset, and letter keys that follow the
// AZERTY layout, are the beliefs here.
#pragma once

#include "CoreMinimal.h"
#include "VaelenPlayerController.h"

#include "VaelenWalkController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class VAELENUI_API AVaelenWalkController : public AVaelenPlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;

	/// How far ahead a step is checked against the fence, in centimetres.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Walk", meta = (ClampMin = "1"))
	float StepCm = 50.0f;

protected:
	/// The region under the walker's FEET, not under the camera's ray: on
	/// the ground, the two differ by a hill. Reach 1: the walker asks for its
	/// neighbours to be detailed, and no more.
	virtual int32 RegionTheCameraIsOver(int32& OutReach) override;

private:
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);

	UInputAction* MoveAction = nullptr;
	UInputAction* LookAction = nullptr;
	UInputMappingContext* Keys = nullptr;
};
