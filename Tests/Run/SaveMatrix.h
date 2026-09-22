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
	using namespace Vaelen;
	using namespace Vaelen::Run;

	/// The horizon, and where along it the saves are taken. Late is not the
	/// last day: a save taken on the final day is never CONTINUED, and
	/// continuing is the whole claim.
	constexpr uint32 Horizon = 30u;
	constexpr uint32 SavePoints[] = {5u, 15u, 25u};

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
	struct Wiring
	{
		const char* Name;
		bool Play;
		bool Stream;
		bool Lively;
		bool Colony;
		bool PlaysAt64;
		bool PlaysAt128;
	};

	constexpr Wiring Wirings[] = {
		{"plain", false, false, false, false, false, false},
		{"stream, no lively", true, true, false, false, false, true},
		{"stream and lively", true, true, true, true, true, true},
	};

	constexpr uint32 Sizes[] = {64u, 128u};

	bool PlaysAt(const Wiring& W, uint32 Size)
	{
		return Size == 64u ? W.PlaysAt64 : W.PlaysAt128;
	}

	/// WHICH person, not merely whether one.
	///
	/// The matrix used to reduce TakeUp's answer to `Who != 0u`. Because Truth
	/// and every restore derive from the same Source, a world that offered a
	/// DIFFERENT person moved them together and all eighteen cells stayed
	/// green: the identity was measured when the table above was written, and
	/// then thrown away into a bit. Found by the Phase 16 adversarial review.
	///
	/// 0 means this cell offers nobody. Every other value is pinned, so a
	/// change that steers BeginEnslaved's candidate order - a different
	/// Bridging.MaxDetailed, a reordered person index - fails here instead of
	/// quietly re-basing the whole matrix on another world.
	///
	/// These are measured at 20+10 years WITH each row's own wiring, which is
	/// not the same as the bare probe in the table above: the lively row also
	/// declares Colony, and a type declared in a different position is a
	/// different world.
	uint32 WhoPlaysAt(const Wiring& W, uint32 Size)
	{
		if (!PlaysAt(W, Size))
		{
			return 0u;
		}
		// AND THE COLONY ROW PROVED THE POINT ABOVE WHILE THIS WAS WRITTEN.
		// The bare probe measured person 8 at 128 with Lively; this row also
		// declares Colony, and it takes person 11. Pinning the identity caught
		// that on the first run - the bit the matrix used to keep could not
		// have, because Truth and every restore moved to person 11 together.
		if (W.Lively)
		{
			return 11u;
		}
		return Size == 64u ? 0u : 11u;
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
	};

	Where WhereItIs(const Aelvor& W)
	{
		Where R;
		R.State = W.StateDigest();
		R.Log = W.LogDigest();
		const std::string Story = W.Life();
		R.Life = HashBytes(Story.data(), Story.size());
		return R;
	}
} // namespace
