// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// The shape 14.08 will write. What it proves here: a UI module can include a
// VaelenGame header - and therefore needs the header UnrealHeaderTool writes
// for it stubbed from a SHARED directory - while seeing no kernel header but
// the view leaves.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Panel.h"

#include "VaelenWorldSubsystem.generated.h"

UCLASS()
class UVaelenWorldSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/// The page of this frame. Views out, nothing in: the UI never holds a
	/// world, and this is the only way it gets one.
	const Vaelen::View::PanelView& Page();

	/// One intent through the door, which stamps it and records it. The
	/// module's single write.
	Vaelen::Player::Refusal Mean(const Vaelen::Player::PlayerCommand& What);

	/// The recorded day turn (ADR-0138).
	void TurnTheDay();

private:
	Vaelen::View::PanelView Panel_;
	uint64 Frames = 0;
};
