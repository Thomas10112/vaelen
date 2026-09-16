// VAELEN - VaelenUI
// Phase 14 task 14.09: the first screen.
//
// The HUD draws the page and composes nothing. Vaelen::View::Lines writes the
// rows into this buffer and Canvas->DrawText puts them on the screen verbatim,
// so the bytes on screen are the bytes View.Panel digests and a screenshot can
// be checked against a number.
//
// Tools/check_ui_fence.py reads this file: every Vaelen include here is a view
// leaf, and nothing in this module names a World.
//
// STATUS: BUILT (Phase 14) - compiled and linked by UnrealBuildTool 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor); NOT yet run, so nothing here is VALIDATED until 14.10's lines come out of a log
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Vaelen/View/Panel.h"

#include "VaelenHUD.generated.h"

UCLASS()
class VAELENUI_API AVaelenHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/// The page's rows, rewritten every frame from the subsystem's cached
	/// PanelView - which the subsystem retakes only when something moved.
	/// Sized by the view's own constant, so a page that grows cannot overrun
	/// this and Lines() refuses a buffer that would not hold the whole page.
	char Rows[Vaelen::View::PanelTextBytes] = {};
};
