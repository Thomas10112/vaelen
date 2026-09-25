// VAELEN - VaelenWalk
// Phase 19 task 19.06: the body that walks.
//
// A character and nothing more: a capsule, a movement component at the
// scene's own slope (Terrain.h's 44.76 deg, the default walkable floor), a
// spring arm and a camera. It holds no view and no world, and it has NO
// Tick: where it stands is the host's state (ADR-0155) - what reaches the
// world is a look and, at the fence, a Move - and the controller of
// VaelenUI is the only thing that moves it.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "VaelenWalker.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class VAELENWALK_API AVaelenWalker : public ACharacter
{
	GENERATED_BODY()

public:
	AVaelenWalker();

	/// Half the capsule: what to add to the ground's height to stand on it.
	float StandingHalfHeight() const;

private:
	USpringArmComponent* Arm = nullptr;
	UCameraComponent* Eye = nullptr;
};
