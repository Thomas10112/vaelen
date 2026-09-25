// VAELEN - VaelenWalk
// Phase 19 task 19.06: what names the walk's three classes.
//
// No asset, no Blueprint: the HUD of 14.09 (the page), the walk controller
// of VaelenUI (ZQSD, the mouse, and 14.09's keys still) and the walker as
// the pawn a player gets. Chosen at launch by the map URL, so that no config
// line has to change on the owner's machine:
//   UnrealEditor.exe Vaelen.uproject /Engine/Maps/Entry?game=/Script/VaelenWalk.VaelenWalkGameMode -game -log
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "VaelenWalkGameMode.generated.h"

UCLASS()
class VAELENWALK_API AVaelenWalkGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaelenWalkGameMode();
};
