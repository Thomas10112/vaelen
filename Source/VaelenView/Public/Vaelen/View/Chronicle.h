// VAELEN - VaelenView
// Phase 14 task 14.05: the last lines of one played life, as the screen that
// shows them needs them.
//
// The first screen (roadmap, "First screen") ends with "the last lines of the
// life with every refusal as a sentence". This is those lines as ONE flat
// char buffer - each line NUL-terminated, packed from the front, oldest first
// - and a LineView per line saying when it was, what kind of thing it says,
// which verb and which refusal, and where its bytes are. Plus the why of the
// newest thing that happened because of the played person: two steps, the
// thing and its cause (deeper is Phase 17). No std::string, no handle, no
// pointer: a leaf, like the seven before it. It includes Core, ViewApi and
// the command surface of 14.01, and nothing that names the World. Taking one
// - TakeChronicleView(const World&, ...) - is declared in Take.h.
//
// INCREMENTAL: the view remembers how much of the world's log it has read
// (Since) and the next take reads only what was appended since. A view grown
// over a year equals, byte for byte, a fresh one taken at the year's end:
// Tests/View/Test_Chronicle.cpp holds that. Person = 0 and nothing written
// when nobody is played; a change of played person starts the view over.
//
// Fixed arrays, fixed size, no padding, every byte past what is used zero:
// MeasureChronicleView hashes the bytes, a renderer copies the struct, and
// 14.06's panel composes it with two others into text.
//
// STATUS: PROTOTYPE (Phase 14) - unit/integration/deterministic tests in Tests/View/Test_Chronicle.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/View/ViewApi.h"

namespace Vaelen::View
{
	inline constexpr uint32 ChronicleLines = 32;	   ///< lines kept, the newest
	inline constexpr uint32 ChronicleTextBytes = 6144; ///< bytes of those lines, terminators included
	inline constexpr uint32 WhyLines = 2;			   ///< the thing and its cause
	inline constexpr uint32 WhyTextBytes = 512;

	/// What kind of thing a line says. The text says it too; this is for a
	/// panel that greys or colours a row without parsing it.
	enum class LineKind : uint32
	{
		None = 0, ///< a line of some other layer (a why step can be one)
		Acted,	  ///< the played person did something (PlayerActed)
		Refused,  ///< the world would not let them (PlayerRefused)
		Walked,	  ///< they moved (PersonMoved)
		Born,
		Died,
		Count
	};

	/// One line of the buffer: Text[Begin .. Begin + Length) is its bytes and
	/// Text[Begin + Length] is a NUL, so Text + Begin prints as a C string.
	struct LineView
	{
		uint64 Tick = 0;	///< when it happened
		uint32 Year = 0;	///< Tick / TicksPerYear
		uint32 Kind = 0;	///< LineKind
		uint32 Verb = 0;	///< Player::Intent for Acted and Refused, else 0
		uint32 Refused = 0; ///< Player::Refusal for Refused, else 0
		uint32 Begin = 0;
		uint32 Length = 0;
	};
	static_assert(sizeof(LineView) == sizeof(uint64) + 6 * sizeof(uint32), "LineView must have no padding");

	struct ChronicleView
	{
		uint64 Tick = 0;   ///< the frame this brings the view up to
		uint64 Since = 0;  ///< log events read so far: the cursor an incremental take resumes from
		uint64 WhyOf = 0;  ///< id of the event the Why lines explain, 0 when nothing yet happened because of them
		uint32 Person = 0; ///< whose life, 0 when nobody is played
		uint32 Year = 0;
		uint32 LineCount = 0; ///< lines held, oldest first, at most ChronicleLines
		uint32 Used = 0;	  ///< bytes of Text in use, terminators included
		uint32 Dropped = 0;	  ///< lines that fell off the front to make room
		uint32 Truncated = 0; ///< lines cut short because they alone would not fit
		uint32 WhyCount = 0;  ///< Why lines held, at most WhyLines
		uint32 WhyUsed = 0;	  ///< bytes of WhyText in use
		LineView Lines[ChronicleLines];
		LineView Why[WhyLines];
		char Text[ChronicleTextBytes] = {};
		char WhyText[WhyTextBytes] = {};
	};
	static_assert(sizeof(ChronicleView) == 3 * sizeof(uint64) + 8 * sizeof(uint32) +
											   (ChronicleLines + WhyLines) * sizeof(LineView) + ChronicleTextBytes +
											   WhyTextBytes,
				  "ChronicleView must have no padding: MeasureChronicleView hashes it and a renderer copies it");
	static_assert(sizeof(ChronicleView) <= 8192, "the chronicle view fits the row's 8 KiB");

	struct ChronicleStats
	{
		uint32 Lines = 0;	   ///< lines held
		uint32 Bytes = 0;	   ///< sizeof the view
		uint32 EventsRead = 0; ///< log events read in the view's life (Since)
		uint32 NonAscii = 0;   ///< bytes of any line outside printable ASCII
		uint32 Truncated = 0;  ///< lines cut short
		uint32 WhyLines_ = 0;  ///< why lines held
		Hash64 Digest = 0;	   ///< every byte of the view
	};
	VAELEN_VIEW_API ChronicleStats MeasureChronicleView(const ChronicleView& V);
} // namespace Vaelen::View
