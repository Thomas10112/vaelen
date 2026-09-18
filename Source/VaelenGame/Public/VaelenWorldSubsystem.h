// VAELEN - VaelenGame
// Phase 14 task 14.08: the one place a world is held.
//
// A host has exactly one of these. It owns Run::Aelvor, Run::Door and the
// region graph cache - behind a pimpl, so that this header names none of them
// and the UI that includes it cannot either. What comes out is views: the
// frame, the people, the life, the chronicle and the page of 14.06, retaken
// when and only when something moved.
//
// NO Tick. The world moves on AdvanceDay and on nothing else, which is
// ADR-0138 in one sentence: the day turn is the one host input that is not a
// gameplay command, it is recorded in the stream as DayTurned, and a world
// that also moved on the frame clock could not be replayed from a key
// sequence. Paused unless asked.
//
// Tools/check_ui_fence.py reads this header with the UI's own: every include
// below is a view leaf, the command surface or Core, and nothing here names a
// World, a Run or a Take.
//
// STATUS: VALIDATED (Phase 14) - built by UnrealBuildTool and RUN on
// 2026-09-16 (UE 5.6, MSVC 19.51, Win64 Development Editor): eighty-three days
// played at the keyboard, and Tools/Atlas replayed the stream headlessly to the
// same four digests, byte for byte. Tests/Run/Streams/README.md has the lines.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/Player/Stream.h"
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Folk.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Panel.h"

#include "VaelenWorldSubsystem.generated.h"

/// Held in the .cpp, where the World may be named.
struct FVaelenHeld;

UCLASS()
class VAELENGAME_API UVaelenWorldSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/// Generates AELVOR at Size tiles a side, runs 300 years of pre-history
	/// and Years of the world, takes somebody up and takes the first views.
	/// False when a world is already begun or the map cannot be generated.
	/// Seconds of work, not a frame's worth: a host calls it from a command.
	///
	/// bStreaming asks for Phase 15's cadence: what is detailed is decided
	/// every DAY rather than every year, and where the host is looking is one
	/// of the things that decides it. It is OFF by default and that is not
	/// timidity - a world with it on is a different world from the same seed,
	/// and every digest this project froze belongs to a world without it. A
	/// walk for the 15.10 gate is recorded with it ON; anything compared
	/// against Phase 14's numbers is recorded with it off.
	bool Begin(int32 Size, int32 Years, bool bStreaming = false);
	bool Begun() const;
	/// Whether the world that is begun was begun with the streaming cadence.
	/// A replay must be told this (VaelenAtlas --stream), so a host that does
	/// not say which it used has written a stream nobody can replay.
	bool Streaming() const;

	/// Where the host is looking: a region of the world, how many borders out
	/// the eye reaches, and the most regions it will pay to have detailed.
	///
	/// NOTHING OF THE WORLD MOVES HERE. This is remembered, and AdvanceDay
	/// hands it through the door once per day turn - which is where it becomes
	/// an input the world's own clock stamps, as a key press is (ADR-0138,
	/// ADR-0143). A camera that handed a look over whenever it moved would put
	/// many records on one tick and none on the next; one per day turn is the
	/// cadence the walk of 15.10 is recorded and replayed at.
	///
	/// Region 0 is a host looking NOWHERE, and is as much an input as a place:
	/// the camera leaving is something the world is entitled to know. Until
	/// this is called the host is not looking at all and no look is recorded,
	/// which is exactly the stream Phase 14 wrote.
	void Watch(int32 Region, int32 Reach, int32 Most);
	/// What Watch was last given. False, and the three untouched, when it never
	/// was - which is a different thing from looking at region 0.
	bool Watching(int32& Region, int32& Reach, int32& Most) const;

	/// The region under a point on the ground, where the ground is drawn the
	/// way VaelenViewDrawer::PlaceOfTile draws it: the map centred on the
	/// origin, TileSize centimetres to a tile, north-west at the lowest
	/// coordinates. 0 when no world is begun, when the point is off the map,
	/// and when the tile there is sea - three different things that all mean
	/// "no region here", and all three are a host looking nowhere.
	///
	/// The host converts, not the world: this takes the units the host draws
	/// in because the host is the only thing that knows them, and hands back a
	/// region index, which is the only thing the simulation knows.
	int32 RegionUnderGround(double GroundX, double GroundY, double TileSize) const;

	/// Days day-turns, each recorded as a DayTurned, then every view retaken.
	/// Nothing when no world is begun.
	void AdvanceDay(int32 Days);

	/// One intent through the door, which stamps it with the world's clock and
	/// records it. The life and the page are retaken after every call, so a
	/// refusal is never read off a stale page. What comes back is the DOOR's
	/// answer - None means QUEUED, not done; the world's own refusals arrive
	/// later and are counted on the queue (Life().Refused).
	Vaelen::Player::Refusal Mean(const Vaelen::Player::PlayerCommand& What);

	const Vaelen::View::WorldView& World() const;
	const Vaelen::View::PeopleView& People() const;
	const Vaelen::View::LifeView& Life() const;
	const Vaelen::View::ChronicleView& Chronicle() const;
	const Vaelen::View::PanelView& Panel() const;
	/// Everything the host has done, in the order it did it (14.01).
	const Vaelen::Player::InputStream& Stream() const;

	/// What a replay is compared by (Run::ReplayReport), and the page's own.
	/// Numbers, not a world: the console command that prints them, and 14.10's
	/// checker that reads what it printed, need no more than this.
	struct FDigests
	{
		Vaelen::Hash64 State = 0;
		Vaelen::Hash64 Log = 0;
		Vaelen::Hash64 Life = 0;  ///< of the life in words (ExportLife)
		Vaelen::Hash64 Panel = 0; ///< MeasurePanel().Digest, the page's last row
	};
	FDigests Digests() const;

	/// Writes the stream to Saved/Vaelen/<seed>-<size>.stream. False when
	/// there is no world or the file cannot be written; the path it tried is
	/// in Out either way.
	bool WriteStream(FString& Out);

private:
	// TPimplPtr and NOT TUniquePtr, and this is not a preference.
	//
	// TUniquePtr deletes through the type. UnrealHeaderTool writes code that
	// destroys this member in VaelenWorldSubsystem.gen.cpp, a translation unit
	// that has seen only the forward declaration above - not just the generated
	// destructor, which declaring one suppresses, but
	// DEFINE_VTABLE_PTR_HELPER_CTOR, which is emitted WHATEVER the class
	// declares. That is the one the owner's build stopped on:
	//
	//   error C4150: deletion of pointer to incomplete type 'FVaelenHeld'
	//     ... in UVaelenWorldSubsystem::UVaelenWorldSubsystem
	//     DEFINE_VTABLE_PTR_HELPER_CTOR(UVaelenWorldSubsystem);
	//
	// TPimplPtr binds its deleter at CONSTRUCTION, where FVaelenHeld is whole,
	// and calls it through a pointer afterwards, so no generated translation
	// unit ever needs the definition. It is the type Epic wrote for exactly
	// this, and there is no path it leaves open.
	TPimplPtr<FVaelenHeld> Held;
};
