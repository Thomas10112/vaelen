// VAELEN - VaelenRun
// Phase 14 task 14.03: the door of a played world, and the replay of what
// came through it.
//
// STATUS: PROTOTYPE (Phase 14) - Tests/Run/Test_Door.cpp
#include "Vaelen/Run/Door.h"

#include <utility>

#include <string>

namespace Vaelen::Run
{
	using Player::DayTurned;
	using Player::Recorded;
	using Player::Refusal;
	using Player::TakenUp;

	Door::Door(Aelvor& InWorld, const Player::StartRules& InRules) : W(InWorld), Start(InRules)
	{
		Tape.Header = W.Header();
	}

	Door::Door(Aelvor& InWorld, const Player::StartRules& InRules, Player::InputStream InTape)
		: W(InWorld), Start(InRules), Tape(std::move(InTape))
	{
		// THREE CASES, AND THE FIRST VERSION OF THIS HAD ONLY ONE.
		//
		// It stamped the world's header onto the tape unconditionally, which is
		// right when the two already agree and is a SILENT ERASURE when they do
		// not: Player::SameWorld(Fresh.Header(), S.Header) is the only guard a
		// later Replay has that a stream belongs to its world, and relabelling
		// the tape disarms it. The comment justified the stamp by saying the
		// world "has just been checked, field by field, against the one the
		// checkpoint declared" - true of Adopt's HOST comparison, but nothing
		// on this path compares the tape's OWN header to anything, and the
		// Begin() + LoadSnapshot restore route has no HOST check at all.
		// Found by the Phase 16 adversarial review.
		const Player::StreamHeader Mine = W.Header();
		if (Tape.Header.Size == 0u)
		{
			// A tape that never recorded names no world - a real one always has
			// a side in tiles. Nothing is being overwritten.
			Tape.Header = Mine;
		}
		else if (Player::SameWorld(Tape.Header, Mine))
		{
			// They agree, so take the VERIFIED copy rather than the one that
			// travelled with the records. That is 16.10's reasoning and it
			// stands - it just does not license the case below.
			Tape.Header = Mine;
		}
		else
		{
			// They do NOT agree. Leave the tape's own header exactly as it came
			// so that Replay refuses it, and say so out loud: a host that built
			// this Door has a tape from another world and needs to know before
			// it records one more day into it.
			Foreign_ = true;
		}
	}

	uint32 Door::TakeUp()
	{
		const uint32 Who = W.TakeUp(Start);
		if (Who != 0)
		{
			Tape.Takings.push_back(TakenUp{W.Now(), Who, 0});
		}
		return Who;
	}

	Refusal Door::Mean(Player::PlayerCommand C)
	{
		// The world's clock, whatever the caller wrote: the one place a tick
		// is put on a command, so that no wall clock and no frame count can be.
		C.Issued = W.Now();
		const Refusal Verdict = W.Submit(C);
		// NoPlayer touched nothing (Player::Submit answers it before any pool is
		// written), so it is not a record: on one tick the text form and Replay
		// put takings before commands, and a NoPlayer command recorded before a
		// same-tick taking would be replayed AFTER it - accepted, and executed,
		// where the original refused it. Found by review before it shipped.
		if (Verdict != Refusal::NoPlayer)
		{
			Tape.Commands.push_back(Recorded{W.Now(), C, Verdict, {}});
		}
		return Verdict;
	}

	uint64 Door::Day()
	{
		// Recorded AT the tick it is turned on: the commands of this tick come
		// before it in the text form, and the takings of the next tick after.
		Tape.Days.push_back(DayTurned{W.Now()});
		const uint64 After = W.Day();
		if (W.Given().Play && W.Played() != 0 && !W.PlayedAlive())
		{
			W.Release();
			TakeUp();
		}
		return After;
	}

	void Door::Look(const Attention& At)
	{
		// A look before the world exists is not a record. Aelvor::LookAt does
		// nothing without Begun_, so recording one would write an input that did
		// nothing when it happened and something when it was replayed - the
		// replay's world IS begun by the time LookDue reaches it. Found by the
		// Phase 15 review.
		if (!W.Begun())
		{
			return;
		}
		Tape.Looks.push_back(Player::Looked{W.Now(), At.Region, At.Reach, At.Most, 0});
		W.LookAt(At);
	}

	ReplayReport Replay(Aelvor& Fresh, const Player::InputStream& S, const Player::StartRules& Rules,
						const DayWatch& Watching)
	{
		ReplayReport R;
		if (!Fresh.Begun() || !Fresh.Given().Play || !Player::SameWorld(Fresh.Header(), S.Header))
		{
			R.Refused = 1;
			return R;
		}
		usize Next = 0;
		usize Took = 0;
		usize Saw = 0;
		// 15.06: the looks go back through the same door they came out of, in
		// tick order with everything else - and with the budget they were made
		// under. The first version dropped Most on the floor here and put a 0
		// in its place, which Reside reads as "the world's own limit", so any
		// host that paid for a different number of regions recorded a stream
		// this function could not reproduce - silently, with Wrong = 0. The
		// Phase 15 review measured the divergence. Most is an INPUT: it rides
		// on every look and it decides which regions are requested, hence which
		// are promoted, hence who exists.
		const auto LookDue = [&]()
		{
			while (Saw < S.Looks.size() && S.Looks[Saw].Tick <= Fresh.Now())
			{
				Fresh.LookAt(Attention{S.Looks[Saw].Region, S.Looks[Saw].Reach, S.Looks[Saw].Most});
				++R.Looks;
				++Saw;
			}
		};
		// Whoever the record says was taken up, on the tick it says - releasing
		// whoever was played, so a replay never has to know why a life ended.
		const auto TakeUpDue = [&]()
		{
			while (Took < S.Takings.size() && S.Takings[Took].Tick <= Fresh.Now())
			{
				if (Fresh.Played() != 0)
				{
					Fresh.Release();
				}
				const uint32 Who = Fresh.TakeUp(Rules);
				const bool Same = Who == S.Takings[Took].Person;
				R.Wrong += Same ? 0u : 1u;
				R.WrongTakings += Same ? 0u : 1u;
				++R.Takings;
				++Took;
			}
		};
		const auto SubmitDue = [&]()
		{
			while (Next < S.Commands.size() && S.Commands[Next].Tick <= Fresh.Now())
			{
				const Recorded& Rec = S.Commands[Next];
				const Refusal Verdict = Fresh.Submit(Rec.Command);
				R.Wrong += Verdict == Rec.Verdict ? 0u : 1u;
				const usize Kind = Rec.Command.Kind < Player::IntentCount ? Rec.Command.Kind : 0u;
				++R.ByKind[Kind];
				++R.Answered;
				++Next;
			}
		};
		LookDue();
		TakeUpDue();
		// Before the first turn, and after what was due before it: this is the
		// world a host had when it took somebody up, and the only moment a
		// watcher can see it. See DayWatch::Begun.
		if (Watching.Begun != nullptr)
		{
			Watching.Begun(Fresh, Watching.User);
		}
		for (usize d = 0; d < S.Days.size(); ++d)
		{
			SubmitDue();
			Fresh.Day();
			++R.Days;
			// LOOKS BEFORE TAKINGS at the tick a day turn lands on, because
			// that is the order EncodeStream writes at equal ticks and the
			// order a host lives in: somebody looks, and then acts on what they
			// saw. This ran TakeUpDue alone here, so every taking recorded at
			// the same tick as a look was applied BEFORE it - and a look
			// changes what is detailed while a taking chooses among the
			// detailed. Three takings of four picked a different person; the
			// walk that found it was rewritten to look first, which papered
			// over the defect instead of fixing it. The Phase 15 review found
			// it under the paper.
			LookDue();
			TakeUpDue();
			// The end of the day, and the same point on every one of them: the
			// day has turned, the looks and takings recorded at the tick it
			// landed on have been applied, and nothing else will touch the
			// world until the next iteration. A watcher that read the world
			// between Day() and LookDue() would be reading a day that is not
			// over, and would report a promotion this walk never had.
			if (Watching.After != nullptr)
			{
				Watching.After(Fresh, static_cast<uint32>(d), Watching.User);
			}
		}
		// What was recorded after the last day turn, on the tick it is now.
		LookDue();
		SubmitDue();
		TakeUpDue();
		R.Left = static_cast<uint32>((S.Commands.size() - Next) + (S.Takings.size() - Took) + (S.Looks.size() - Saw));
		R.State = Fresh.StateDigest();
		R.Log = Fresh.LogDigest();
		const std::string Story = Fresh.Life();
		R.Life = HashBytes(Story.data(), Story.size());
		return R;
	}
} // namespace Vaelen::Run
