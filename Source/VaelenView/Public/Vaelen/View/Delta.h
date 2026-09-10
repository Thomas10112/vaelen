// VAELEN - VaelenView
// Phase 13 task 13.02: what changed since the last frame.
//
// 13.01 takes a whole view every frame, and for a world of ninety-nine regions
// that is 5600 bytes and nobody cares. AELVOR at 256 has thousands, and a
// renderer that re-reads all of them sixty times a second to find the four that
// moved is not a renderer, it is a poll.
//
// So: the difference between two views, and - the part that makes it worth
// having rather than worth trusting - a way to APPLY that difference to the
// older view and get the newer one back, byte for byte. The test does exactly
// that and compares digests, because a delta nobody can replay is a delta
// nobody should believe.
//
// The delta is the same kind of thing the view is: a flat block of numbers with
// no way back into the world (ADR-0104).
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/edge tests in Tests/View/Test_Delta.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/ViewApi.h"

#include <vector>

namespace Vaelen::View
{
	/// What one frame changed.
	struct ViewDelta
	{
		uint64 Tick = 0; ///< the frame this brings the view up to
		uint32 Year = 0;
		uint32 Width = 0;
		uint32 Height = 0;
		uint32 People = 0;
		uint32 Played = 0;
		/// 1 when the ground itself changed - a different map, a different world -
		/// and Changed carries the whole view rather than a difference. Rare, and
		/// the honest thing to do when the two views are not of the same thing.
		uint32 Whole = 0;
		uint32 Reserved = 0;

		std::vector<RegionView> Changed; ///< regions whose bytes differ, in index order
		std::vector<uint32> Gone;		 ///< region indices the newer view no longer has
	};

	/// The difference between two views. `Was` may be empty, which makes the
	/// first delta of a session the whole view - which is correct, because a
	/// renderer that has drawn nothing yet needs everything.
	VAELEN_VIEW_API void Diff(const WorldView& Was, const WorldView& Now, ViewDelta& Out);

	/// Applies a delta to a view in place. After this, `Onto` is byte for byte
	/// the view the delta was made from - which is the claim the whole task
	/// rests on and the one its test checks.
	VAELEN_VIEW_API void Apply(WorldView& Onto, const ViewDelta& D);

	struct DeltaStats
	{
		uint32 Changed = 0; ///< regions carried
		uint32 Gone = 0;
		uint32 Bytes = 0;	 ///< what the delta weighs
		uint32 Whole = 0;	 ///< bytes the full view would have weighed
		uint32 PerMille = 0; ///< the delta as a share of the full view, which is the point
		Hash64 Digest = 0;
	};
	VAELEN_VIEW_API DeltaStats MeasureDelta(const ViewDelta& D, const WorldView& Of);
} // namespace Vaelen::View
