// VAELEN - VaelenRun
// Phase 15 task 15.06: where the host is looking, as something the world can be
// told and a replay can put back.
//
// A LEAF. It includes CoreTypes and nothing else, so a host may build one
// without reaching for a world, a view or a system - which is what lets the UI
// fence (Tools/check_ui_fence.py) admit it beside Intent.h and Stream.h.
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
		/// How many regions the host will pay to have in detail at once. It is
		/// NOT recorded in the stream: like StartRules it is the host's
		/// configuration, and a replay is told it rather than reading it, for
		/// the reason Door.h gives about rules.
		uint32 Most = 0;
	};
	static_assert(sizeof(Attention) == 12, "Attention must stay padding free");
} // namespace Vaelen::Run
