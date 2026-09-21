// VAELEN - VaelenGame
// Phase 14 task 14.08: the one place a world is held.
//
// This file may name a World, a Run and a Take, and it is the only file of
// this module that may: the public header holds a pointer to FVaelenHeld and
// nothing else, so the UI that includes it has no vocabulary for the
// simulation. Tools/check_ui_fence.py reads Public and deliberately not
// Private, which is that seam written down.
//
// STATUS: VALIDATED (Phase 14) for what Phase 14 left here - built by
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
#include "VaelenWorldSubsystem.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/View/Take.h"

/// Everything the subsystem owns. One world, one door, one graph cache, and
/// the five views a host reads - retaken when something moved and never on a
/// frame.
struct FVaelenHeld
{
	TUniquePtr<Vaelen::Run::Aelvor> World;
	TUniquePtr<Vaelen::Run::Door> Door;
	Vaelen::WorldGen::RegionGraphCache Ways;
	bool Streaming = false;

	/// The ground, taken ONCE when the world is begun. Land.h says it is not a
	/// per-frame structure and means it: 65536 tiles at eight bytes is half a
	/// megabyte. It is here because it is what turns a point the camera is
	/// over into a region, and the map does not move.
	Vaelen::View::MapView Ground;

	/// Where the host says it is looking, and whether it has ever said. Two
	/// fields and not one, because a host looking at region 0 (nowhere) and a
	/// host that has never looked are different: the first is recorded, the
	/// second is the Phase 14 stream.
	Vaelen::Run::Attention Eyes;
	bool Watched = false;

	Vaelen::View::WorldView Frame;
	Vaelen::View::PeopleView Folk;
	Vaelen::View::LifeView Life;
	Vaelen::View::ChronicleView Told;
	Vaelen::View::PanelView Page;

	void TakeAll()
	{
		if (!World)
		{
			return;
		}
		const Vaelen::View::ViewSources From = World->Sources();
		Vaelen::View::TakeView(World->Instance(), From, Frame);
		Vaelen::View::TakePeopleView(World->Instance(), From, Folk);
		Vaelen::View::TakeLifeView(World->Instance(), From, Ways, Life);
		Vaelen::View::TakeChronicleView(World->Instance(), From, Told);
		Vaelen::View::TakePanel(Frame, Life, Told, Page);
	}

	/// After a command: nothing of the world moved - a Mean only queues - so
	/// the frame, the people and the chronicle are still true, and the life
	/// (its queue) and the page are not.
	void TakeLifeAndPage()
	{
		if (!World)
		{
			return;
		}
		Vaelen::View::TakeLifeView(World->Instance(), World->Sources(), Ways, Life);
		Vaelen::View::TakePanel(Frame, Life, Told, Page);
	}
};

void UVaelenWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// MakePimpl here, where FVaelenHeld is complete: this call is what
	// captures the deleter the header can then use from anywhere.
	Held = MakePimpl<FVaelenHeld>();
}

void UVaelenWorldSubsystem::Deinitialize()
{
	// The door first: it holds a reference to the world.
	if (Held)
	{
		Held->Door.Reset();
		Held->World.Reset();
		Held = nullptr;
	}
	Super::Deinitialize();
}

bool UVaelenWorldSubsystem::Begun() const
{
	return Held && Held->World && Held->World->Begun();
}

bool UVaelenWorldSubsystem::Begin(int32 Size, int32 Years, bool bStreaming)
{
	if (!Held || Held->World)
	{
		return false; // one world per host; Deinitialize is how it ends
	}
	Vaelen::Run::Options Asked;
	Asked.Size = static_cast<Vaelen::uint32>(Size > 0 ? Size : 128);
	Asked.Years = static_cast<Vaelen::uint32>(Years > 0 ? Years : 120);
	Asked.Play = true;
	Asked.Stream = bStreaming;
	Held->World = MakeUnique<Vaelen::Run::Aelvor>(Asked);
	if (!Held->World->Begin())
	{
		Held->World.Reset();
		return false;
	}
	Held->Streaming = bStreaming;
	Vaelen::Player::StartRules Rules;
	Rules.WantBound = 0; // whoever the world offers: a map may hold nobody bound
	Held->Door = MakeUnique<Vaelen::Run::Door>(*Held->World, Rules);
	Held->Door->TakeUp();
	// Once, here: the ground of a begun world does not change, and taking it
	// on a frame would be taking half a megabyte on a frame.
	Vaelen::View::TakeMapView(Held->World->Instance(), Held->World->Sources(), Held->Ground);
	Held->TakeAll();
	return true;
}

int32 UVaelenWorldSubsystem::TakeSomebodyElse()
{
	if (!Held || !Held->Door || !Held->World || !Held->World->Begun())
	{
		return 0;
	}
	// Release first: TakeUp refuses while somebody is played (Aelvor.h), and
	// the release is not recorded because a replay performs it for itself
	// before every TakenUp it applies.
	if (Held->World->Played() != 0)
	{
		Held->World->Release();
	}
	const int32 Who = static_cast<int32>(Held->Door->TakeUp());
	// The life and the page, not the frame: nothing of the world moved, but
	// who is being played did, and both of those read from it.
	Held->TakeLifeAndPage();
	return Who;
}

bool UVaelenWorldSubsystem::Streaming() const
{
	return Held && Held->Streaming;
}

void UVaelenWorldSubsystem::Watch(int32 Region, int32 Reach, int32 Most)
{
	if (!Held)
	{
		return;
	}
	// Clamped where they are read from, not where they are used: a negative
	// reach from a host that divided by something is a host bug, and turning it
	// into four billion borders is this function's bug.
	Held->Eyes.Region = Region > 0 ? static_cast<Vaelen::uint32>(Region) : 0u;
	Held->Eyes.Reach = Reach > 0 ? static_cast<Vaelen::uint32>(Reach) : 0u;
	Held->Eyes.Most = Most > 0 ? static_cast<Vaelen::uint32>(Most) : 0u;
	Held->Watched = true;
}

bool UVaelenWorldSubsystem::Watching(int32& Region, int32& Reach, int32& Most) const
{
	if (!Held || !Held->Watched)
	{
		return false;
	}
	Region = static_cast<int32>(Held->Eyes.Region);
	Reach = static_cast<int32>(Held->Eyes.Reach);
	Most = static_cast<int32>(Held->Eyes.Most);
	return true;
}

int32 UVaelenWorldSubsystem::RegionUnderGround(double GroundX, double GroundY, double TileSize) const
{
	if (!Held || Held->Ground.Width == 0 || Held->Ground.Height == 0 || !(TileSize > 0.0))
	{
		return 0;
	}
	// The inverse of VaelenViewDrawer::PlaceOfTile, which puts tile (X, Y) at
	// ((X - Width/2) * TileSize, (Y - Height/2) * TileSize).
	//
	// FLOOR and not a cast: a cast towards zero folds the two tiles either side
	// of the origin into one, which puts the whole western edge of the map one
	// tile east.
	//
	// AND + 0.5, which the first version of this did not have. PlaceOfTile
	// returns a tile's CENTRE - the slab it places there is a unit mesh scaled
	// to TileSize, so the tile covers the half tile either side of that point.
	// Inverting without the half put every look one column and one row towards
	// the north-west of where the camera was actually over, for every tile of
	// every map. It reads correctly in a log either way, because the answer is
	// always a plausible region.
	const double AtX = GroundX / TileSize + static_cast<double>(Held->Ground.Width) * 0.5 + 0.5;
	const double AtY = GroundY / TileSize + static_cast<double>(Held->Ground.Height) * 0.5 + 0.5;
	// THE BOUNDS FIRST, and then a plain cast. Casting a double to an integer
	// truncates towards zero, which is floor only for a non-negative value -
	// so the negative half is refused here rather than folded onto tile 0, and
	// what survives needs no rounding function at all.
	//
	// It called FMath::FloorToDouble until this line was written down. That is
	// an Unreal API this project had never compiled, on a machine this session
	// cannot reach, in a module whose last two build failures were exactly that
	// kind of assumption. Ordinary arithmetic cannot be wrong about a spelling.
	if (AtX < 0.0 || AtY < 0.0 || AtX >= static_cast<double>(Held->Ground.Width) ||
		AtY >= static_cast<double>(Held->Ground.Height))
	{
		return 0; // off the map, which is a host looking nowhere
	}
	const Vaelen::View::TileView* Tile =
		Vaelen::View::TileIn(Held->Ground, static_cast<Vaelen::uint32>(AtX), static_cast<Vaelen::uint32>(AtY));
	// TileView::Region is 0 on sea and on unassigned land (Land.h), which is
	// the same answer as off the map and means the same thing.
	return Tile != nullptr ? static_cast<int32>(Tile->Region) : 0;
}

void UVaelenWorldSubsystem::AdvanceDay(int32 Days)
{
	if (!Held || !Held->Door)
	{
		return;
	}
	for (int32 i = 0; i < Days; ++i)
	{
		// THE LOOK FIRST, and once per day turn. It goes through the door, so
		// the world's own clock stamps it and the stream carries it; the door
		// is the only way an input reaches the simulation (ADR-0138). A host
		// that has never looked hands nothing over and writes the stream Phase
		// 14 wrote.
		//
		// Before the turn rather than after, because that is the order a person
		// lives in and the order the stream's encoder writes at equal ticks:
		// somebody looks, and then time passes over what they saw.
		//
		// AND NOT AT ALL ON A TICK THAT ALREADY CARRIES A TAKING, which is not
		// tidiness. Door::Day records DayTurned at the tick before the turn and
		// then, if the played person did not survive it, records the TakenUp at
		// the tick AFTER - so the taking lands on the tick this loop's next
		// look would land on, and lands there FIRST, because the host learns
		// the new tick only after the turn.
		//
		// Replay applies looks before takings at equal ticks, deliberately and
		// for a measured reason (Door.cpp). So a host that recorded take-then-
		// look at one tick recorded a world its own replay does not reach.
		// MEASURED, not feared: four hundred days with one automatic taking in
		// them recorded b6ad941689ba6b1a and replayed 37c1a3fe1e4961b2 - a
		// different world - with Wrong = 0 and every taking matching, because
		// nothing in a stream says which of two orders a tick was lived in.
		// One tick of four hundred. Skipping that one look costs a host
		// nothing: it looks again the next day.
		//
		// Door.TheEngineHostsOwnOrderReplaysToItself drives this exact loop
		// headlessly and keeps the divergent arm beside it as the control.
		const Vaelen::Player::InputStream& Tape = Held->Door->Stream();
		const bool TookHere = !Tape.Takings.empty() && Tape.Takings.back().Tick == Held->World->Now();
		if (Held->Watched && !TookHere)
		{
			Held->Door->Look(Held->Eyes);
		}
		Held->Door->Day(); // records DayTurned and turns it
	}
	Held->TakeAll();
}

Vaelen::Player::Refusal UVaelenWorldSubsystem::Mean(const Vaelen::Player::PlayerCommand& What)
{
	if (!Held || !Held->Door)
	{
		return Vaelen::Player::Refusal::NoPlayer;
	}
	const Vaelen::Player::Refusal Answer = Held->Door->Mean(What);
	Held->TakeLifeAndPage();
	return Answer;
}

const Vaelen::View::WorldView& UVaelenWorldSubsystem::World() const
{
	static const Vaelen::View::WorldView Nothing;
	return Held ? Held->Frame : Nothing;
}

const Vaelen::View::PeopleView& UVaelenWorldSubsystem::People() const
{
	static const Vaelen::View::PeopleView Nothing;
	return Held ? Held->Folk : Nothing;
}

const Vaelen::View::LifeView& UVaelenWorldSubsystem::Life() const
{
	static const Vaelen::View::LifeView Nothing;
	return Held ? Held->Life : Nothing;
}

const Vaelen::View::ChronicleView& UVaelenWorldSubsystem::Chronicle() const
{
	static const Vaelen::View::ChronicleView Nothing;
	return Held ? Held->Told : Nothing;
}

const Vaelen::View::PanelView& UVaelenWorldSubsystem::Panel() const
{
	static const Vaelen::View::PanelView Nothing;
	return Held ? Held->Page : Nothing;
}

const Vaelen::Player::InputStream& UVaelenWorldSubsystem::Stream() const
{
	static const Vaelen::Player::InputStream Nothing;
	return Held && Held->Door ? Held->Door->Stream() : Nothing;
}

UVaelenWorldSubsystem::FDigests UVaelenWorldSubsystem::Digests() const
{
	FDigests Out;
	if (!Held || !Held->World)
	{
		return Out;
	}
	Out.State = Held->World->StateDigest();
	Out.Log = Held->World->LogDigest();
	// The life in words, hashed the way Run::Replay hashes it, so that the
	// engine's line and a headless replay's are the same number or the replay
	// failed (Door.cpp).
	const std::string Story = Held->World->Life();
	Out.Life = Vaelen::HashBytes(Story.data(), Story.size());
	Out.Panel = Vaelen::View::MeasurePanel(Held->Page).Digest;
	return Out;
}

bool UVaelenWorldSubsystem::WriteStream(FString& Out)
{
	Out.Reset();
	if (!Held || !Held->World || !Held->Door)
	{
		return false;
	}
	const Vaelen::Player::StreamHeader Head = Held->World->Header();
	// <seed>-<size>.stream: the two things a replay needs to know it is the
	// same world before it reads a single record.
	const FString Name = FString::Printf(TEXT("%012llx-%u.stream"), static_cast<unsigned long long>(Head.Seed),
										 static_cast<unsigned>(Head.Size));
	Out = FPaths::Combine(FPaths::ProjectSavedDir(), FString(TEXT("Vaelen")));
	Out = FPaths::Combine(Out, Name);
	const std::string Text = Vaelen::Player::EncodeStream(Held->Door->Stream());
	return FFileHelper::SaveStringToFile(FString(ANSI_TO_TCHAR(Text.c_str())), *Out);
}
