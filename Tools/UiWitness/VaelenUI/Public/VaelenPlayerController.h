// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// Every key of the first screen, bound to what the PAGE says the key is.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Vaelen/Player/Intent.h"

#include "VaelenPlayerController.generated.h"

UCLASS()
class AVaelenPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;

private:
	void Work();
	void Rest();
	void Eat();
	void WaitOut();
	void Speak();
	void Give();
	void Take();
	void Move();
	void Space();

	void Verb(Vaelen::Player::Intent Kind);
};
