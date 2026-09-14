// VAELEN - VaelenUI
// Phase 14 task 14.09: one key, one press, one intent.
//
// The eight verbs are lettered the way the PAGE letters them: the panel
// carries a Key per verb (14.06) and these bindings are that letter, so the
// screen and the keyboard cannot drift apart. Tab cycles what the next
// Speak, Give, Take or Move is aimed at; Space turns the day, which is the
// one host input that is not a gameplay command (ADR-0138); F9 writes the
// stream.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, not yet built by UnrealBuildTool
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Vaelen/Player/Intent.h"

#include "VaelenPlayerController.generated.h"

UCLASS()
class VAELENUI_API AVaelenPlayerController : public APlayerController
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
	void NextTarget();
	void TurnTheDay();
	void WriteStream();

	/// Through the page and then through the door, and nowhere else. The page
	/// answers first - an unoffered verb costs the world nothing - and what it
	/// offers becomes a PlayerCommand the module hands to Mean.
	void Verb(Vaelen::Player::Intent Kind);

	/// What Speak, Give, Take and Move are aimed at: an index into the page's
	/// company for the three that need a person, and into its neighbours for
	/// the one that needs a place. Tab moves it; nothing else does.
	uint32 Aim = 0;
};
