// VAELEN - VaelenView
// Phase 14 task 14.04: one played life, as the screen that shows it needs it.
//
// The first screen (roadmap, "First screen") is one person, their day and
// their verbs. This is everything it draws about them, as a flat block of
// numbers and NUL-terminated ASCII names: who they are and where, what the
// life was at its first moment, the hours of the day, the body, the queue,
// the regard, the cost of each verb, the neighbours a Move will not be refused
// for, and the company a Speak, Give or Take can be aimed at. No pointer, no
// handle, no tick of birth: an age. A leaf, like the six of 14.02: it includes
// Core, ViewApi and the command surface of 14.01, and nothing that names the
// World. Taking one - TakeLifeView(const World&, ...) - is declared in Take.h.
//
// Fixed arrays, fixed names, fixed size, no padding: MeasureLifeView hashes
// the bytes, a renderer copies the struct, and 14.06's panel composes it with
// two others into text. Person = 0 and every name empty when nobody is played.
//
// STATUS: PROTOTYPE (Phase 14) - unit/integration/deterministic tests in Tests/View/Test_Life.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/View/ViewApi.h"

namespace Vaelen::View
{
	/// Bytes of every name here, terminator included. A generated name is at
	/// most 23 letters (Sim/Naming.h) and the fallback is "person 12345".
	inline constexpr uint32 LifeNameBytes = 32;
	inline constexpr uint32 MostWaiting = 8;  ///< == Player::MostOrders, asserted in Life.cpp
	inline constexpr uint32 MostKnownOf = 8;  ///< == Player::MostKnown, asserted in Life.cpp
	inline constexpr uint32 MostNear = 8;	  ///< neighbours listed
	inline constexpr uint32 MostCompany = 16; ///< people listed of those in the region
	inline constexpr uint32 IntentSlots = 16; ///< == the size of OrderRules::HoursOf, asserted in Life.cpp

	/// Somebody the played person has an opinion of (10.06).
	struct KnownView
	{
		uint32 Person = 0;
		int32 Regard = 0; ///< their standing in the played person's eyes
		uint32 Met = 0;	  ///< times met
		uint32 Reserved = 0;
		char Name[LifeNameBytes] = {};
	};
	static_assert(sizeof(KnownView) == 4 * sizeof(uint32) + LifeNameBytes, "KnownView must have no padding");

	/// Somebody alive in the same region: what Speak, Give and Take aim at.
	struct CompanyView
	{
		uint32 Person = 0;
		uint32 Years = 0; ///< age at this frame's tick
		uint8 Sex = 0;	  ///< Population::Sex
		uint8 Reserved[3] = {};
		char Name[LifeNameBytes] = {};
	};
	static_assert(sizeof(CompanyView) == 2 * sizeof(uint32) + 4 + LifeNameBytes, "CompanyView must have no padding");

	struct LifeView
	{
		uint64 Tick = 0;
		uint32 Year = 0;
		uint32 Day = 0; ///< day of the year, 0-based

		// Who, and where.
		uint32 Person = 0; ///< 0 when nobody is played
		uint32 Region = 0;
		uint32 Alive = 0; ///< 1 while the played person is alive
		uint32 Years = 0; ///< their age at this frame's tick
		char Name[LifeNameBytes] = {};
		char RegionName[LifeNameBytes] = {};

		// What the life was at its first moment (10.02).
		uint32 StartRegion = 0;
		uint32 Holder = 0; ///< person index of who held them, 0 = the region itself
		uint32 StartYear = 0;
		uint32 Bond = 0; ///< Society::BondKind at the start
		char HolderName[LifeNameBytes] = {};

		// The day (10.03).
		uint32 Awake = 0; ///< waking hours this day gives
		uint32 Spent = 0; ///< of those, already spent
		uint32 Left = 0;  ///< what remains to mean anything with
		uint32 DaysLived = 0;
		uint32 Missed = 0;
		uint32 Season = 0; ///< 18.04: 0 without a climate, else 1 spring .. 4 winter

		// The body, 0..255.
		uint8 Food = 0;
		uint8 Health = 0;
		uint8 Rest = 0;
		uint8 Hungry = 0;

		// The queue (10.04).
		uint32 Held = 0;
		uint32 Taken = 0;
		uint32 Refused = 0;
		uint32 Dropped = 0;
		uint32 LastRefusal = 0; ///< Player::Refusal of the last refusal
		int32 Degrees = 0; ///< 18.04: tenths of a degree at the played region's centroid today; 0 without a climate
		uint32 Chill = 0;  ///< 18.04: the played person's chill, 0 = warm (filled by 18.05; 0 until then)
		Player::PlayerCommand Waiting[MostWaiting]; ///< the first Held of them, oldest first

		// The regard (10.06).
		int32 Repute = 0;
		uint32 KnownCount = 0;
		uint32 Kindnesses = 0;
		uint32 Wrongs = 0;
		KnownView Known[MostKnownOf];

		// The verbs: hours each Intent costs, by Intent.
		uint32 Cost[IntentSlots] = {};

		// Where a Move will not be refused TooFar: adjacent AND detailed.
		uint32 Near[MostNear] = {};
		uint32 NearCount = 0;
		uint32 CompanyCount = 0; ///< listed below
		uint32 CompanyThere = 0; ///< alive in the region besides the played person, listed or not
		uint32 Winter = 0;		 ///< 18.04: this year's winter severity at the played region, 0..3; 0 without a climate
		CompanyView Company[MostCompany];
	};
	static_assert(sizeof(LifeView) ==
					  sizeof(uint64) + 6 * sizeof(uint32) + 2 * LifeNameBytes + 4 * sizeof(uint32) + LifeNameBytes +
						  6 * sizeof(uint32) + 4 + 7 * sizeof(uint32) + MostWaiting * sizeof(Player::PlayerCommand) +
						  4 * sizeof(uint32) + MostKnownOf * sizeof(KnownView) + IntentSlots * sizeof(uint32) +
						  MostNear * sizeof(uint32) + 4 * sizeof(uint32) + MostCompany * sizeof(CompanyView),
				  "LifeView must have no padding: MeasureLifeView hashes it and a renderer copies it");

	struct LifeStats
	{
		uint32 Bytes = 0;
		uint32 Named = 0;  ///< names present: the person, the region, the holder, the known, the company
		uint32 Listed = 0; ///< Near + Known + Company entries
		uint32 Reserved = 0;
		Hash64 Digest = 0; ///< every byte of the view
	};
	VAELEN_VIEW_API LifeStats MeasureLifeView(const LifeView& V);
} // namespace Vaelen::View
