// VAELEN - VaelenRun
// Phase 14 task 14.03: the door of a played world, and the replay of what
// came through it.
//
// Two inputs reach the simulation from a host and no other: what the played
// person MEANS (a PlayerCommand, through Mean) and that TIME MOVES (a day
// turned, through Day). Both are recorded here, in the stream of 14.01, so
// that the whole of a played life is a function of what came through this
// door - and Replay() is the proof: a fresh world of the same seed, the same
// records on the same ticks, and the same digests at the end. ADR-0136 is the
// stream, ADR-0138 is the reading of the layering rule that makes the day
// turn a recorded input rather than a key.
//
// `Issued` is stamped here, by the world's clock, whatever the caller wrote:
// no wall clock and no frame count ever enters the simulation.
//
// A command answered NoPlayer is not recorded: the world was untouched, and
// on one tick takings come before commands in the text form and in Replay, so
// recording it would let a replay execute what the original refused.
//
// What a replay cannot know: a played person who died with nobody left to
// take up. The Door releases them and records nothing, because there is
// nothing to record; the replay keeps the dead mark and the Phase 10
// components the original released, so the two state digests differ by
// those components from that day on. Named here rather than hidden, and a
// record kind for it is a later task if a stream ever meets it.
//
// STATUS: PROTOTYPE (Phase 14) - Tests/Run/Test_Door.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Player/Stream.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Attention.h"
#include "Vaelen/Run/RunApi.h"

namespace Vaelen::Run
{
	class VAELEN_RUN_API Door
	{
	public:
		/// The rules are the host's configuration, not an input: a replay must
		/// be given the same ones, and the tests are.
		explicit Door(Aelvor& InWorld, const Player::StartRules& InRules = Player::StartRules{});

		/// Takes somebody up now and records it. 0 when the world offers
		/// nobody or somebody is already played.
		uint32 TakeUp();
		/// Stamps Issued with the world's clock, submits, records the answer -
		/// unless it is NoPlayer, which touched nothing and is not a record.
		Player::Refusal Mean(Player::PlayerCommand C);
		/// Records where the host is looking, stamped with the world's clock
		/// exactly as Mean stamps Issued - a host that passes its own tick is
		/// passing a number the replay would have to trust (ADR-0138). All
		/// three fields of the Attention are recorded, Most included: see
		/// Attention.h for why calling it configuration was wrong.
		///
		/// 15.06 records the look and hands it to the world; what the world
		/// does with it is 15.07's warden. A look at region 0 is a host looking
		/// nowhere and is recorded as such, because "the camera left" is as
		/// much an input as where it went.
		void Look(const Attention& At);

		/// Records DayTurned{Now()} and turns the day. When the played person
		/// is no longer alive afterwards they are released and another is
		/// taken up, recorded; returns the tick it is now.
		uint64 Day();

		const Player::InputStream& Stream() const noexcept { return Tape; }
		uint32 Days() const noexcept { return static_cast<uint32>(Tape.Days.size()); }
		Aelvor& Of() noexcept { return W; }
		const Player::StartRules& Rules() const noexcept { return Start; }

	private:
		Aelvor& W;
		Player::StartRules Start;
		Player::InputStream Tape;
	};

	/// What a replay came to.
	struct ReplayReport
	{
		uint32 Answered = 0; ///< commands submitted
		uint32 Wrong = 0;	 ///< answers that differed from the record, persons that were not the recorded one
		/// Of Wrong, the ones that were a TAKING and not an answer. Kept apart
		/// because Answered - Wrong is what a replay prints as "answered
		/// identically", and a walk with no intents and three mismatched
		/// takings made that underflow to 4294967293 - a number that read like
		/// corruption and was arithmetic. 15.10 found it.
		uint32 WrongTakings = 0;
		uint32 Days = 0;						 ///< day turns made - exactly the DayTurned records
		uint32 Takings = 0;						 ///< takings applied
		uint32 Looks = 0;						 ///< looks put back through the door (15.06)
		uint32 Left = 0;						 ///< records on ticks the days never reached
		uint32 Refused = 0;						 ///< 1 when nothing was replayed: another world, no Play, or not Begun
		uint32 ByKind[Player::IntentCount] = {}; ///< commands by Intent; an unknown kind counts under None
		Hash64 State = 0;
		Hash64 Log = 0;
		Hash64 Life = 0;
	};

	/// A reader called at the end of each replayed day, with the world as it
	/// then stands and the day's own number from 0.
	///
	/// Here because the gate of 15.10 asks questions about EVERY day of a
	/// walk - how many regions one day turn promoted, how many days after a
	/// taking somewhere to walk appeared - and the only other way to ask them
	/// is to replay the stream a second time in the caller, by hand. A second
	/// replay is the one thing a replay must not have: it would drift from
	/// this one, and the drift would look like a property of the walk.
	///
	/// It is handed a const world and its answer is never read. Nothing it
	/// does can change what the replay comes to, which is what lets the same
	/// walk be replayed with a watcher and without one and reach the same
	/// digests - Tests/Run/Test_Door.cpp checks exactly that.
	struct DayWatch
	{
		void (*After)(const Aelvor& W, uint32 Day, void* User) = nullptr;
		void* User = nullptr;
	};

	/// The stream into a fresh world. Bounded by the records, not by a
	/// lifetime: for each DayTurned in order, every Recorded due is submitted
	/// (Tick <= Now, as Test_PlayerGate does), the day is turned, and every
	/// TakenUp due is taken up - releasing whoever was played, so a replay
	/// never has to know why a life ended. What was recorded after the last
	/// day turn is submitted last. Fresh must be Begun, with Play.
	VAELEN_RUN_API ReplayReport Replay(Aelvor& Fresh, const Player::InputStream& S,
									   const Player::StartRules& Rules = Player::StartRules{},
									   const DayWatch& Watching = DayWatch{});
} // namespace Vaelen::Run
