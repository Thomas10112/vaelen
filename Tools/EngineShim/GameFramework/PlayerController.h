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
};
