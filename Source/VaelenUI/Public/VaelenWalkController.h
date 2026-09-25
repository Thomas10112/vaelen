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

	/// 19.12: what the days on foot came to, for Vaelen.Stream.Write's line.
	struct FOnFoot
	{
		uint32 Days = 0;	  ///< day turns taken on foot
		uint32 Crossings = 0; ///< days whose turn changed the life's region
		uint32 Refused = 0;	  ///< world refusals TooFar seen after a turn
		uint32 Aimed = 0;	  ///< verbs aimed by the body (not by Tab)
		uint32 Fence = 0;	  ///< steps the fence refused
		uint32 PutBack = 0;	  ///< times the walker was put back after a day
	};
	const FOnFoot& OnFoot() const { return Walked; }

protected:
	/// The region under the walker's FEET, not under the camera's ray: on
	/// the ground, the two differ by a hill. Reach 1: the walker asks for its
	/// neighbours to be detailed, and no more.
	virtual int32 RegionTheCameraIsOver(int32& OutReach) override;
	/// Move at the fence facing a Near region -> that region (CrossingOf);
	/// Speak, Give, Take -> the company figure in front (AimAt); else Tab's.
	virtual uint32 TargetFor(Vaelen::Player::Intent Kind, uint32 TabTarget) override;
	/// The walker put back inside its region (PlaceAfterDay), counted, and
	/// the one line of the day: `LogVaelenWalk: day D tile T region R life L looked K`.
	virtual void AfterTheDay(int32 Looked) override;
	/// `LogVaelenWalk: N days on foot, C crossings, R refused, A aimed, F fence contacts, P put back`.
	virtual void AfterStreamWritten() override;

private:
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	/// Where the body stands and faces, in the map's frame (the land is at the origin).
	bool Feet(Vaelen::int64& X, Vaelen::int64& Y, Vaelen::int64& DirX, Vaelen::int64& DirY) const;

	FOnFoot Walked;
	uint32 RegionBefore = 0;
	uint32 RefusedBefore = 0;

	UInputAction* MoveAction = nullptr;
	UInputAction* LookAction = nullptr;
	UInputMappingContext* Keys = nullptr;
};
