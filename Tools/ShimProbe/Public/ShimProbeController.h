// VAELEN - Tools/ShimProbe (19.02). Not built; parsed. README.md says why.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "ShimProbeController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class AShimProbeController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

private:
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	/// A method of the wrong shape for a handler: what test_engine_shim.py binds by mistake.
	void Aim(int32 Which);

	UInputAction* MoveAction = nullptr;
	UInputAction* LookAction = nullptr;
	UInputMappingContext* Keys = nullptr;
};
