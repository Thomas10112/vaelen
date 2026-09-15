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
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, not yet built by UnrealBuildTool
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
	bool Begin(int32 Size, int32 Years);
	bool Begun() const;

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
