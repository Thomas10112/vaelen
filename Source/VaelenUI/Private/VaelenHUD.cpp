// VAELEN - VaelenUI
// Phase 14 task 14.09: the first screen.
//
// No World in this translation unit, no Take, no Submit: the page arrives
// composed from the one module that holds a world, and the only way back is
// Press() through that same module. What is drawn is exactly what Lines()
// wrote - one Canvas->DrawText per row, in order, no formatting of our own.
//
// STATUS: VALIDATED (Phase 14) - built by UnrealBuildTool and RUN on
// 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor): eighty-three days
// played at the keyboard, and Tools/Atlas replayed the stream headlessly to the
// same four digests, byte for byte. Tests/Run/Streams/README.md has the lines.
#include "VaelenHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "VaelenWorldSubsystem.h"

void AVaelenHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr || GetWorld() == nullptr || GetWorld()->GetGameInstance() == nullptr)
	{
		return;
	}
	UVaelenWorldSubsystem* Held = GetWorld()->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>();
	if (Held == nullptr)
	{
		return;
	}
	const Vaelen::View::PanelView& Page = Held->Panel();
	const Vaelen::uint32 Written = Vaelen::View::Lines(Page, Rows, Vaelen::View::PanelTextBytes);
	if (Written == 0)
	{
		return; // no page yet, or none that fits whole: half a page is not drawn
	}

	// One row per line of what Lines wrote, in the order it wrote them. The
	// page is ASCII and every row is terminated where its Length says, so a
	// line is a pointer into this buffer and nothing is copied twice.
	const float Line = 14.0f;
	float Y = 16.0f;
	Vaelen::uint32 Start = 0;
	for (Vaelen::uint32 i = 0; i <= Written; ++i)
	{
		if (i != Written && Rows[i] != '\n')
		{
			continue;
		}
		Rows[i] = '\0';
		Canvas->DrawText(GEngine->GetSmallFont(), FString(ANSI_TO_TCHAR(Rows + Start)), 16.0f, Y);
		Y += Line;
		Start = i + 1;
	}
}
