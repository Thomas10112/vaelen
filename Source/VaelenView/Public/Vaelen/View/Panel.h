// VAELEN - VaelenView
// Phase 14 task 14.06: the first screen, composed kernel-side.
//
// The roadmap's "First screen" is one played person, their day and their
// verbs. 14.02 to 14.05 made the numbers; this makes the PAGE: rows of
// printable ASCII composed from three views and nothing else. TakePanel takes
// a WorldView, a LifeView and a ChronicleView - no World, no ViewSources,
// nothing from Take.h - so the screen cannot reach the simulation even by
// mistake, and a host draws it by copying bytes.
//
// Why a page of text rather than a widget: it is legible in a screenshot,
// parseable by the CI shim that cannot build the engine (14.07), and it can
// be frozen. The LAST row is the page's own digest in hex, of every byte
// before it, so one line of a screenshot says whether the screen is the
// screen the headless suite holds.
//
// The verbs are the half that goes the other way. Each carries its cost, the
// key that presses it and whether the page offers it at all; Press turns a
// key into a PlayerCommand, or refuses it with the refusal the page already
// foresaw - before any world is asked. What the page cannot foresee (no grain
// to eat, a person who just died) is the world's to answer, and comes back as
// a chronicle line.
//
// A leaf, like the eight before it: Core, ViewApi, the three views and the
// command surface of 14.01. Flat, fixed and padding-free: MeasurePanel hashes
// the struct and a renderer copies it.
//
// STATUS: PROTOTYPE (Phase 14) - unit/integration/deterministic tests in Tests/View/Test_Panel.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Life.h"
#include "Vaelen/View/ViewApi.h"

namespace Vaelen::View
{
	inline constexpr uint32 PanelRows = 48;		   ///< rows the page can hold
	inline constexpr uint32 PanelTextBytes = 3072; ///< bytes of those rows, terminators included
	inline constexpr uint32 PanelVerbs = 8;		   ///< Wait, Work, Rest, Eat, Move, Speak, Give, Take
	inline constexpr uint32 PanelChronicle = 16;   ///< chronicle lines the page shows, the newest
	inline constexpr uint32 PanelNamed = 6;		   ///< company named on the "here" row
	inline constexpr uint32 PanelDigestBytes = 16;

	/// What a row of the page says. The text says it too; this is for a widget
	/// that draws a row differently without parsing it.
	enum class RowKind : uint32
	{
		None = 0,
		Date,	   ///< the world and the day
		Self,	   ///< who they are, and who holds them
		Body,	   ///< food, health, rest
		Hours,	   ///< the day's hours and the queue
		Verb,	   ///< one of the eight
		Near,	   ///< the regions a Move will not be refused for
		Company,   ///< who is here
		Chronicle, ///< a line of the life
		Digest,	   ///< the page's own digest, always last
		Count
	};

	/// One of the eight verbs as the page offers it.
	struct VerbView
	{
		uint32 Verb = 0;	 ///< Player::Intent
		uint32 Cost = 0;	 ///< hours it costs
		uint32 Offered = 0;	 ///< 1 when the page offers it
		uint32 Foreseen = 0; ///< Player::Refusal the page foresees, None when the only want is hours
		uint8 Key = 0;		 ///< the ASCII key that presses it (14.09 binds what this says)
		uint8 Reserved[3] = {};
	};
	static_assert(sizeof(VerbView) == 4 * sizeof(uint32) + 4, "VerbView must have no padding");

	/// One row: Text[Begin .. Begin + Length) is its bytes and Text[Begin +
	/// Length] is a NUL, so Text + Begin prints as a C string.
	struct RowView
	{
		uint32 Kind = 0; ///< RowKind
		uint32 Verb = 0; ///< Player::Intent on a Verb row, else 0
		uint32 Begin = 0;
		uint32 Length = 0;
	};
	static_assert(sizeof(RowView) == 4 * sizeof(uint32), "RowView must have no padding");

	struct PanelView
	{
		uint64 Tick = 0;   ///< the frame this page was composed at
		uint64 Digest = 0; ///< of every byte before the Digest row: what that row prints
		uint32 Year = 0;
		uint32 Day = 0;
		uint32 Person = 0;	  ///< whose page, 0 when nobody is played
		uint32 Alive = 0;	  ///< 1 while they live
		uint32 Left = 0;	  ///< hours left today
		uint32 Awake = 0;	  ///< hours the day gives
		uint32 Held = 0;	  ///< intents waiting
		uint32 RowCount = 0;  ///< rows written, in the order they are drawn
		uint32 Used = 0;	  ///< bytes of Text in use, terminators included
		uint32 Truncated = 0; ///< rows cut short because the page ran out of bytes
		uint32 Dropped = 0;	  ///< rows the page had no room to begin at all
		uint32 Offered = 0;	  ///< of the eight verbs, how many the page offers
		VerbView Verbs[PanelVerbs];
		RowView Rows[PanelRows];
		char Text[PanelTextBytes] = {};
	};
	static_assert(sizeof(PanelView) == 2 * sizeof(uint64) + 12 * sizeof(uint32) + PanelVerbs * sizeof(VerbView) +
										   PanelRows * sizeof(RowView) + PanelTextBytes,
				  "PanelView must have no padding: MeasurePanel hashes it and a renderer copies it");
	static_assert(sizeof(PanelView) <= 4096, "the page fits the row's 4 KiB");

	/// Composes the page. Views in, rows out: no World, no ViewSources, no
	/// allocation, and nothing here can move anything. The three views must be
	/// of the same frame - the page says so by carrying the life's tick.
	VAELEN_VIEW_API void TakePanel(const WorldView& World_, const LifeView& Life, const ChronicleView& Told,
								   PanelView& Out);

	/// Turns a verb the page offers into an intent, with Issued = 0: the door
	/// (Run::Door::Mean) stamps that, and a page cannot know the tick it will
	/// be meant at. Answers the refusal the page foresaw when the verb is not
	/// offered, Costly when the only want is the hours this day has left (the
	/// kernel's Costly is narrower: more than a whole day, which nothing can
	/// ever pay for), Unknown when it is not one of the eight, and None when
	/// Out was filled. Nothing is asked of any world: this is the page's answer, and
	/// the world's own comes back later on the queue.
	VAELEN_VIEW_API Player::Refusal Press(const PanelView& V, Player::Intent Verb, uint32 Target, uint32 Amount,
										  Player::PlayerCommand& Out);

	/// Writes the rows a widget draws, joined by '\n' and NUL-terminated, into
	/// a buffer of Bytes. Returns the bytes written, terminator excluded. ALL
	/// of the page or none of it: a buffer too small for the whole page is
	/// given an empty string and 0, because half a page is one a widget draws
	/// without anybody seeing that the bottom - the digest row - is missing.
	/// PanelTextBytes is always enough.
	VAELEN_VIEW_API uint32 Lines(const PanelView& V, char* Out, uint32 Bytes);

	struct PanelStats
	{
		uint32 Rows = 0;	  ///< rows held
		uint32 Bytes = 0;	  ///< sizeof the view
		uint32 TextBytes = 0; ///< bytes of the page itself
		uint32 NonAscii = 0;  ///< bytes of any row outside printable ASCII
		uint32 Truncated = 0; ///< rows cut short
		uint32 Dropped = 0;	  ///< rows there was no room for
		uint32 Offered = 0;	  ///< verbs the page offers
		uint32 Reserved = 0;
		Hash64 Digest = 0; ///< recomputed from the text: what the last row prints
	};
	VAELEN_VIEW_API PanelStats MeasurePanel(const PanelView& V);
} // namespace Vaelen::View
