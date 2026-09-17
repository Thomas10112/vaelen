// VAELEN - VaelenRun
// Phase 14 task 14.03: the door of a played world, and the replay of what
// came through it.
//
// STATUS: PROTOTYPE (Phase 14) - Tests/Run/Test_Door.cpp
#include "Vaelen/Run/Door.h"

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
		Tape.Looks.push_back(Player::Looked{W.Now(), At.Region, At.Reach});
		W.LookAt(At);
	}

	ReplayReport Replay(Aelvor& Fresh, const Player::InputStream& S, const Player::StartRules& Rules)
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
		// tick order with everything else. Most is not in the record - it is
		// the host's configuration - so a replay gives the Rules' own, which is
		// the same argument SameWorld makes about the header.
		const auto LookDue = [&]()
		{
			while (Saw < S.Looks.size() && S.Looks[Saw].Tick <= Fresh.Now())
			{
				Fresh.LookAt(Attention{S.Looks[Saw].Region, S.Looks[Saw].Reach, 0});
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
		for (usize d = 0; d < S.Days.size(); ++d)
		{
			LookDue();
			SubmitDue();
			Fresh.Day();
			++R.Days;
			TakeUpDue();
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
