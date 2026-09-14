// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class UCanvas;
class APlayerController;

class AHUD : public AActor
{
public:
	/// AActor's own `using Super = AActor;` is exactly right for an actor
	/// derived straight from it, and says so. This is the day the file warned
	/// about: a HUD derives from AHUD, so Super is re-named here, and every
	/// base class this shim gains from now on does the same. Without it,
	/// AVaelenHUD::Super would resolve to AActor and Super::DrawHUD() would
	/// not compile - or, worse, would find something else that does.
	using Super = AHUD;

	UCanvas* Canvas = nullptr;
	APlayerController* PlayerOwner = nullptr;

	virtual void DrawHUD();
};
