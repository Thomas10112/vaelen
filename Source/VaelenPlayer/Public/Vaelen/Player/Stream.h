// VAELEN - VaelenPlayer
// Phase 14.01: the played input as a STREAM - what was meant, when, and how the
// days were turned - in a form a replay can read back.
//
// STATUS: PROTOTYPE (Phase 14) - unit/edge/deterministic tests in Tests/Player/Test_Stream.cpp
//
// 10.04's claim is that "a recorded command stream replays to the same life".
// Five gate tests proved it, and each of them declared its own private
// Recorded{Tick, Command, Verdict} and its own replay loop. The claim had no
// shared type and no interchange form: a life played in the engine could not
// be handed to a headless tool, and the phase gate of Phase 14 is exactly that
// hand-over. This header is the shared type and the form.
//
// THREE record kinds, because a replay is a function of the keys alone:
//
//   Recorded   an intent as submitted, with the tick and the door's verdict
//   TakenUp    a life taken up (the gates' private `Taking`, renamed so the
//              leaf is self-describing - Regard.h:91 is a field, not a clash)
//   DayTurned  ONE PER DAY TURN. The day turn is an input the host makes -
//              Space, in the engine - and a replay must know how many to make.
//              Without it a replay stops at the last command's tick and every
//              trailing day is lost. ADR-0138 is the reading of the layering
//              rule this rests on; this is the record that makes it testable.
//
// ORDER. Three vectors, each in time order, and one rule for equal ticks that
// Encode writes and a replay must honour: takings, then commands, then day
// turns. The rule is what really happens - a person is taken up at tick T and
// their first command is issued at that same T (the gates do exactly this),
// then Day() turns the day AT T and advances to T+24 - and it is asserted in
// the tests rather than trusted. The first draft had commands before takings,
// which would have replayed the first command of every life to nobody.
//
// NOT A SAVE. No file I/O here: the kernel writes no file, and the text is an
// input record, not world state. No VAELEN_SAVE_FORMAT_VERSION bump; Phase 16
// inherits nothing. ADR-0136.
//
// Includes only the leaf. A file that records what a player meant does not
// need to be able to name the World.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/Player/PlayerApi.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen::Player
{
	/// One intent as it was submitted, with what the door said at the time.
	/// Verdict is the DOOR's answer (None = queued), never the world's later
	/// refusal - see Refusal in Intent.h for the two families.
	struct Recorded
	{
		uint64 Tick = 0;
		PlayerCommand Command;
		Refusal Verdict = Refusal::None;
		uint8 Reserved[7] = {};
	};
	static_assert(sizeof(Recorded) == 40, "Recorded must have its padding named");

	/// A life taken up: the person now played, and when.
	struct TakenUp
	{
		uint64 Tick = 0;
		uint32 Person = 0;
		uint32 Reserved = 0;
	};
	static_assert(sizeof(TakenUp) == 16, "TakenUp must have its padding named");

	/// One day turn by the host, at this tick, before the world moved on.
	struct DayTurned
	{
		uint64 Tick = 0;
	};
	static_assert(sizeof(DayTurned) == 8, "DayTurned is one tick");

	/// Where the host was looking, at this tick. Phase 15 task 15.06.
	///
	/// A look is an INPUT and not a frame: what a camera did between two of
	/// these is the presentation layer's business and no replay's. Only the
	/// region it settled on and how far it reached are recorded, because only
	/// those can change what the world pays attention to (ADR-0037: the
	/// presentation writes requests and never promotions).
	///
	/// What is NOT here is how many regions the host will pay for at once. That
	/// is the host's configuration, like StartRules, and a replay is told it
	/// rather than reading it - see Run/Door.h for why the rules stay out of
	/// the stream.
	struct Looked
	{
		uint64 Tick = 0;
		uint32 Region = 0; ///< the region looked at, 0 for nowhere
		uint32 Reach = 0;  ///< how far beyond it, in regions
	};
	static_assert(sizeof(Looked) == 16, "Looked must stay padding free");

	/// The world the stream belongs to. A stream replayed into another world is
	/// refused at the header, not discovered by a digest that will not match.
	struct StreamHeader
	{
		uint64 Seed = 0;
		uint32 Size = 0;	   ///< map side in tiles
		uint32 PreHistory = 0; ///< years before play
		uint32 Years = 0;	   ///< years run after it, before play
		uint32 Version = 1;	   ///< of the text form
	};
	static_assert(sizeof(StreamHeader) == 24, "StreamHeader must stay padding free");

	/// Whether two headers name the same world at the same moment.
	VAELEN_PLAYER_API bool SameWorld(const StreamHeader& A, const StreamHeader& B);

	/// Everything a replay needs and nothing a save needs.
	struct InputStream
	{
		StreamHeader Header;
		std::vector<Recorded> Commands;
		std::vector<TakenUp> Takings;
		std::vector<DayTurned> Days;
		/// 15.06. A stream written before this existed simply has none, which is
		/// why the text form's version did not change: the decoder that reads
		/// `l` lines reads a file without them exactly as it always did.
		std::vector<Looked> Looks;
	};

	/// How many records of each kind, and what they weigh.
	VAELEN_PLAYER_API usize StreamRecords(const InputStream& S);

	/// The text form: one header line, then one record per line in time order,
	/// takings before commands before day turns at equal ticks.
	///
	///   vaelen-stream 1 <seed> <size> <prehistory> <years>
	///   c <tick> <kind> <target> <amount> <hours> <issued> <verdict>
	///   t <tick> <person>
	///   d <tick>
	///   l <tick> <region> <reach>
	///
	/// At equal ticks a look comes before everything: somebody looks, and then
	/// acts on what they saw.
	///
	/// Every number decimal, so a person can read it and a diff can show it.
	VAELEN_PLAYER_API std::string EncodeStream(const InputStream& S);

	struct StreamReport
	{
		uint32 Lines = 0;	   ///< read, blank lines included
		uint32 Records = 0;	   ///< accepted
		uint32 BadLines = 0;   ///< neither blank nor a record - counted, skipped
		uint32 HeaderBad = 0;  ///< 1 when the first line was not a header
		uint32 Refused = 0;	   ///< 1 when Expect was given and the header named another world
		uint32 VersionBad = 0; ///< 1 when the header is one, but of a form this build does not read
	};

	/// Reads the text form back. A corrupt line is counted in BadLines and
	/// skipped, never crashed on. With Expect, a header naming another world is
	/// refused and Out is left untouched. False when the header is bad, of
	/// another version, or refused; BadLines alone do not fail it, they are
	/// reported. Lines is what `wc -l` would say of the text.
	VAELEN_PLAYER_API bool DecodeStream(std::string_view Text, InputStream& Out, StreamReport& Report,
										const StreamHeader* Expect = nullptr);
} // namespace Vaelen::Player
