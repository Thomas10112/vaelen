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
	/// 19.11b BELIEF: spawns and possesses a pawn for the controller as the
	/// game's start does - what Vaelen.Walk asks when the first pawn fell out
	/// of the world (no floor under the origin before the land exists).
	/// The engine takes an AController*, which this shim has none of
	/// (PlayerController.h says so); a player controller converts to it there.
	void RestartPlayer(class APlayerController* NewPlayer);
};
