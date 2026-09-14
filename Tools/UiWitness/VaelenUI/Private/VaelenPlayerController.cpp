// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// One key is one press: Press() turns it into an intent or refuses it from the
// page alone, and what survives goes to the door's Mean. Space is the recorded
// day turn (ADR-0138). Nothing here asks the world anything else.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
#include "VaelenPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "VaelenWorldSubsystem.h"

void AVaelenPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent == nullptr)
	{
		return;
	}
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AVaelenPlayerController::Work);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AVaelenPlayerController::Rest);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AVaelenPlayerController::Eat);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AVaelenPlayerController::WaitOut);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AVaelenPlayerController::Speak);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AVaelenPlayerController::Give);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AVaelenPlayerController::Take);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AVaelenPlayerController::Move);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AVaelenPlayerController::Space);
}

void AVaelenPlayerController::Work()
{
	Verb(Vaelen::Player::Intent::Work);
}
void AVaelenPlayerController::Rest()
{
	Verb(Vaelen::Player::Intent::Rest);
}
void AVaelenPlayerController::Eat()
{
	Verb(Vaelen::Player::Intent::Eat);
}
void AVaelenPlayerController::WaitOut()
{
	Verb(Vaelen::Player::Intent::Wait);
}
void AVaelenPlayerController::Speak()
{
	Verb(Vaelen::Player::Intent::Speak);
}
void AVaelenPlayerController::Give()
{
	Verb(Vaelen::Player::Intent::Give);
}
void AVaelenPlayerController::Take()
{
	Verb(Vaelen::Player::Intent::Take);
}
void AVaelenPlayerController::Move()
{
	Verb(Vaelen::Player::Intent::Move);
}

void AVaelenPlayerController::Space()
{
	UVaelenWorldSubsystem* Held = GetWorld() != nullptr && GetWorld()->GetGameInstance() != nullptr
									  ? GetWorld()->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>()
									  : nullptr;
	if (Held != nullptr)
	{
		Held->TurnTheDay();
	}
}

void AVaelenPlayerController::Verb(Vaelen::Player::Intent Kind)
{
	UVaelenWorldSubsystem* Held = GetWorld() != nullptr && GetWorld()->GetGameInstance() != nullptr
									  ? GetWorld()->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>()
									  : nullptr;
	if (Held == nullptr)
	{
		return;
	}
	Vaelen::Player::PlayerCommand What;
	// The page's own answer first: an unoffered verb costs the world nothing.
	if (Vaelen::View::Press(Held->Page(), Kind, 0, 1, What) != Vaelen::Player::Refusal::None)
	{
		return;
	}
	Held->Mean(What);
}
