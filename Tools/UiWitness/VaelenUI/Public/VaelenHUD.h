// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// The shape 14.09 will write: a HUD that draws a page it did not compose. Its
// includes are the whole claim of the fence - the view leaves, the command
// surface, Unreal, and nothing that names a World.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Vaelen/View/Panel.h"

#include "VaelenHUD.generated.h"

UCLASS()
class AVaelenHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/// The page's rows, written once per frame by Vaelen::View::Lines and
	/// drawn as they are. Sized by the view's own constant, so a page that
	/// grows cannot overrun this.
	char Rows[Vaelen::View::PanelTextBytes] = {};
};
