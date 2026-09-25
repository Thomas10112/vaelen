// VAELEN - VaelenRun tests
// Phase 17 task 17.01: the container corpus reads, and the record beside it is
// checked against the bytes rather than admired beside them.
//
// WHY THIS IS THE FIRST TASK OF THE PHASE. Phase 16 built VAELENCP - the
// container, its variable-length section table, its two flag halves and its
// migration machinery - and closed without a single container checked in.
// `Tests/Run/Golden/` holds `.snapshot` files, which are the INNER format: a
// reader handed one says BadMagic. So every instrument Phase 17 plans (the
// inspector, the store's cold listing, the census) was specified against files
// that did not exist, and three of the four planning angles wrote gate clauses
// over them.
//
// NO WORLD IS GENERATED HERE. There is no `Aelvor` in this file: the corpus is
// read from disk and every number is checked against the bytes. That is the
// whole claim - a container is readable without the simulation that wrote it -
// and it is why the file is structured so that the claim is visible rather than
// argued. Regenerating the corpus IS a world, three of them, and that is the
// separate `Atlas.ContainersRegenerate` entry.
//
// STATUS: PROTOTYPE (Phase 17)
#include "VaelenTest.h"

#include "Vaelen/Core/Version.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Player/Stream.h"
#include "Vaelen/Run/Checkpoint.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	struct Recorded
	{
		uint16 Kind;
		uint64 Offset;
		uint64 Length;
		uint64 Digest;
	};

	struct Container
	{
		const char* Name;
		usize Bytes;
		uint32 Version;
		uint32 Flags;
		uint32 InnerFormat;
		uint64 Seed;
		uint64 Tick;
		uint64 LogEvents;
		uint64 LogBytes;
		/// The IMAGE's trailer - the last eight bytes of the STATE section -
		/// which is what `ComputeStateDigest` returns and what every frozen
		/// digest in this repository is. NOT the STATE row's section digest,
		/// which is a different number over the same bytes. Confusing the two
		/// is the defect 17.03 exists for, and the corpus carries both so that
		/// a reader which swapped them cannot pass.
		uint64 Trailer;
		usize SectionCount;
		Recorded Sections[4];
	};

	// The table of Tests/Run/Containers/README.md, in code. Written by
	// `VaelenAtlas --containers`, which reads its own output back through
	// `ReadCheckpoint` before recording a single figure - so these are what a
	// READER sees, not what the writer intended.
	constexpr Container Corpus[] = {
		{"bare-16.container",
		 77686,
		 2,
		 0,
		 3,
		 0x000041454c564f52ull,
		 95040,
		 129,
		 14464,
		 0xd17d7fd9a6f09ea6ull,
		 3,
		 {{1, 146, 77478, 0xeae7b629c9d0924eull},
		  {2, 77624, 29, 0xbfd0910bf8f1c63full},
		  {3, 77653, 25, 0x4a4c61d69390db4bull},
		  {0, 0, 0, 0}}},
		{"played-16.container",
		 80863,
		 2,
		 0,
		 3,
		 0x000041454c564f52ull,
		 95184,
		 148,
		 16592,
		 0x64d11b40b612581cull,
		 4,
		 {{1, 176, 80420, 0x389defae6a977e74ull},
		  {2, 80596, 37, 0xbefe7c22e544cdd4ull},
		  {3, 80633, 25, 0xd4f215b4679211f7ull},
		  {4, 80658, 197, 0xa65b8a037dcc3b7aull}}},
		{"full-32.container",
		 182720,
		 2,
		 0,
		 3,
		 0x000041454c564f52ull,
		 95184,
		 577,
		 64640,
		 0x2ec516c4d275ab6full,
		 3,
		 {{1, 146, 182498, 0x3e0cd92228e56387ull},
		  {2, 182644, 43, 0x40ed8c3bbb2a1640ull},
		  {3, 182687, 25, 0x0df7628587f8e927ull},
		  {0, 0, 0, 0}}},
		// 18.02: THE FIRST OLDER-FORMAT INSTANCE IN THE TREE. This is
		// played-16.container as it was written before 18.02 added the fifth
		// HOST byte, kept byte for byte and NEVER regenerated: its HOST
		// section is 24 bytes, and a reader that reads it as Climate = false
		// is the 24/25 rule Run.Checkpoint pins. Same world, same trailer as
		// the row above; one byte shorter, and every offset after the HOST
		// section one less.
		{"host24-16.container",
		 80862,
		 2,
		 0,
		 3,
		 0x000041454c564f52ull,
		 95184,
		 148,
		 16592,
		 0x64d11b40b612581cull,
		 4,
		 {{1, 176, 80420, 0x389defae6a977e74ull},
		  {2, 80596, 37, 0xbefe7c22e544cdd4ull},
		  {3, 80633, 24, 0x8616198be3fd64adull},
		  {4, 80657, 197, 0xa65b8a037dcc3b7aull}}},
	};

	// 18.02 MOVED BYTES AND NO WORLD. The fifth HOST byte changed three
	// section digests and three file sizes, and it must have changed no image:
	// the three trailers are the values they had before it, named here so that
	// a regeneration which moved one is caught by this line and not only by
	// the table above quietly re-pinned.
	static_assert(Corpus[0].Trailer == 0xd17d7fd9a6f09ea6ull, "bare-16's image moved through the HOST byte");
	static_assert(Corpus[1].Trailer == 0x64d11b40b612581cull, "played-16's image moved through the HOST byte");
	static_assert(Corpus[2].Trailer == 0x2ec516c4d275ab6full, "full-32's image moved through the HOST byte");
	static_assert(Corpus[3].Trailer == Corpus[1].Trailer, "the kept older container is not the world of played-16");

	bool ReadWhole(const std::string& Path, std::vector<uint8>& Out)
	{
		std::FILE* File = std::fopen(Path.c_str(), "rb");
		if (File == nullptr)
		{
			return false;
		}
		std::fseek(File, 0, SEEK_END);
		const long Size = std::ftell(File);
		std::fseek(File, 0, SEEK_SET);
		Out.resize(Size > 0 ? static_cast<usize>(Size) : 0u);
		const usize Read = Out.empty() ? 0u : std::fread(Out.data(), 1, Out.size(), File);
		std::fclose(File);
		return Read == Out.size();
	}

	/// The image's own trailer, read out of the STATE section. This was kept
	/// here rather than in the kernel "until something in production needs
	/// to"; 16.14's engine store did, and `Run::ImageTrailer` is the kernel's
	/// reader now. This one STAYS, as the independent reader: a corpus test
	/// that read the trailer through the function under test would be
	/// checking that function against itself, and the case below requires the
	/// two to agree on every file.
	bool TrailerOf(const CheckpointView& View, uint64& Out) noexcept
	{
		uint64 Length = 0;
		const uint8* State = View.Find(SectionKind::State, Length);
		if (State == nullptr || Length < sizeof(uint64))
		{
			return false;
		}
		uint64 Value = 0;
		for (usize I = 0; I < sizeof(uint64); ++I)
		{
			Value |= static_cast<uint64>(State[Length - sizeof(uint64) + I]) << (8u * I);
		}
		Out = Value;
		return true;
	}

	/// The FIRST field of the record that the bytes disagree with, by name, or
	/// nullptr when every one of them agrees.
	///
	/// This exists as a function rather than as a wall of VT_CHECK so that the
	/// comparison itself can be put on trial: `PerturbationsAreSeen` below
	/// feeds it a record with one field bent and requires it to name that
	/// field. A comparison nobody has watched fail is a comparison nobody
	/// knows reads anything.
	const char* FirstDisagreement(const Container& R, const CheckpointView& V, usize Size, uint64 Trailer) noexcept
	{
		if (Size != R.Bytes)
		{
			return "bytes";
		}
		if (V.Version != R.Version)
		{
			return "version";
		}
		if (V.Flags != R.Flags)
		{
			return "flags";
		}
		if (V.InnerFormat != R.InnerFormat)
		{
			return "inner format";
		}
		if (V.Seed != R.Seed)
		{
			return "seed";
		}
		if (V.Tick != R.Tick)
		{
			return "tick";
		}
		if (V.LogEvents != R.LogEvents)
		{
			return "log events";
		}
		if (V.LogBytes != R.LogBytes)
		{
			return "log bytes";
		}
		if (Trailer != R.Trailer)
		{
			return "trailer";
		}
		if (V.Sections.size() != R.SectionCount)
		{
			return "section count";
		}
		for (usize I = 0; I < R.SectionCount; ++I)
		{
			if (V.Sections[I].Kind != R.Sections[I].Kind)
			{
				return "section kind";
			}
			if (V.Sections[I].Offset != R.Sections[I].Offset)
			{
				return "section offset";
			}
			if (V.Sections[I].Length != R.Sections[I].Length)
			{
				return "section length";
			}
			if (V.Sections[I].Digest != R.Sections[I].Digest)
			{
				return "section digest";
			}
		}
		return nullptr;
	}

	std::string PathOf(const char* Name)
	{
		return std::string(VAELEN_CONTAINER_DIR) + "/" + Name;
	}
} // namespace

VAELEN_TEST(Containers, TheCorpusReadsWithoutAWorld)
{
	VT_CHECK_MSG(CheckpointVersion == 2u, "the corpus is v2; a version bump needs a migration step and a new corpus");
	for (const Container& C : Corpus)
	{
		std::vector<uint8> Bytes;
		VT_CHECK_MSG(ReadWhole(PathOf(C.Name), Bytes), "%s", C.Name);

		CheckpointView View;
		const CheckpointRefusal Read = ReadCheckpoint(Bytes.data(), Bytes.size(), View);
		if (Read.Result != CheckpointResult::Ok)
		{
			// One failure and ON TO THE NEXT FILE. Checked while making this
			// test fail on purpose: a container that does not read leaves an
			// empty view, and every clause below then fails too - four
			// failures for one cause, the first of which is the only true one.
			// The other two containers are still worth reading.
			VT_CHECK_MSG(false, "%s was refused: %s", C.Name, CheckpointResultToString(Read.Result));
			continue;
		}

		uint64 Trailer = 0;
		VT_CHECK_MSG(TrailerOf(View, Trailer), "%s: the STATE section is too short to hold a trailer", C.Name);
		VT_CHECK_MSG(ImageTrailer(View) == Trailer,
					 "%s: the kernel's ImageTrailer (%016llx) disagrees with this file's own reader (%016llx)", C.Name,
					 static_cast<unsigned long long>(ImageTrailer(View)), static_cast<unsigned long long>(Trailer));

		const char* Wrong = FirstDisagreement(C, View, Bytes.size(), Trailer);
		VT_CHECK_MSG(Wrong == nullptr, "%s disagrees about its %s", C.Name, Wrong == nullptr ? "nothing" : Wrong);

		// THE SHAPE, not only the numbers. A corpus where every file has the
		// same section table cannot catch a reader that assumes one, so the
		// three differ deliberately and the difference is asserted.
		uint64 StreamLength = 0;
		const bool HasStream = View.Find(SectionKind::Stream, StreamLength) != nullptr;
		VT_CHECK(HasStream == (C.SectionCount == 4u));

		// Every container has the other three, whatever else it has.
		uint64 Length = 0;
		VT_CHECK(View.Find(SectionKind::State, Length) != nullptr);
		VT_CHECK(View.Find(SectionKind::Run, Length) != nullptr);
		VT_CHECK(View.Find(SectionKind::Host, Length) != nullptr);
	}
}

VAELEN_TEST(Containers, TheStreamSectionCarriesRulesNobodyDefaulted)
{
	// 17.07's defect, guarded a phase early and by the corpus rather than by a
	// macro: an assertion that compares two default-constructed structs passes
	// even when the reader wrote nothing into either. So the played container's
	// StartRules are all four NON-DEFAULT, and this names every one of them.
	//
	// They are also the only rules a ten-year world answers at all: nobody in
	// it is bound and nobody in it is twelve, so `StartRules{}` - a bound life
	// aged 16 to 40 - is offered nobody. The corpus could not have a played
	// container until the window was opened, which is a fact about a young
	// world and is recorded in Tools/Atlas/Main.cpp beside the code that uses
	// it.
	std::vector<uint8> Bytes;
	VT_CHECK(ReadWhole(PathOf("played-16.container"), Bytes));
	CheckpointView View;
	VT_CHECK(ReadCheckpoint(Bytes.data(), Bytes.size(), View).Result == CheckpointResult::Ok);

	Player::InputStream Tape;
	Player::StartRules Rules;
	VT_CHECK_MSG(ReadStreamSection(View, Tape, Rules), "played-16 has a STREAM section and it did not decode");

	const Player::StartRules Default;
	VT_CHECK_MSG(Rules.FromAge != Default.FromAge && Rules.ToAge != Default.ToAge &&
					 Rules.WantBound != Default.WantBound && Rules.PreferOre != Default.PreferOre,
				 "the corpus rules must differ from the defaults in every field, or this assertion is vacuous");
	VT_CHECK(Rules.FromAge == 0u);
	VT_CHECK(Rules.ToAge == 45u);
	VT_CHECK(Rules.WantBound == 0u);
	VT_CHECK(Rules.PreferOre == 0u);

	// The tape is a walk and not an empty stream: six days, and a header that
	// names the world it came from.
	VT_CHECK(Tape.Days.size() == 6u);
	VT_CHECK(Tape.Header.Size == 16u);
	VT_CHECK(Tape.Header.PreHistory == 10u);
	VT_CHECK(Tape.Header.Years == 1u);
}

VAELEN_TEST(Containers, PerturbationsAreSeen)
{
	// CONTROL ONE, AND IT IS KEPT. Everything above rests on
	// `FirstDisagreement` actually reading each field. A comparison that
	// silently skipped one would leave the README agreeing with nothing, which
	// is precisely how a corpus rots: the numbers stay in the file, nobody
	// checks them, and a reader that swapped two of them passes.
	//
	// So every field is bent by one, in turn, and the comparison must name the
	// field that was bent. Both arms in one run: the untouched record agrees,
	// and each bent one does not.
	std::vector<uint8> Bytes;
	VT_CHECK(ReadWhole(PathOf("played-16.container"), Bytes));
	CheckpointView View;
	VT_CHECK(ReadCheckpoint(Bytes.data(), Bytes.size(), View).Result == CheckpointResult::Ok);
	uint64 Trailer = 0;
	VT_CHECK(TrailerOf(View, Trailer));

	const Container Good = Corpus[1];
	VT_CHECK(std::string(Good.Name) == "played-16.container");
	VT_CHECK(FirstDisagreement(Good, View, Bytes.size(), Trailer) == nullptr);

	struct Bend
	{
		const char* Names;
		void (*Apply)(Container&);
	};
	const Bend Bends[] = {
		{"bytes", [](Container& C) { C.Bytes += 1; }},
		{"version", [](Container& C) { C.Version += 1; }},
		{"flags", [](Container& C) { C.Flags += 1; }},
		{"inner format", [](Container& C) { C.InnerFormat += 1; }},
		{"seed", [](Container& C) { C.Seed += 1; }},
		{"tick", [](Container& C) { C.Tick += 1; }},
		{"log events", [](Container& C) { C.LogEvents += 1; }},
		{"log bytes", [](Container& C) { C.LogBytes += 1; }},
		{"trailer", [](Container& C) { C.Trailer += 1; }},
		{"section count", [](Container& C) { C.SectionCount -= 1; }},
		{"section kind", [](Container& C) { C.Sections[3].Kind += 1; }},
		{"section offset", [](Container& C) { C.Sections[2].Offset += 1; }},
		{"section length", [](Container& C) { C.Sections[1].Length += 1; }},
		{"section digest", [](Container& C) { C.Sections[0].Digest += 1; }},
	};
	for (const Bend& B : Bends)
	{
		Container Bent = Good;
		B.Apply(Bent);
		const char* Said = FirstDisagreement(Bent, View, Bytes.size(), Trailer);
		VT_CHECK_MSG(Said != nullptr, "bending %s was not seen at all", B.Names);
		VT_CHECK_MSG(std::string(Said) == B.Names, "bending %s was reported as %s", B.Names, Said);
	}
}

VAELEN_TEST(Containers, EditedBytesAreRefused)
{
	// CONTROL TWO: the bytes, not the record. A container whose section count
	// says one more or one fewer than the table describes must be REFUSED, and
	// so must a flipped byte in a payload. A corpus that loaded anything would
	// make every clause above meaningless.
	std::vector<uint8> Original;
	VT_CHECK(ReadWhole(PathOf("played-16.container"), Original));

	// Byte 52 is the SectionCount word: Magic(8) + Version(4) + Flags(4) +
	// InnerFormat(4) + Seed(8) + Tick(8) + LogEvents(8) + LogBytes(8).
	constexpr usize SectionCountAt = 52;
	VT_CHECK(Original[SectionCountAt] == 4u);

	for (uint8 Count : {uint8{3}, uint8{5}, uint8{0}, uint8{64}})
	{
		std::vector<uint8> Bent = Original;
		// Through a checked pointer rather than operator[]: gcc at -O2 with
		// -Wnull-dereference cannot see that a copy of a non-empty vector has
		// storage, and -Werror made the release legs refuse to build this.
		uint8* const Bytes = Bent.data();
		VT_REQUIRE(Bytes != nullptr && Bent.size() > SectionCountAt);
		Bytes[SectionCountAt] = Count;
		CheckpointView View;
		const CheckpointRefusal Read = ReadCheckpoint(Bent.data(), Bent.size(), View);
		VT_CHECK_MSG(Read.Result != CheckpointResult::Ok, "it was read as Ok, not refused (%s)",
					 CheckpointResultToString(Read.Result));
	}

	// A byte in the middle of the STATE payload, which no header field covers.
	{
		std::vector<uint8> Bent = Original;
		Bent[Bent.size() / 2] ^= 0x40u;
		CheckpointView View;
		const CheckpointRefusal Read = ReadCheckpoint(Bent.data(), Bent.size(), View);
		VT_CHECK_MSG(Read.Result != CheckpointResult::Ok, "it was read as Ok, not refused (%s)",
					 CheckpointResultToString(Read.Result));
	}

	// And truncation: a container cut short of its trailer.
	{
		std::vector<uint8> Bent = Original;
		Bent.resize(Bent.size() - 1u);
		CheckpointView View;
		const CheckpointRefusal Read = ReadCheckpoint(Bent.data(), Bent.size(), View);
		VT_CHECK_MSG(Read.Result != CheckpointResult::Ok, "it was read as Ok, not refused (%s)",
					 CheckpointResultToString(Read.Result));
	}

	// The untouched bytes still read, in the same run, so that a refusal of
	// everything cannot be mistaken for a working guard.
	{
		CheckpointView View;
		VT_CHECK(ReadCheckpoint(Original.data(), Original.size(), View).Result == CheckpointResult::Ok);
	}
}

VAELEN_TEST(Containers, AnImageIsNotAContainer)
{
	// The mistake the whole phase turns on, made deliberately: hand the reader
	// a `.snapshot` from the Phase 16 golden corpus and it must say BadMagic
	// rather than half-parse it. Three planning angles wrote gate clauses over
	// files of the wrong format, and this is what would have caught them.
	std::vector<uint8> Image;
	VT_CHECK(ReadWhole(std::string(VAELEN_GOLDEN_DIR) + "/bare-16.snapshot", Image));
	CheckpointView View;
	const CheckpointRefusal Read = ReadCheckpoint(Image.data(), Image.size(), View);
	VT_CHECK_MSG(Read.Result == CheckpointResult::BadMagic, "%s", CheckpointResultToString(Read.Result));
	VT_CHECK(View.Sections.empty());
}
