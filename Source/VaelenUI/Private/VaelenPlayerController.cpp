// VAELEN - VaelenUI
// Phase 14 task 14.09: one key, one press, one intent.
//
// The module's single write is the Mean below. Everything else here reads a
// page: the target a key aims at, whether the page offers the verb at all,
// and what it foresaw when it does not.
//
// STATUS: BUILT (engine) - compiled and linked by UnrealBuildTool on 2026-10-01 (b1001, d96a418, UE 5.6.1,
// MSVC) and RUN under play-in-editor: Enter began a world from the title page, the eight verbs and Tab queued
// and aimed (the sitting's log, Tests/Run/Sessions/s4-2026-10-01.log). FKey(FName) and the next-tick timer are
// beliefs no more. Not VALIDATED: the page's rows were not brought back. The record of what earlier builds
// validated follows.
// BUILD: b1001 - Tools/engine_builds.txt
//
// UNTIL 2026-10-01: UNVERIFIED (engine) since 19.10 (and 22.01: the front end's pages, keys and autosave) - its code
// had changed after the last build that compiled it (b0921, 15.10, 867a129): the eight verbs bound from the host's key
// table (ADR-0158) instead of eight literals.
//
// UNTIL 19.10: VALIDATED (Phase 14) for what Phase 14 left here - built by
// UnrealBuildTool and RUN on 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development
// Editor): eighty-three days played at the keyboard, and Tools/Atlas replayed
// the stream headlessly to the same four digests, byte for byte.
// Tests/Run/Streams/README.md has the lines.
//
// VALIDATED (Phase 15 task 15.10) for what 15.10 added - built by
// UnrealBuildTool and RUN on 2026-09-21 (UE 5.6, MSVC 19.51, Win64 Development
// Editor): the look, the cadence argument, the camera and Vaelen.TakeUp. 142
// day turns, 138 looks, 4 takings and 2 intents were played and written to
// Tests/Run/Streams/aelvor128-played-2026-09-21.stream, which replays
// headlessly to the same four digests and keeps all six clauses of the phase
// gate. This line said UNVERIFIED for three days while it was true.
#include "VaelenPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "VaelenWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaelenUI, Log, All);

namespace
{
	/// The one way to the module that holds the world. Null in the editor
	/// without a game instance, and the caller does nothing then.
	UVaelenWorldSubsystem* Held(const UWorld* From)
	{
		if (From == nullptr || From->GetGameInstance() == nullptr)
		{
			return nullptr;
		}
		return From->GetGameInstance()->GetSubsystem<UVaelenWorldSubsystem>();
	}

	/// A capital letter of the page's table as the key of that name: EKeys::T
	/// is the key named "T", and so for every letter (19.10, ADR-0158).
	FKey KeyOf(uint8 Letter)
	{
		const TCHAR Name[2] = {static_cast<TCHAR>(Letter), TEXT('\0')};
		return FKey(FName(Name));
	}
} // namespace

void AVaelenPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent == nullptr)
	{
		return;
	}
	// The letters the page prints (PanelView::Verbs[i].Key), from the one
	// table the host holds (19.10, ADR-0158) - eight literals until then, and
	// nothing checked that they agreed with the page. In Intent order.
	const UVaelenWorldSubsystem* World = Held(GetWorld());
	const Vaelen::View::PanelKeys& Keys = World != nullptr ? World->Keys() : Vaelen::View::DefaultKeys;
	InputComponent->BindKey(KeyOf(Keys.Keys[0]), IE_Pressed, this, &AVaelenPlayerController::WaitOut);
	InputComponent->BindKey(KeyOf(Keys.Keys[1]), IE_Pressed, this, &AVaelenPlayerController::Work);
	InputComponent->BindKey(KeyOf(Keys.Keys[2]), IE_Pressed, this, &AVaelenPlayerController::Rest);
	InputComponent->BindKey(KeyOf(Keys.Keys[3]), IE_Pressed, this, &AVaelenPlayerController::Eat);
	InputComponent->BindKey(KeyOf(Keys.Keys[4]), IE_Pressed, this, &AVaelenPlayerController::Move);
	InputComponent->BindKey(KeyOf(Keys.Keys[5]), IE_Pressed, this, &AVaelenPlayerController::Speak);
	InputComponent->BindKey(KeyOf(Keys.Keys[6]), IE_Pressed, this, &AVaelenPlayerController::Give);
	InputComponent->BindKey(KeyOf(Keys.Keys[7]), IE_Pressed, this, &AVaelenPlayerController::Take);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AVaelenPlayerController::NextTarget);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AVaelenPlayerController::TurnTheDay);
	InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &AVaelenPlayerController::WriteStream);
	// 22.01: the front end's keys, said on the pages the HUD draws.
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AVaelenPlayerController::NewWorld);
	InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &AVaelenPlayerController::Continue);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AVaelenPlayerController::TogglePause);
	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &AVaelenPlayerController::QuickSave);
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &AVaelenPlayerController::Quit);
	// 19.11b: the hooks a subclass fills follow the SUBSYSTEM's signals, not
	// this controller's keys, so that Vaelen.Stream.Write, Vaelen.Day and
	// Vaelen.TakeUp reach them as F9 and Space do. AddUObject: the bindings
	// die with the controller. Its own name: the owner's build of 3ee975a
	// stopped on a second `World` in the scope of line 72's (MSVC C4456).
	if (UVaelenWorldSubsystem* Signals = Held(GetWorld()))
	{
		Signals->OnStreamWritten.AddUObject(this, &AVaelenPlayerController::AfterStreamWritten);
		Signals->OnViewsTaken.AddUObject(this, &AVaelenPlayerController::AfterViewsTaken);
	}
	bShowMouseCursor = true;
	RefreshFront(); // 22.01: the title page, before any key
}

void AVaelenPlayerController::Work()
{
	Verb(Vaelen::Player::Intent::Work);
}
void AVaelenPlayerController::Rest()
{
	Verb(Vaelen::Player::Intent::Rest);
}
void AVaelenPlayerController::Eat()
{
	Verb(Vaelen::Player::Intent::Eat);
}
void AVaelenPlayerController::WaitOut()
{
	Verb(Vaelen::Player::Intent::Wait);
}
void AVaelenPlayerController::Speak()
{
	Verb(Vaelen::Player::Intent::Speak);
}
void AVaelenPlayerController::Give()
{
	Verb(Vaelen::Player::Intent::Give);
}
void AVaelenPlayerController::Take()
{
	Verb(Vaelen::Player::Intent::Take);
}
void AVaelenPlayerController::Move()
{
	Verb(Vaelen::Player::Intent::Move);
}

void AVaelenPlayerController::NextTarget()
{
	UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || bPaused_)
	{
		return;
	}
	// One counter for both lists: the company a Speak can reach and the
	// neighbours a Move can. It wraps on the longer of the two, so every
	// target of either is reachable by pressing Tab enough times.
	const Vaelen::View::LifeView& Life = World->Life();
	const uint32 Most = Life.CompanyCount > Life.NearCount ? Life.CompanyCount : Life.NearCount;
	Aim = Most == 0 ? 0 : (Aim + 1) % Most;
	UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: aim %u of %u"), static_cast<unsigned>(Aim),
		   static_cast<unsigned>(Most));
}

int32 AVaelenPlayerController::RegionTheCameraIsOver(int32& OutReach)
{
	OutReach = 0;
	const UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || !(TileSize > 0.0f))
	{
		return 0;
	}
	FVector From = FVector::ZeroVector;
	FRotator Facing = FRotator::ZeroRotator;
	// The VIEW point and not the pawn's: what the person at the keyboard is
	// looking at is what the camera is pointed at, which is the whole question.
	GetPlayerViewPoint(From, Facing);
	// FRotator::Vector(): the direction a rotation faces. This module has never
	// compiled it, but VaelenViewDrawer.cpp has compiled its exact inverse -
	// FVector::Rotation() - so the pair is present and spelled this way.
	const FVector Ahead = Facing.Vector();

	// Where the view ray meets the ground plane the map is drawn on (Z = 0).
	// A ray that goes up, or along the horizon, never meets it: that is a host
	// looking at the sky, and the honest answer is nowhere.
	// KINDA_SMALL_NUMBER and not UE_KINDA_SMALL_NUMBER. Both are spelled in
	// UE 5.6; only one of them is spelled anywhere this project has ever
	// compiled - VaelenViewDrawer.cpp, built on 2026-09-16 - and between a
	// belief and a build the build wins.
	if (Ahead.Z > -KINDA_SMALL_NUMBER || From.Z <= 0.0)
	{
		return 0;
	}
	const double Along = From.Z / -Ahead.Z;
	const FVector On = From + Ahead * Along - MapOrigin;

	// How far out to ask for detail, from how high the camera stands. A host's
	// rule of thumb and nothing more - the world neither knows nor cares how it
	// was arrived at, only what number arrived.
	const double Tiles = From.Z / static_cast<double>(TileSize);
	const int32 Borders = TilesPerBorder > 0 ? static_cast<int32>(Tiles / static_cast<double>(TilesPerBorder)) : 0;
	OutReach = Borders < 0 ? 0 : (Borders > 3 ? 3 : Borders);
	return World->RegionUnderGround(On.X, On.Y, static_cast<double>(TileSize));
}

void AVaelenPlayerController::TurnTheDay()
{
	UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || bPaused_)
	{
		return; // 22.01: paused, the day does not turn
	}
	// THE LOOK BEFORE THE TURN, and only here. The subsystem remembers it and
	// hands it through the door as the day turns, so the stream carries one
	// Looked per DayTurned and a replay puts them back in that order.
	// AND ONLY WHERE THE WORLD CAN ACT ON IT. A world begun without the daily
	// cadence ignores every look, so recording them would put a hundred records
	// in a stream that change nothing - and would stop this host writing the
	// stream Phase 14 wrote, which its own comments promise it still writes.
	int32 Looked = -1;
	if (bLookFromCamera && World->Streaming())
	{
		int32 Reach = 0;
		Looked = RegionTheCameraIsOver(Reach);
		World->Watch(Looked, Reach, MostRegions);
	}
	World->AdvanceDay(1); // one DayTurned, recorded (ADR-0138)
	AfterTheDay(Looked);
}

void AVaelenPlayerController::WriteStream()
{
	UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr)
	{
		return;
	}
	FString Path;
	UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: stream %s"), World->WriteStream(Path) ? *Path : TEXT("not written"));
	// AfterStreamWritten is not called here: it follows the subsystem's
	// OnStreamWritten (bound in SetupInputComponent), so that the console's
	// Vaelen.Stream.Write and this key print the same summary (19.11b).
}

void AVaelenPlayerController::Verb(Vaelen::Player::Intent Kind)
{
	UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || bPaused_)
	{
		return; // 22.01: paused, no verb reaches the door
	}
	const Vaelen::View::PanelView& Page = World->Panel();
	const Vaelen::View::LifeView& Life = World->Life();

	// What this verb is aimed at, read from the page: a person for the three
	// that need one, a region for the one that needs a place, nothing for the
	// four that need neither.
	uint32 Target = 0;
	if (Kind == Vaelen::Player::Intent::Speak || Kind == Vaelen::Player::Intent::Give ||
		Kind == Vaelen::Player::Intent::Take)
	{
		Target = Aim < Life.CompanyCount ? Life.Company[Aim].Person : 0u;
	}
	else if (Kind == Vaelen::Player::Intent::Move)
	{
		Target = Aim < Life.NearCount ? Life.Near[Aim] : 0u;
	}
	// 19.11: the walk aims by where the body stands and faces; Tab is its fallback.
	Target = TargetFor(Kind, Target);

	Vaelen::Player::PlayerCommand What;
	const Vaelen::Player::Refusal Foreseen = Vaelen::View::Press(Page, Kind, Target, 1, What);
	if (Foreseen != Vaelen::Player::Refusal::None)
	{
		// The page's own answer, before any world is asked.
		UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: %s -> %s"), ANSI_TO_TCHAR(Vaelen::Player::IntentName(Kind)),
			   ANSI_TO_TCHAR(Vaelen::Player::RefusalName(Foreseen)));
		return;
	}
	const Vaelen::Player::Refusal Answer = World->Mean(What);
	UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: %s -> %s"), ANSI_TO_TCHAR(Vaelen::Player::IntentName(Kind)),
		   Answer == Vaelen::Player::Refusal::None ? TEXT("queued")
												   : ANSI_TO_TCHAR(Vaelen::Player::RefusalName(Answer)));
}

// ---------------------------------------------------------------- 22.01: the front end

AVaelenPlayerController::EFront AVaelenPlayerController::Front() const
{
	if (bPaused_)
	{
		return EFront::Paused;
	}
	if (bLoading_)
	{
		return EFront::Loading;
	}
	const UVaelenWorldSubsystem* World = Held(GetWorld());
	return World != nullptr && World->Begun() ? EFront::Playing : EFront::Title;
}

bool AVaelenPlayerController::LinesLanded() const
{
	const UVaelenWorldSubsystem* World = Held(GetWorld());
	return World != nullptr && World->Begun();
}

void AVaelenPlayerController::RefreshFront()
{
	// Composed when the page changes: after a key, after the lines ran, on
	// pause and resume - and not on a frame. The rows say the keys and what
	// the keys run, from the same lines the keys run.
	FrontRows_.Reset();
	UVaelenWorldSubsystem* World = Held(GetWorld());
	switch (Front())
	{
	case EFront::Title:
	{
		TArray<FString> New;
		NewWorldLines(New);
		FrontRows_.Add(TEXT("VAELEN"));
		FrontRows_.Add(TEXT("AELVOR - a living world, four hundred years deep before you arrive"));
		FrontRows_.Add(TEXT(""));
		FrontRows_.Add(FString::Printf(TEXT("Enter    a new world: %s (half a minute)"),
									   New.Num() > 0 ? *New[0] : TEXT("nothing to run")));
		FString Newest;
		uint64 Tick = 0;
		if (World != nullptr && World->NewestSave(Newest, Tick))
		{
			FrontRows_.Add(FString::Printf(TEXT("F8       continue: %s (tick %llu)"), *Newest,
										   static_cast<unsigned long long>(Tick)));
		}
		else
		{
			FrontRows_.Add(TEXT("F8       continue: no save yet"));
		}
		FrontRows_.Add(TEXT("Escape   quit"));
		break;
	}
	case EFront::Loading:
		FrontRows_.Add(TEXT("VAELEN"));
		FrontRows_.Add(TEXT(""));
		FrontRows_.Add(Loading_);
		break;
	case EFront::Paused:
	{
		const Vaelen::View::LifeView& Life = World->Life();
		FrontRows_.Add(FString::Printf(TEXT("PAUSED - AELVOR %d, year %u day %u, %u days lived"), World->Size(),
									   static_cast<unsigned>(Life.Year), static_cast<unsigned>(Life.Day),
									   static_cast<unsigned>(Life.DaysLived)));
		FrontRows_.Add(TEXT(""));
		FrontRows_.Add(TEXT("Escape   back to the world"));
		FrontRows_.Add(TEXT("F5       save (quick)"));
		FrontRows_.Add(TEXT("F9       write the stream"));
		FrontRows_.Add(
			FString::Printf(TEXT("F10      quit (the autosave is at most %d day turns old)"), World->AutosaveEvery()));
		break;
	}
	default:
		break;
	}
	if (!FrontRows_.IsEmpty() && !Notice_.IsEmpty())
	{
		FrontRows_.Add(TEXT(""));
		FrontRows_.Add(Notice_);
	}
}

void AVaelenPlayerController::NewWorld()
{
	const UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || World->Begun() || bLoading_ || GetWorld() == nullptr)
	{
		return; // Enter means nothing but on the title page
	}
	Pending_.Reset();
	NewWorldLines(Pending_);
	Loading_ = TEXT("Generating AELVOR: three hundred years of pre-history and a hundred and twenty of the world. "
					"Half a minute or so; the window does not answer meanwhile.");
	Notice_.Reset();
	bLoading_ = true;
	RefreshFront();
	// Drawn this frame, run the next: the one deferral in this module, and
	// not a Tick - it fires once, and the world moves on the console lines it
	// runs exactly as it would on the same lines typed (ADR-0138).
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AVaelenPlayerController::RunPending);
}

void AVaelenPlayerController::Continue()
{
	UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || World->Begun() || bLoading_ || GetWorld() == nullptr)
	{
		return;
	}
	FString Name;
	uint64 Tick = 0;
	if (!World->NewestSave(Name, Tick))
	{
		Notice_ = TEXT("no save to continue from");
		RefreshFront();
		return;
	}
	Pending_.Reset();
	ContinueLines(Name, Pending_);
	Loading_ = FString::Printf(TEXT("Loading %s (tick %llu)."), *Name, static_cast<unsigned long long>(Tick));
	Notice_.Reset();
	bLoading_ = true;
	RefreshFront();
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AVaelenPlayerController::RunPending);
}

void AVaelenPlayerController::RunPending()
{
	bLoading_ = false;
	if (GEngine == nullptr || GetWorld() == nullptr || Pending_.IsEmpty())
	{
		RefreshFront();
		return;
	}
	const TArray<FString> Lines = Pending_;
	Pending_.Reset();
	for (const FString& Line : Lines)
	{
		GEngine->Exec(GetWorld(), *Line);
		const UVaelenWorldSubsystem* World = Held(GetWorld());
		const bool Begun = World != nullptr && World->Begun();
		UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: front end ran `%s`: %s"), *Line,
			   Begun ? TEXT("a world is begun") : TEXT("no world"));
		if (!Begun)
		{
			// The line said why on its own log; the title page says that it did not.
			Notice_ = FString::Printf(TEXT("no world after `%s` - see the log"), *Line);
			RefreshFront();
			return;
		}
	}
	if (!LinesLanded())
	{
		// A world, and not what the page promised (the walk: no body placed).
		// One world per host: the log says what stopped, and the page is the
		// world's from here, since there is no way back to the title.
		UE_LOG(LogVaelenUI, Warning,
			   TEXT("LogVaelenUI: the front end's lines ran and the page's promise did not land - see the log above"));
	}
	RefreshFront();
}

void AVaelenPlayerController::TogglePause()
{
	const UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || bLoading_)
	{
		return;
	}
	if (!World->Begun())
	{
		Quit(); // Escape on the title page
		return;
	}
	bPaused_ = !bPaused_;
	if (!bPaused_)
	{
		Notice_.Reset(); // read on the pause page; gone with it
	}
	RefreshFront();
	UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: %s"), bPaused_ ? TEXT("paused") : TEXT("resumed"));
}

void AVaelenPlayerController::QuickSave()
{
	UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr || !World->Begun() || bLoading_)
	{
		return;
	}
	FString Where, Check;
	Notice_ = FString::Printf(
		TEXT("%s%s"), World->Save(TEXT("quick"), Where, Check) ? TEXT("saved: ") : TEXT("save refused: "), *Where);
	RefreshFront();
	UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: %s"), *Notice_);
}

void AVaelenPlayerController::Quit()
{
	// From the pause page (F10), or from the title page through Escape: the
	// world is not saved here - F5 is a key away and the autosave is ten
	// days old at most. Through the console's own `quit`, which the editor
	// turns into "stop playing" and a package into an orderly exit (the
	// review of 22.01: a platform exit closed the whole editor).
	if (bLoading_ || (Front() != EFront::Paused && Front() != EFront::Title))
	{
		return;
	}
	UE_LOG(LogVaelenUI, Log, TEXT("LogVaelenUI: quit"));
	ConsoleCommand(TEXT("quit"));
}
