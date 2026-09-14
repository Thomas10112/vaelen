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
		uint32 Days = 0;	 ///< day turns made - exactly the DayTurned records
		uint32 Takings = 0;	 ///< takings applied
		uint32 Left = 0;	 ///< records on ticks the days never reached
		uint32 Refused = 0;	 ///< 1 when nothing was replayed: another world, no Play, or not Begun
		uint32 ByKind[Player::IntentCount] = {}; ///< commands by Intent; an unknown kind counts under None
		Hash64 State = 0;
		Hash64 Log = 0;
		Hash64 Life = 0;
	};

	/// The stream into a fresh world. Bounded by the records, not by a
	/// lifetime: for each DayTurned in order, every Recorded due is submitted
	/// (Tick <= Now, as Test_PlayerGate does), the day is turned, and every
	/// TakenUp due is taken up - releasing whoever was played, so a replay
	/// never has to know why a life ended. What was recorded after the last
	/// day turn is submitted last. Fresh must be Begun, with Play.
	VAELEN_RUN_API ReplayReport Replay(Aelvor& Fresh, const Player::InputStream& S,
									   const Player::StartRules& Rules = Player::StartRules{});
} // namespace Vaelen::Run
