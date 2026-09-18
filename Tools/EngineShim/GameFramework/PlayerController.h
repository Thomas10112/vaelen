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
	virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
};
