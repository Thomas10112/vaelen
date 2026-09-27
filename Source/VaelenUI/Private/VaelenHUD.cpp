// VAELEN - VaelenUI
// Phase 14 task 14.09: the first screen.
//
// No World in this translation unit, no Take, no Submit: the page arrives
// composed from the one module that holds a world, and the only way back is
// Press() through that same module. What is drawn is exactly what Lines()
// wrote - one Canvas->DrawText per row, in order, no formatting of our own.
//
// STATUS: UNVERIFIED (engine) since 22.01 - its code has changed after the last build that
// compiled it (b0921, 15.10, 867a129): the front end's pages (title, loading, pause) drawn before
// the world's page. Parsed against Tools/EngineShim, never compiled; sitting S4 builds it.
// BUILD: b0921 - Tools/engine_builds.txt
//
// UNTIL 22.01: VALIDATED (Phase 14) - built by UnrealBuildTool and RUN on
// 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor): eighty-three days
// played at the keyboard, and Tools/Atlas replayed the stream headlessly to the
// same four digests, byte for byte. Tests/Run/Streams/README.md has the lines.
#include "VaelenHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "VaelenPlayerController.h"
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
	// 22.01: the front end's pages come before the world's page. Host text,
	// not the world's: nothing here is digested, and the panel below is drawn
	// only when the front end says the world is up and not paused.
	if (const AVaelenPlayerController* Front = Cast<AVaelenPlayerController>(GetOwningPlayerController()))
	{
		if (Front->Front() != AVaelenPlayerController::EFront::Playing)
		{
			DrawFront(*Held, *Front);
			return;
		}
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

void AVaelenHUD::DrawFront(UVaelenWorldSubsystem& Held, const AVaelenPlayerController& Front)
{
	// The pages, as rows of text in the small font: what a person at the
	// keyboard reads before the world, while it is made, and while it stands.
	TArray<FString> Rows_;
	switch (Front.Front())
	{
	case AVaelenPlayerController::EFront::Title:
	{
		Rows_.Add(TEXT("VAELEN"));
		Rows_.Add(TEXT("AELVOR - a living world, four hundred years deep before you arrive"));
		Rows_.Add(TEXT(""));
		Rows_.Add(TEXT("Enter    a new world (AELVOR 128, 120 years; a few seconds)"));
		FString Newest;
		uint64 Tick = 0;
		if (Held.NewestSave(Newest, Tick))
		{
			Rows_.Add(FString::Printf(TEXT("F8       continue: %s (tick %llu)"), *Newest,
									  static_cast<unsigned long long>(Tick)));
		}
		else
		{
			Rows_.Add(TEXT("F8       continue: no save yet"));
		}
		Rows_.Add(TEXT("Escape   quit"));
		break;
	}
	case AVaelenPlayerController::EFront::Loading:
		Rows_.Add(TEXT("VAELEN"));
		Rows_.Add(TEXT(""));
		Rows_.Add(Front.LoadingWords());
		break;
	case AVaelenPlayerController::EFront::Paused:
	{
		const Vaelen::View::LifeView& Life = Held.Life();
		Rows_.Add(FString::Printf(TEXT("PAUSED - AELVOR %d, year %u day %u, %u days lived"), Held.Size(),
								  static_cast<unsigned>(Life.Year), static_cast<unsigned>(Life.Day),
								  static_cast<unsigned>(Life.DaysLived)));
		Rows_.Add(TEXT(""));
		Rows_.Add(TEXT("Escape   back to the world"));
		Rows_.Add(TEXT("F5       save (quick)"));
		Rows_.Add(TEXT("F9       write the stream"));
		Rows_.Add(
			FString::Printf(TEXT("F10      quit (the autosave is at most %d day turns old)"), Held.AutosaveEvery()));
		break;
	}
	default:
		return;
	}
	if (!Front.Notice().IsEmpty())
	{
		Rows_.Add(TEXT(""));
		Rows_.Add(Front.Notice());
	}
	const float Line = 14.0f;
	float Y = 16.0f;
	for (const FString& Row : Rows_)
	{
		Canvas->DrawText(GEngine->GetSmallFont(), Row, 16.0f, Y);
		Y += Line;
	}
}
