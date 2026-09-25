// VAELEN - Tools/ShimProbe (19.02). Not built; parsed. README.md says why.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "ShimProbeWalker.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class AShimProbeWalker : public ACharacter
{
	GENERATED_BODY()

public:
	AShimProbeWalker();

	/// Resolves to ACharacter::Jump only with an exact Super (19.02).
	virtual void Jump() override;

	void Walk(const FVector2D& Axis, const FRotator& Facing);

private:
	USpringArmComponent* Arm = nullptr;
	UCameraComponent* Eye = nullptr;
};
