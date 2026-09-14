// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// What a screen is here: Lines() into a buffer, and the buffer drawn. No
// World in this translation unit, no Take.h, no Submit - the page arrives
// composed and the only way back is Press() through the subsystem.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
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
	const uint32 Written = Vaelen::View::Lines(Page, Rows, Vaelen::View::PanelTextBytes);
	if (Written == 0)
	{
		return;
	}
	// One row per line of what Lines wrote, in the order it wrote them. The
	// page is ASCII and every row is terminated where its Length says, so a
	// line is a pointer into this buffer and nothing is copied twice.
	// Bottom-aligned: the page grows upward from the foot of the screen, so
	// the newest chronicle line is always in the same place.
	const float Line = 14.0f;
	float Y = Canvas->SizeY - Line * static_cast<float>(Page.RowCount) - Line;
	uint32 Start = 0;
	for (uint32 i = 0; i <= Written; ++i)
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
