// VAELEN - VaelenView
// Phase 19 task 19.04: the lines a sitting is checked by, composed once.
//
// A LEAF. An engine sitting is closed by a log line re-read against the line
// the headless side prints for the same world (Tools/check_session.py). Two
// printf calls with one format string each - one in the Atlas, one in the
// engine - agree until somebody edits one of them, and 14.10 learned that the
// only comparison worth having is byte for byte. So the line is composed HERE,
// from plain numbers, and both sides call this: the Atlas today, the engine's
// Vaelen.Play and Vaelen.Day from 19.06.
//
// What goes in is facts, not a world: the caller measures, this writes. So
// the engine, which may not name a World, can still print the line.
//
// STATUS: VALIDATED headless (Phase 19 task 19.04)
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/View/Climate.h"
#include "Vaelen/View/ViewApi.h"

namespace Vaelen::View
{
	/// What LogVaelenClimate says, each field measured by the caller.
	struct ClimateLineFacts
	{
		uint32 Size = 0;		///< the map's side, in tiles
		uint64 Seed = 0;		///< the world's seed
		uint32 Day = 0;			///< of the year, 0-based as ClimateView keeps it; printed 1-based
		uint32 Year = 0;		///< ClimateView::Year
		uint32 Season = 0;		///< ClimateView::Season: 0 none, 1 spring ... 4 winter
		ClimateViewStats Stats; ///< MeasureClimateView of the same view
		uint32 HardWinters = 0; ///< great and terrible winters on the log
		uint32 ColdDeaths = 0;	///< the dead of the cold, coarse and person alike
		uint32 Reserved = 0;
	};

	/// The whole line and nothing else - no newline - NUL-terminated into a
	/// buffer of Bytes. Returns the bytes written, terminator excluded. ALL OR
	/// NOTHING, as Lines() is: a buffer too small gets an empty string and 0,
	/// because a line cut short still starts like the right one.
	/// ClimateLineBytes is always enough: 134 bytes of words, and with every
	/// number at its widest and the longest season the line is 270 - which
	/// View.Proof composes and checks against this.
	inline constexpr uint32 ClimateLineBytes = 320;
	VAELEN_VIEW_API uint32 ClimateLine(const ClimateLineFacts& Facts, char* Out, uint32 Bytes);
} // namespace Vaelen::View
