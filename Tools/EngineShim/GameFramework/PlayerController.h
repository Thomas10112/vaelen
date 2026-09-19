// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "Components/InputComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class APlayerController : public AActor
{
public:
	using Super = APlayerController; ///< see GameFramework/HUD.h

	UInputComponent* InputComponent = nullptr;
	bool bShowMouseCursor = false;

	virtual void SetupInputComponent();
	/// Where the person at the keyboard is looking from and at. 15.10 reads it
	/// on the day turn, which is the only moment this module reads anything of
	/// the frame - there is no Tick here and the fence refuses one.
	///
	/// Declared const because UE's is believed to be, and called from a
	/// NON-const helper in VaelenUI so that a shim which guessed the
	/// qualifier wrong cannot make the real build fail: a const member can
	/// be called on a non-const object either way. This file proves shape;
	/// only the owner's machine proves the engine (ADR-0134).
	virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
};
