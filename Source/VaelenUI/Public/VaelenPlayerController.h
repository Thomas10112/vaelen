// VAELEN - VaelenUI
// Phase 14 task 14.09: one key, one press, one intent.
//
// The eight verbs are lettered the way the PAGE letters them: the panel
// carries a Key per verb (14.06) and these bindings are that letter, so the
// screen and the keyboard cannot drift apart. Tab cycles what the next
// Speak, Give, Take or Move is aimed at; Space turns the day, which is the
// one host input that is not a gameplay command (ADR-0138); F9 writes the
// stream.
//
// STATUS: UNVERIFIED (engine) since 19.06 - its code has changed after the last build that
// compiled it (b0921, 15.10, 867a129): RegionTheCameraIsOver is virtual and protected, so the
// walk's controller can answer with the region under the walker's feet. Parsed, never
// compiled; sitting S2 builds it.
// BUILD: b0921 - Tools/engine_builds.txt
//
// UNTIL 19.06: VALIDATED (Phase 14) for what Phase 14 left here - built by
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
#include "GameFramework/PlayerController.h"
#include "Vaelen/Player/Intent.h"

#include "VaelenPlayerController.generated.h"

UCLASS()
class VAELENUI_API AVaelenPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;

private:
	void Work();
	void Rest();
	void Eat();
	void WaitOut();
	void Speak();
	void Give();
	void Take();
	void Move();
	void NextTarget();
	void TurnTheDay();
	void WriteStream();

public:
	/// What the camera is over, as a region, and how far it is worth detailing
	/// around it. Read on every day turn and handed to the subsystem, which
	/// hands it through the door - so where somebody is looking is an INPUT
	/// with a tick on it, and a walk can be replayed from the record alone
	/// (ADR-0143). There is NO Tick in this module and there must not be: a
	/// world that also moved on the frame clock could not be replayed from a
	/// key sequence, so the camera is read when the day turns and at no other
	/// moment.
	///
	/// The three below are the host's drawing scale, and the world knows
	/// nothing of them.

	/// Centimetres to a tile, which must be the TileSize the ground under the
	/// camera was drawn at (AVaelenViewActor::TileSize, 100 by default). Wrong
	/// here and the camera reports the wrong region - the world is none the
	/// wiser, and the walk is a walk over somewhere else.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Look", meta = (ClampMin = "1"))
	float TileSize = 100.0f;

	/// How many tiles of height buy one border of reach. A host's choice with
	/// no meaning to the simulation: it decides how much ground a high camera
	/// asks to have detailed, and nothing else.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Look", meta = (ClampMin = "1"))
	int32 TilesPerBorder = 12;

	/// The most regions the host will pay to have simulated person by person
	/// around what it is looking at. Recorded with every look, because a replay
	/// given a different budget details different regions and therefore has
	/// different people in it (Attention.h).
	UPROPERTY(EditAnywhere, Category = "AELVOR|Look", meta = (ClampMin = "0"))
	int32 MostRegions = 4;

	/// Where the map's own origin sits in the level. PlaceOfTile returns an
	/// offset the VAELEN View actor's transform is then applied to, so a host
	/// that moved that actor must say where: leaving this at zero while the
	/// actor sits elsewhere reads a region from the wrong part of the map, and
	/// reads it quietly, because every answer is a plausible region.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Look")
	FVector MapOrigin = FVector::ZeroVector;

	/// Off, and the day turns without a look: the stream is then the one Phase
	/// 14 wrote. On for a walk - but ONLY where the world can act on it: a
	/// world begun without the daily cadence ignores looks, so recording them
	/// into its stream would change the stream's shape and change nothing else.
	/// TurnTheDay asks the subsystem which cadence it was begun with.
	UPROPERTY(EditAnywhere, Category = "AELVOR|Look")
	bool bLookFromCamera = true;

private:
	/// Where the camera's view ray meets the ground, as a region. 0 when the
	/// camera looks at or above the horizon, when the ray lands off the map and
	/// when it lands on water - three things that all mean the host is looking
	/// nowhere, which is itself an input worth recording.
	///
	/// THE GROUND IS THE PLANE Z = 0, which is sea level and not the land. The
	/// drawer stands relief on top of it, so a camera at a shallow angle over
	/// hill country reports the region its ray reaches at sea level, which can
	/// be a tile or two beyond the peak it appears to be pointed at. Directly
	/// overhead - which is how AELVOR is looked at - the error is nothing. It
	/// is recorded here rather than fixed because a look is a host's opinion
	/// about where it is looking: whichever region it names, the world records
	/// that one and the replay reproduces it, so this costs accuracy and never
	/// determinism.
	///
	/// NOT const, and deliberately: APlayerController::GetPlayerViewPoint is
	/// what this reads the camera with, and whether THAT is const in UE 5.6 is
	/// a thing this session believes rather than knows. A non-const helper
	/// compiles either way, and its one caller (TurnTheDay) is non-const.
	///
	/// Virtual since 19.06: the walk's controller answers with the region
	/// under the walker's feet, and the day turn asks the same question.
protected:
	virtual int32 RegionTheCameraIsOver(int32& OutReach);

	/// 19.11: what a verb is aimed at, given what Tab would aim it at. The
	/// base answers Tab's target; the walk answers the region ahead at the
	/// fence for Move and the figure in front for Speak, Give and Take, and
	/// falls back to Tab's when there is none (ROADMAP 19.11).
	virtual uint32 TargetFor(Vaelen::Player::Intent Kind, uint32 TabTarget) { return TabTarget; }
	/// 19.11: after the day turned and the views were retaken. The base does
	/// nothing; the walk puts the walker back inside its region and says its
	/// one line of the day.
	virtual void AfterTheDay(int32 Looked) { (void)Looked; }
	/// 19.12: after the stream was written. The base does nothing; the walk
	/// says what the days on foot came to.
	virtual void AfterStreamWritten() {}

private:
	/// Through the page and then through the door, and nowhere else. The page
	/// answers first - an unoffered verb costs the world nothing - and what it
	/// offers becomes a PlayerCommand the module hands to Mean.
	void Verb(Vaelen::Player::Intent Kind);

	/// What Speak, Give, Take and Move are aimed at: an index into the page's
	/// company for the three that need a person, and into its neighbours for
	/// the one that needs a place. Tab moves it; nothing else does.
	uint32 Aim = 0;
};
