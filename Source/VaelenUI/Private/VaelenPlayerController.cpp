// VAELEN - VaelenUI
// Phase 14 task 14.09: one key, one press, one intent.
//
// The module's single write is the Mean below. Everything else here reads a
// page: the target a key aims at, whether the page offers the verb at all,
// and what it foresaw when it does not.
//
// STATUS: VALIDATED (Phase 14) for what Phase 14 left here - built by
// UnrealBuildTool and RUN on 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development
// Editor): eighty-three days played at the keyboard, and Tools/Atlas replayed
// the stream headlessly to the same four digests, byte for byte.
// Tests/Run/Streams/README.md has the lines.
//
// UNVERIFIED (Phase 15 task 15.10) for everything added on 2026-09-18 - the
// look, the cadence argument, the camera. It parses against Tools/EngineShim
// and UnrealBuildTool has never seen it. A file that said VALIDATED over code
// no compiler has read would be exactly the fake "done" this project refuses,
// so the line is split rather than dated forward.
#include "VaelenPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
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
} // namespace

void AVaelenPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent == nullptr)
	{
		return;
	}
	// The letters the page carries (PanelView::Verbs[i].Key), in Intent order.
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AVaelenPlayerController::Work);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AVaelenPlayerController::Rest);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AVaelenPlayerController::Eat);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AVaelenPlayerController::WaitOut);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AVaelenPlayerController::Speak);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AVaelenPlayerController::Give);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AVaelenPlayerController::Take);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AVaelenPlayerController::Move);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AVaelenPlayerController::NextTarget);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AVaelenPlayerController::TurnTheDay);
	InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &AVaelenPlayerController::WriteStream);
	bShowMouseCursor = true;
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
	if (World == nullptr)
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
	if (World == nullptr)
	{
		return;
	}
	// THE LOOK BEFORE THE TURN, and only here. The subsystem remembers it and
	// hands it through the door as the day turns, so the stream carries one
	// Looked per DayTurned and a replay puts them back in that order.
	// AND ONLY WHERE THE WORLD CAN ACT ON IT. A world begun without the daily
	// cadence ignores every look, so recording them would put a hundred records
	// in a stream that change nothing - and would stop this host writing the
	// stream Phase 14 wrote, which its own comments promise it still writes.
	if (bLookFromCamera && World->Streaming())
	{
		int32 Reach = 0;
		const int32 Region = RegionTheCameraIsOver(Reach);
		World->Watch(Region, Reach, MostRegions);
	}
	World->AdvanceDay(1); // one DayTurned, recorded (ADR-0138)
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
}

void AVaelenPlayerController::Verb(Vaelen::Player::Intent Kind)
{
	UVaelenWorldSubsystem* World = Held(GetWorld());
	if (World == nullptr)
	{
		return;
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
