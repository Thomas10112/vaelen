// VAELEN - VaelenRun tests
// Phase 16 task 16.12: the save-point matrix, shared by the test and its
// control.
//
// IT IS A HEADER SO THAT THE TWO CANNOT DRIFT APART. The row asks the control
// to run "the identical matrix" with the RUN section withheld; a control that
// quietly walks a different horizon, or a different set of wirings, from the
// test it is controlling is not a control at all - it is a second test with a
// similar name. Sharing the scaffolding makes "identical" a property of the
// build rather than of somebody remembering to edit both files.
//
// STATUS: PROTOTYPE (Phase 16) - Tests/Run/Test_SaveContinue.cpp,
// Tests/Run/Test_SaveWithheld.cpp
#pragma once

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Attention.h"

#include <string>

namespace
{
	// Inside the anonymous namespace, so that including this header does not
	// open Vaelen into the including file's own scope.
	//
	// EVERYTHING DECLARED HERE MUST BE USED BY BOTH INCLUDING FILES. The
	// build is -Wunused-function -Werror, and a helper that only one of the
	// two calls does not warn in that file - it breaks the other one. That is
	// a blunt fence, and it happens to enforce the point of the header: a
	// scaffold the control does not use is a scaffold the control is not
	// sharing.
	using namespace Vaelen;
	using namespace Vaelen::Run;

	/// The horizon, and where along it the saves are taken. Late is not the
	/// last day: a save taken on the final day is never CONTINUED, and
	/// continuing is the whole claim.
	constexpr uint32 Horizon = 30u;
	constexpr uint32 SavePoints[] = {5u, 15u, 25u};

	constexpr uint32 Sizes[] = {64u, 128u};
	constexpr usize SizeCount = sizeof(Sizes) / sizeof(Sizes[0]);

	/// The records, as a function of the day and nothing else. Both the
	/// straight run and every restored one walk this same script, so "the SAME
	/// remaining records" is a property of the code rather than a promise in a
	/// comment.
	Attention BeatOf(uint32 Day)
	{
		Attention At;
		At.Region = static_cast<uint32>(1u + (Day % 7u));
		At.Reach = 1u;
		At.Most = 0u;
		return At;
	}

	/// WHAT Aelvor::Life() IS WORTH IN A CELL, which is not the same question
	/// as whether the cell compares it.
	///
	/// Found by the Phase 16 adversarial review, and measured worse than the
	/// review said. The matrix compares three digests per cell and its header
	/// claimed three arms of coverage. The life arm did not have it:
	///
	///   None    Aelvor::Life() is the empty string, so the cell hashes
	///           nullptr/0 and compares that against itself. Options::Play is
	///           false, and no restore can change that.
	///   Fixed   Life() is non-empty but IDENTICAL at every save point and at
	///           the horizon. It is settled when the person is taken up and
	///           never moves again, so any restore that carries the snapshot
	///           lands on it - the arm cannot see the days it exists to check.
	///   Moving  Life() at the EARLIEST save point differs from Life() at the
	///           horizon. Only here is "land on the same life digest" a claim
	///           about what the restored world did after the save.
	///
	/// The review counted nine vacuous cells of eighteen. Measuring found
	/// twelve: six None (the plain row) and six more Fixed - the "stream, no
	/// lively" row, whose chronicle is written at TakeUp and then stands
	/// still at both sizes. Only six of eighteen were Moving, and all six sat
	/// in the Stream rows. The whole agreeing half of the control - the cells
	/// that must NOT part when the run is withheld - had no life arm at all.
	///
	/// So the row below was added rather than the claim softened, and the
	/// declaration is now checked per cell (ArmOf) with a tally, so a cell
	/// that silently stops carrying a chronicle fails here instead of passing
	/// three vacuous comparisons.
	enum class LifeArm : uint8
	{
		None,
		Fixed,
		Moving
	};

	/// WHETHER A WORLD OFFERS ANYBODY TO PLAY IS A PROPERTY OF ITS SIZE, and
	/// the expectation is carried per size because measuring it corrected a
	/// claim made one task earlier. `TakeUp` over sizes 32, 64 and 128 at
	/// histories of 10+10, 20+10 and 30+30 years:
	///
	///              32          64          128
	///   no lively  0  0  0     0  0  0     15  11  12
	///   lively     1  1  3    11 11 25     46   8  12
	///
	/// So Options::Lively is SUFFICIENT at every size tried and NECESSARY only
	/// at 32 and 64: a world of 128 has enough people that somebody fits the
	/// start rules without it. 16.11 measured 32 and 64 only and wrote the
	/// result as a law about Lively; this matrix put a 128 cell beside it and
	/// the law did not survive. The Lively rows it added to two size-32 tests
	/// are still right - at 32 those tests were playing nobody.
	///
	/// AND IT IS NOT THE AGE WINDOW. Measured while closing the review: every
	/// cell below takes up the same person under StartRules 1..200 as under
	/// the default 16..40. A row that plays nobody is not a row whose people
	/// are the wrong age - Options::Lively is the gate, which is why widening
	/// the window was not the way to give the no-lively row a chronicle.
	struct Wiring
	{
		const char* Name;
		bool Play;
		bool Stream;
		bool Lively;
		bool Colony;
		/// The person this cell takes up, per entry of Sizes, 0 for nobody.
		///
		/// WHICH person, not merely whether one. The matrix used to reduce
		/// TakeUp's answer to `Who != 0u`. Because Truth and every restore
		/// derive from the same Source, a world that offered a DIFFERENT
		/// person moved them together and all cells stayed green: the
		/// identity was measured when the table above was written, and then
		/// thrown away into a bit. Found by the Phase 16 adversarial review.
		///
		/// It is indexed by the POSITION in Sizes rather than looked up from
		/// the value, because the lookup this replaced - `Size == 64u ? A :
		/// B` - mapped every size that is not 64 onto the 128 column and
		/// would have kept doing so for a size that was neither.
		///
		/// These are measured at 20+10 years WITH each row's own wiring,
		/// which is not the same as the bare probe in the table above: a type
		/// declared in a different position is a different world. The lively
		/// row also declares Colony, and that moved the 128 answer from
		/// person 8 to person 11 - pinning the identity caught it on the
		/// first run.
		uint32 Who[SizeCount];
		/// What the life digest is worth here, per entry of Sizes. Measured,
		/// and checked against ArmOf every run.
		LifeArm Life[SizeCount];
	};

	// A size added without a column added is a Who of 0 and a LifeArm of
	// None, silently, for every row. Both are checked at runtime and would
	// fail loudly rather than pass - but they would fail as "this world
	// stopped offering anybody", which is the wrong diagnosis. Fail here
	// instead, where the cause is.
	static_assert(SizeCount == 2u, "every Wiring pins Who[] and Life[] per size: add the column with the size");

	constexpr Wiring Wirings[] = {
		{"plain", false, false, false, false, {0u, 0u}, {LifeArm::None, LifeArm::None}},
		// 16.12, added closing the Phase 16 review: the ONLY row whose life
		// arm moves without Options::Stream. The control's agreeing half was
		// six cells of `HashBytes(nullptr, 0)` compared with itself; this row
		// gives it six cells whose chronicle is written across the very days
		// a restore has to reproduce. It declares Colony because without it
		// the 128 cell settles at TakeUp and stands still - Fixed, not
		// Moving, which is the coverage this row exists to add.
		{"lively, no stream", true, false, true, true, {11u, 11u}, {LifeArm::Moving, LifeArm::Moving}},
		{"stream, no lively", true, true, false, false, {0u, 11u}, {LifeArm::Fixed, LifeArm::Fixed}},
		{"stream and lively", true, true, true, true, {11u, 11u}, {LifeArm::Moving, LifeArm::Moving}},
	};

	const char* ArmName(LifeArm A)
	{
		switch (A)
		{
		case LifeArm::None:
			return "None";
		case LifeArm::Fixed:
			return "Fixed";
		case LifeArm::Moving:
			return "Moving";
		}
		return "Unknown";
	}

	Options OptionsFor(const Wiring& W, uint32 Size)
	{
		Options O;
		O.Size = Size;
		O.PreHistory = 20u;
		O.Years = 10u;
		O.Play = W.Play;
		O.Stream = W.Stream;
		O.Lively = W.Lively;
		O.Colony = W.Colony;
		return O;
	}

	struct Where
	{
		Hash64 State = 0;
		Hash64 Log = 0;
		Hash64 Life = 0;
		/// Carried beside the digest because the digest cannot tell an empty
		/// chronicle from any other: HashBytes(nullptr, 0) is a perfectly
		/// ordinary value, and comparing it with itself is what nine - in
		/// fact twelve - of the cells were doing.
		usize LifeBytes = 0;
	};

	/// WHAT THE LIFE ARM ACTUALLY IS in a cell, measured from the straight run
	/// rather than taken from the table. `First` is the source's Where at the
	/// EARLIEST save point, `End` its Where at the horizon.
	///
	/// Earliest rather than any: at 64 the lively rows write their last act
	/// before day 25, so the late save already matches the horizon. A cell is
	/// Moving if the life digest moves across the widest span the matrix
	/// restores over, which is the span that decides whether landing on
	/// Truth.Life is a claim or a tautology.
	LifeArm ArmOf(const Where& First, const Where& End)
	{
		if (End.LifeBytes == 0u)
		{
			return LifeArm::None;
		}
		return First.Life != End.Life ? LifeArm::Moving : LifeArm::Fixed;
	}

	Where WhereItIs(const Aelvor& W)
	{
		Where R;
		R.State = W.StateDigest();
		R.Log = W.LogDigest();
		const std::string Story = W.Life();
		R.Life = HashBytes(Story.data(), Story.size());
		R.LifeBytes = Story.size();
		return R;
	}
} // namespace
