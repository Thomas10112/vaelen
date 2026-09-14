// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// What names the HUD and the controller. No asset, no Blueprint: the mode is
// the one place the two classes of the first screen are said out loud.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "VaelenGameMode.generated.h"

UCLASS()
class AVaelenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaelenGameMode();
};
