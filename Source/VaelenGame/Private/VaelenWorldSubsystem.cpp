// VAELEN - VaelenGame
// Phase 14 task 14.08: the one place a world is held.
//
// This file may name a World, a Run and a Take, and it is the only file of
// this module that may: the public header holds a pointer to FVaelenHeld and
// nothing else, so the UI that includes it has no vocabulary for the
// simulation. Tools/check_ui_fence.py reads Public and deliberately not
// Private, which is that seam written down.
//
// STATUS: VALIDATED (Phase 14) - built by UnrealBuildTool and RUN on
// 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor): eighty-three days
// played at the keyboard, and Tools/Atlas replayed the stream headlessly to the
// same four digests, byte for byte. Tests/Run/Streams/README.md has the lines.
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

bool UVaelenWorldSubsystem::Begin(int32 Size, int32 Years)
{
	if (!Held || Held->World)
	{
		return false; // one world per host; Deinitialize is how it ends
	}
	Vaelen::Run::Options Asked;
	Asked.Size = static_cast<Vaelen::uint32>(Size > 0 ? Size : 128);
	Asked.Years = static_cast<Vaelen::uint32>(Years > 0 ? Years : 120);
	Asked.Play = true;
	Held->World = MakeUnique<Vaelen::Run::Aelvor>(Asked);
	if (!Held->World->Begin())
	{
		Held->World.Reset();
		return false;
	}
	Vaelen::Player::StartRules Rules;
	Rules.WantBound = 0; // whoever the world offers: a map may hold nobody bound
	Held->Door = MakeUnique<Vaelen::Run::Door>(*Held->World, Rules);
	Held->Door->TakeUp();
	Held->TakeAll();
	return true;
}

void UVaelenWorldSubsystem::AdvanceDay(int32 Days)
{
	if (!Held || !Held->Door)
	{
		return;
	}
	for (int32 i = 0; i < Days; ++i)
	{
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
