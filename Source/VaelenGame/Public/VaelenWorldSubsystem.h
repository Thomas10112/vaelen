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
// STATUS: UNVERIFIED (engine) since 19.01's ledger - its code has changed after the last build
// that compiled it (b0921, 15.10, 867a129): 16.14's save, load and store (c146c43), 19.10's
// keys. Parsed
// against Tools/EngineShim, never compiled. Tools/check_engine_status.py holds this line to
// Tools/engine_builds.txt; the record of what earlier builds validated follows.
// BUILD: b0921
//
// UNTIL 19.01: VALIDATED (Phase 14) for what Phase 14 left here - built by
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
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/View/Climate.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Net.h"

#include "VaelenWorldSubsystem.generated.h"

/// Held in the .cpp, where the World may be named.
struct FVaelenHeld;

/// 19.06: fired after every retaking of the views - Begin, AdvanceDay, Load -
/// and never on a frame. What the walk repaints on (ADR-0138: the scene moves
/// when the world does, and the world moves on a day turn).
DECLARE_MULTICAST_DELEGATE(FVaelenViewsTaken);

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
	/// Lets the played person go and takes somebody else up, recorded. The
	/// person taken, or 0 when the world offers nobody.
	///
	/// THIS IS A VERB THE HOST WAS MISSING, and 15.10's gate is what found it.
	/// The gate wants at least four takings over a walk, and the door takes
	/// somebody up on its own only when the played person dies - which,
	/// measured rather than assumed, does not happen: four thousand day turns
	/// at AELVOR 128, eleven years of play, and the first person taken up was
	/// still alive at the end. A host with no way to let go could not record a
	/// second taking, so the clause was unmeetable at the keyboard for want of
	/// a verb rather than for want of a world.
	///
	/// A release is not itself a record: Run::Replay releases whoever is played
	/// before applying any TakenUp, so the record of the taking is the whole
	/// of it. That is the same thing Tools/Atlas --walk does between lives.
	int32 TakeSomebodyElse();

	void Watch(int32 Region, int32 Reach, int32 Most);
	/// What Watch was last given. False, and the three untouched, when it never
	/// was - which is a different thing from looking at region 0.
	bool Watching(int32& Region, int32& Reach, int32& Most) const;

	/// The region under a point on the ground, where the ground is drawn the
	/// way VaelenViewDrawer::PlaceOfTile draws it. 0 when no world is begun,
	/// when the point is off the map, and when the tile there is sea - three
	/// different things that all mean "no region here", and all three are a
	/// host looking nowhere.
	///
	/// The host converts, not the world: this takes the units the host draws
	/// in because the host is the only thing that knows them, and hands back a
	/// region index, which is the only thing the simulation knows.
	///
	/// TWO THINGS IT ASSUMES, both of them the drawer's own conventions, and
	/// both of them the caller's to honour:
	///
	/// - The point is in the MAP'S space, not the level's. PlaceOfTile returns
	///   an offset that the view actor's transform is then applied to, so a
	///   host whose actor is not at the origin subtracts its location first.
	///   A caller that forgets reads a region from somewhere else on the map -
	///   quietly, because every answer is a plausible region.
	/// - PlaceOfTile returns a tile's CENTRE, not its corner: the slab it
	///   places is a unit mesh scaled to TileSize, so tile X covers the half
	///   tile either side of it. The inverse below adds that half back.
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
	/// 19.06: what the walk reads. The map, taken ONCE at Begin (Land.h says
	/// it is not a per-frame structure); the scene's ground, cut from it once
	/// in integers (Terrain.h, ADR-0156); the climate and the net, retaken with
	/// the other views. Empty before Begin.
	const Vaelen::View::MapView& Ground() const;
	const Vaelen::Scene::Ground& Scene() const;
	const Vaelen::View::ClimateView& Climate() const;
	const Vaelen::View::NetView& Net() const;
	/// The world's seed and size, for the lines the walk prints in the Atlas's
	/// words. 0 before Begin.
	uint64 Seed() const;
	int32 Size() const;
	/// 19.06: fired after every retaking of the views. See FVaelenViewsTaken.
	FVaelenViewsTaken OnViewsTaken;
	/// 19.06: LogVaelenClimate, composed by Vaelen/View/Proof.h from facts this
	/// module reads off the world (the winters and the dead of the cold are the
	/// log's) - the bytes `VaelenAtlas` prints for the same world on the same
	/// day, so a sitting compares them byte for byte. Empty without a climate
	/// or before Begin.
	FString ClimateLine() const;
	/// 19.10 (ADR-0158): the letters that press the eight verbs - the host's
	/// table, which every page this host composes prints and every key the
	/// controller binds comes from. DefaultKeys until 19.06 puts ZQSD on the
	/// keyboard (then WalkKeys, Speak on F); a table that is not the default
	/// is said on the `check it headless` line as `--keys`, told, not read.
	const Vaelen::View::PanelKeys& Keys() const;
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

	/// 16.14: the world, its run and its tape as ONE container under
	/// Saved/Vaelen/<name>, through a store that cannot destroy the last good
	/// save of that name (a write goes to `<name>.writing` and is moved into
	/// place whole). True with the path in Out and, in OutCheck, the headless
	/// command that must come back to the same state digest; false with the
	/// reason in Out - no world, a name the store refuses
	/// (Run::IsUsableCheckpointName), the container or the write refused, each
	/// by its own name.
	bool Save(const FString& Name, FString& Out, FString& OutCheck);
	/// 16.14: takes a saved world up INTO A FRESH HOST, never over a begun one
	/// (one world per host, as Begin says; Run::Aelvor::Adopt refuses it too,
	/// as AlreadyBegun). The world is built from the container's own HOST
	/// section, not from anything typed, then adopted; the tape the save
	/// carries is handed back to the door, which goes on recording into it.
	/// On any refusal nothing is held afterwards, as before the call, and Out
	/// says why: the store's, the container's or Adopt's reason, by name.
	bool Load(const FString& Name, FString& Out, FString& OutCheck);
	/// 16.14: one line per save under Saved/Vaelen, for the console and for a
	/// chooser: name, bytes, tick, container version, sections, state digest.
	/// Empty when the folder is not there yet. Returns how many.
	int32 Saves(TArray<FString>& Out);

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
	/// The host's verb keys (19.10). Here and not in Held: the controller binds
	/// them at SetupInputComponent, before any world is begun.
	Vaelen::View::PanelKeys Keys_ = Vaelen::View::DefaultKeys;
};
