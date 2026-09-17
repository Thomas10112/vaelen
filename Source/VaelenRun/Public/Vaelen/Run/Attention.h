// VAELEN - VaelenRun
// Phase 15 task 15.06: where the host is looking, as something the world can be
// told and a replay can put back.
//
// A LEAF. It includes CoreTypes and nothing else, so a host may build one
// without reaching for a world, a view or a system.
//
// That does NOT mean the UI fence admits it: check_ui_fence.py refuses every
// Vaelen/Run/ include outright, and this file said it was admitted "beside
// Intent.h and Stream.h" when those two are named in the fence's allow list and
// this is not. Whoever gives VaelenUI a camera reads the fence first and
// decides deliberately - either the attention is built in VaelenGame, which
// already may name a Run, or the fence gains an entry with a reason. Found by
// the Phase 15 review.
//
// DELIBERATELY NOT View::Eye. The eye of Vaelen/View/Eye.h is an input to the
// VIEW: it says what to put in a frame, it is read by TakeViewFor, and nothing
// it holds reaches the simulation - ADR-0106 is the rule that it must not.
// This is an input to the RUN: it goes through the Door, it is recorded, and a
// replay applies it. The two carry the same three numbers today and they are
// still not the same thing, because one of them is allowed to change what the
// world simulates and the other is forbidden to. Merging them would be the
// shortest path to breaking the rule that keeps the renderer honest.
//
// STATUS: PROTOTYPE (Phase 15) - Tests/Run/Test_Door.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"

namespace Vaelen::Run
{
	/// What the host is looking at, as a request and never as an instruction.
	struct Attention
	{
		uint32 Region = 0; ///< the region the camera settled on, 0 for nowhere
		uint32 Reach = 0;  ///< how far beyond it to care, in regions
		/// How many regions the host will pay to have in detail at once. 0 means
		/// the world's own limit (LodRules::MaxDetailed).
		///
		/// IT IS RECORDED, and the first version of this file said the opposite
		/// - that it was the host's configuration like StartRules, told to a
		/// replay rather than read. That was wrong, and wrong in a way worth
		/// keeping written down: StartRules is fixed for a session and passed to
		/// Replay as a parameter, while this rides on every look and can change
		/// between two of them. It also decides which regions are requested,
		/// hence promoted, hence who exists. An input that changes the world and
		/// has no channel to a replay is not configuration, it is a hole - and
		/// the Phase 15 review measured it: a walk recorded with Most = 1
		/// replayed to another world with Wrong = 0 and no diagnostic.
		uint32 Most = 0;
	};
	static_assert(sizeof(Attention) == 12, "Attention must stay padding free");
} // namespace Vaelen::Run
