// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

class AGameModeBase : public AActor
{
public:
	using Super = AGameModeBase; ///< see GameFramework/HUD.h

	TSubclassOf<AHUD> HUDClass;
	TSubclassOf<APlayerController> PlayerControllerClass;
	/// 19.02 BELIEF: the walker a walk game mode spawns for its player.
	TSubclassOf<AActor> DefaultPawnClass;
};
