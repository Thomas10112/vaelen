// VAELEN - VaelenRun tests
// Phase 17 task 17.03: a store that lists a directory it did not fill, and
// reports the digest it says it reports.
//
// TWO DEFECTS, BOTH FROM 16.07, AND BOTH MINE.
//
// The first: `List()` iterated a private `std::vector<std::string> Written`
// that only `Write()` and `Remember()` ever filled. A host started fresh and
// pointed at a folder full of saves was told it was empty - which defeats the
// one thing a save browser is for. `Run.Store` passed the whole time because it
// wrote and listed in ONE process, with ONE store object: an instrument blind
// to the only dimension that matters.
//
// The second: the entry's digest was `View.Sections.front().Digest`, reported
// after the result of `View.Find(SectionKind::State, Length)` had been called
// and thrown away. Two faults in one line, and making this test fail on purpose
// reversed which of them matters. The SECTION digest is not the image TRAILER -
// different numbers over the same bytes, on EVERY container, whatever the
// section order: e0614906cb8a5676 where `ComputeStateDigest` returns
// 0f6fa26b35d09a70. That was wrong always. Reading by POSITION was the second
// fault, ADR-0150's "safe by accident", and it had not started mattering yet.
//
// THE PRE-FIX BEHAVIOUR IS KEPT HERE, as `AWrittenOnlyStore` below, and both
// arms run in the same test. A test that only shows the new code working
// cannot show that the old code was broken, and in a year nobody will
// reconstruct why this file exists.
//
// STATUS: PROTOTYPE (Phase 17)
#include "VaelenTest.h"

#include "StdioCheckpointStore.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Store.h"
#include "Vaelen/Sim/Snapshot.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;
using VaelenHost::StdioCheckpointStore;

namespace
{
	/// A directory of this test's own, so that `Run.Store`'s files cannot be
	/// counted here and a stale run cannot make this one pass.
	std::string Somewhere()
	{
		return std::string(VAELEN_STORE_DIR) + "/cold/";
	}

	bool MakeTheDirectory()
	{
		// No <filesystem> needed for one level, and the CMake entry has already
		// created it; this is belt and braces for a developer running the
		// binary by hand.
		const std::string Path = Somewhere();
		std::FILE* Probe = std::fopen((Path + ".probe").c_str(), "wb");
		if (Probe == nullptr)
		{
			return false;
		}
		std::fclose(Probe);
		std::remove((Path + ".probe").c_str());
		return true;
	}

	/// THE PRE-FIX STORE, kept deliberately.
	///
	/// This is `StdioCheckpointStore::List` as 16.07 wrote it: a listing over
	/// names the object watched go past, and a digest taken from the first row
	/// of the section table. It shares the real store's `Write` and `Read`, so
	/// the only difference between it and the fixed one is the defect.
	class AWrittenOnlyStore final : public StdioCheckpointStore
	{
	public:
		using StdioCheckpointStore::StdioCheckpointStore;

		StoreResult Write(const char* Name, const uint8* Bytes, usize Size) override
		{
			const StoreResult Result = StdioCheckpointStore::Write(Name, Bytes, Size);
			if (Result == StoreResult::Ok)
			{
				Written.emplace_back(Name);
			}
			return Result;
		}

		std::vector<StoreEntry> List() override
		{
			std::vector<StoreEntry> Out;
			for (const std::string& Name : Written)
			{
				std::vector<uint8> Bytes;
				if (Read(Name.c_str(), Bytes) != StoreResult::Ok)
				{
					continue;
				}
				StoreEntry Entry;
				Entry.Name = Name;
				Entry.Bytes = static_cast<uint64>(Bytes.size());
				CheckpointView View;
				if (ReadCheckpoint(Bytes.data(), Bytes.size(), View).Result == CheckpointResult::Ok)
				{
					Entry.Tick = View.Tick;
					Entry.ContainerVersion = View.Version;
					uint64 Length = 0;
					if (View.Find(SectionKind::State, Length) != nullptr && !View.Sections.empty())
					{
						// The line as it stood: the Find's answer is discarded
						// and the digest comes from position 0.
						Entry.Digest = View.Sections.front().Digest;
					}
				}
				Out.push_back(std::move(Entry));
			}
			return Out;
		}

	private:
		std::vector<std::string> Written;
	};

	struct Made
	{
		std::vector<uint8> Bytes;
		uint64 Tick = 0;
		Hash64 Trailer = 0;
		uint32 Sections = 0;
	};

	/// A container, and the figures a listing must later agree with - taken
	/// from the world and from `ComputeStateDigest`, never from the listing.
	Made AContainer(uint32 Size, uint32 Years)
	{
		Made M;
		Options O;
		O.Size = Size;
		O.PreHistory = 4u;
		O.Years = Years;
		Aelvor A(O);
		if (!A.Begin() || BuildCheckpoint(A, M.Bytes) != CheckpointResult::Ok)
		{
			M.Bytes.clear();
			return M;
		}
		M.Tick = A.Instance().Clock().Now();
		M.Trailer = ComputeStateDigest(A.Instance());
		CheckpointView View;
		if (ReadCheckpoint(M.Bytes.data(), M.Bytes.size(), View).Result == CheckpointResult::Ok)
		{
			M.Sections = static_cast<uint32>(View.Sections.size());
		}
		return M;
	}

	const StoreEntry* Named(const std::vector<StoreEntry>& Entries, const char* Name)
	{
		for (const StoreEntry& E : Entries)
		{
			if (E.Name == Name)
			{
				return &E;
			}
		}
		return nullptr;
	}

	void Sweep(StdioCheckpointStore& Store)
	{
		for (const StoreEntry& E : Store.List())
		{
			Store.Forget(E.Name.c_str());
		}
	}
} // namespace

VAELEN_TEST(StoreColdProcess, AColdStoreListsTheDirectoryAndTheOldOneListsNothing)
{
	VT_REQUIRE(MakeTheDirectory());
	{
		StdioCheckpointStore Sweeper(Somewhere());
		Sweep(Sweeper);
	}

	const Made First = AContainer(16u, 2u);
	const Made Second = AContainer(16u, 4u);
	const Made Third = AContainer(24u, 2u);
	VT_REQUIRE(!First.Bytes.empty() && !Second.Bytes.empty() && !Third.Bytes.empty());

	// ONE STORE WRITES...
	{
		StdioCheckpointStore Writer(Somewhere());
		VT_CHECK(Writer.Write("alpha", First.Bytes.data(), First.Bytes.size()) == StoreResult::Ok);
		VT_CHECK(Writer.Write("beta", Second.Bytes.data(), Second.Bytes.size()) == StoreResult::Ok);
		VT_CHECK(Writer.Write("gamma", Third.Bytes.data(), Third.Bytes.size()) == StoreResult::Ok);
	}

	// ...AND A STORE THAT NEVER SAW IT HAPPEN LISTS ALL THREE. This object has
	// written nothing; everything it knows, it read off the disk.
	StdioCheckpointStore Cold(Somewhere());
	const std::vector<StoreEntry> Listed = Cold.List();
	VT_CHECK_MSG(Listed.size() == 3u, "a cold store listed %zu of 3", Listed.size());

	struct Want
	{
		const char* Name;
		const Made* What;
	};
	const Want Wanted[] = {{"alpha", &First}, {"beta", &Second}, {"gamma", &Third}};
	for (const Want& W : Wanted)
	{
		const StoreEntry* Entry = Named(Listed, W.Name);
		VT_CHECK_MSG(Entry != nullptr, "%s was not listed", W.Name);
		if (Entry == nullptr)
		{
			continue;
		}
		VT_CHECK_MSG(Entry->Bytes == W.What->Bytes.size(), "%s: %llu bytes listed, %zu written", W.Name,
					 static_cast<unsigned long long>(Entry->Bytes), W.What->Bytes.size());
		VT_CHECK_MSG(Entry->Tick == W.What->Tick, "%s: tick %llu listed, %llu written", W.Name,
					 static_cast<unsigned long long>(Entry->Tick), static_cast<unsigned long long>(W.What->Tick));
		VT_CHECK_MSG(Entry->ContainerVersion == CheckpointVersion, "%s: container version %u", W.Name,
					 Entry->ContainerVersion);
		VT_CHECK_MSG(Entry->SectionCount == W.What->Sections, "%s: %u sections listed, %u written", W.Name,
					 Entry->SectionCount, W.What->Sections);
		// THE DIGEST IS THE IMAGE TRAILER, checked against ComputeStateDigest
		// over the world that was saved - not against another listing.
		VT_CHECK_MSG(Entry->Digest == W.What->Trailer, "%s: listed %016llx, ComputeStateDigest %016llx", W.Name,
					 static_cast<unsigned long long>(Entry->Digest), static_cast<unsigned long long>(W.What->Trailer));
	}

	// The listing is sorted, so two hosts over one directory agree on order.
	for (usize I = 1; I < Listed.size(); ++I)
	{
		VT_CHECK_MSG(Listed[I - 1].Name < Listed[I].Name, "'%s' is listed before '%s'", Listed[I - 1].Name.c_str(),
					 Listed[I].Name.c_str());
	}

	// THE PRE-FIX ARM, IN THE SAME RUN. The store as 16.07 wrote it, over the
	// same three files, from an object that did not write them: it lists NONE
	// of them. That is the defect, and it stays visible here after the fix.
	AWrittenOnlyStore AsItWas(Somewhere());
	const std::vector<StoreEntry> Nothing = AsItWas.List();
	VT_CHECK_MSG(Nothing.empty(),
				 "the pre-fix store listed %zu entries; it is supposed to list none, and if it now "
				 "lists three this arm is no longer testing anything",
				 Nothing.size());

	// And the same object, once it HAS written, lists what it wrote - so the
	// arm above is about coldness and not about the class being broken outright.
	VT_CHECK(AsItWas.Write("delta", First.Bytes.data(), First.Bytes.size()) == StoreResult::Ok);
	VT_CHECK_MSG(AsItWas.List().size() == 1u, "the pre-fix store lists what it wrote in its own lifetime");
	VT_CHECK_MSG(Cold.List().size() == 4u, "and the fixed store sees that fourth file because it is THERE");

	Sweep(Cold);
}

VAELEN_TEST(StoreColdProcess, TheDigestIsTheTrailerAndNotWhicheverSectionIsFirst)
{
	// THE SECOND CONTROL THE GATE ASKS FOR: a container in which STATE is not
	// the first section, and the reported digest must be unchanged.
	//
	// AND WRITING IT FOUND SOMETHING ABOUT THE FORMAT. The first version simply
	// rotated the table ROWS and was refused `BadSectionTable`, which is
	// correct and is not a bug: `ReadCheckpoint` requires the rows to be in
	// ascending offset order, requires them not to overlap, and requires them
	// to claim every payload byte between the table and the trailer. So a table
	// whose order disagrees with its payloads is not a legal container at all.
	//
	// That narrows the defect rather than dismissing it. A legal container with
	// STATE second exists - the PAYLOADS have to move with the rows - and this
	// builds one: the first two sections are swapped bytes and all. Nothing in
	// this tree writes such a file today, because `BuildCheckpoint` always puts
	// STATE first, which is precisely why 16.07's `Sections.front()` was right.
	// It was right BY ACCIDENT (ADR-0150), and a writer that one day puts a
	// cheap section first so a browser can read it without seeking is a
	// reasonable thing to build.
	VT_REQUIRE(MakeTheDirectory());
	{
		StdioCheckpointStore Sweeper(Somewhere());
		Sweep(Sweeper);
	}

	const Made M = AContainer(16u, 2u);
	VT_REQUIRE(!M.Bytes.empty());
	VT_REQUIRE(M.Sections >= 2u);

	CheckpointView Was;
	VT_REQUIRE(ReadCheckpoint(M.Bytes.data(), M.Bytes.size(), Was).Result == CheckpointResult::Ok);
	const SectionEntry A = Was.Sections[0];
	const SectionEntry B = Was.Sections[1];
	VT_CHECK_MSG(A.Kind == static_cast<uint16>(SectionKind::State), "section 0 is not STATE; this test assumes the "
																	"layout BuildCheckpoint writes");
	VT_REQUIRE(A.Offset + A.Length == B.Offset);

	constexpr usize HeaderBytes = 56;
	constexpr usize EntryBytes = 30;

	// Swap the two sections, payloads and rows together, so the result is a
	// container the reader accepts and not a corrupt one.
	std::vector<uint8> Swapped = M.Bytes;
	const usize At = static_cast<usize>(A.Offset);
	const std::vector<uint8> APayload(M.Bytes.begin() + static_cast<long>(A.Offset),
									  M.Bytes.begin() + static_cast<long>(A.Offset + A.Length));
	const std::vector<uint8> BPayload(M.Bytes.begin() + static_cast<long>(B.Offset),
									  M.Bytes.begin() + static_cast<long>(B.Offset + B.Length));
	std::memcpy(Swapped.data() + At, BPayload.data(), BPayload.size());
	std::memcpy(Swapped.data() + At + BPayload.size(), APayload.data(), APayload.size());

	// Row 0 becomes B at the old offset; row 1 becomes A, just after it.
	uint8* const Table = Swapped.data() + HeaderBytes;
	std::vector<uint8> RowA(Table, Table + EntryBytes);
	std::vector<uint8> RowB(Table + EntryBytes, Table + 2u * EntryBytes);
	const auto PutU64 = [](uint8* Where, uint64 Value)
	{
		for (usize I = 0; I < sizeof(uint64); ++I)
		{
			Where[I] = static_cast<uint8>((Value >> (8u * I)) & 0xffu);
		}
	};
	PutU64(RowB.data() + 6, A.Offset);
	PutU64(RowA.data() + 6, A.Offset + B.Length);
	std::memcpy(Table, RowB.data(), EntryBytes);
	std::memcpy(Table + EntryBytes, RowA.data(), EntryBytes);

	// The trailer is FNV-1a over every preceding byte, so it has to be redone
	// or the container is simply corrupt and this would prove nothing.
	const Hash64 NewTrailer = HashBytes(reinterpret_cast<const char*>(Swapped.data()), Swapped.size() - sizeof(uint64));
	PutU64(Swapped.data() + Swapped.size() - sizeof(uint64), NewTrailer);

	CheckpointView Check;
	const CheckpointRefusal Read = ReadCheckpoint(Swapped.data(), Swapped.size(), Check);
	VT_CHECK_MSG(Read.Result == CheckpointResult::Ok,
				 "the reordered container must still READ, or this proves nothing about the digest: %s",
				 CheckpointResultToString(Read.Result));
	VT_REQUIRE(Read.Result == CheckpointResult::Ok);
	VT_CHECK_MSG(Check.Sections.front().Kind != static_cast<uint16>(SectionKind::State),
				 "STATE is still first, so the reorder did not happen and this test cannot bite");
	// The STATE section's BYTES must be the same ones, or the trailer inside it
	// would differ for a reason that has nothing to do with the defect.
	uint64 Length = 0;
	const uint8* State = Check.Find(SectionKind::State, Length);
	VT_REQUIRE(State != nullptr);
	VT_CHECK(Length == A.Length);
	VT_CHECK(std::memcmp(State, APayload.data(), APayload.size()) == 0);

	StdioCheckpointStore Store(Somewhere());
	VT_CHECK(Store.Write("swapped", Swapped.data(), Swapped.size()) == StoreResult::Ok);

	StdioCheckpointStore Cold(Somewhere());
	const StoreEntry* Entry = Named(Cold.List(), "swapped");
	VT_CHECK_MSG(Entry != nullptr, "the reordered container was not listed at all");
	if (Entry != nullptr)
	{
		VT_CHECK_MSG(Entry->Digest == M.Trailer,
					 "a reordered table changed the listed digest: %016llx, expected %016llx",
					 static_cast<unsigned long long>(Entry->Digest), static_cast<unsigned long long>(M.Trailer));
	}

	// AND THE PRE-FIX ARM, over the same file: the old code reports whatever
	// row is first, which is now a different section entirely. That is the
	// defect, measured rather than argued.
	AWrittenOnlyStore AsItWas(Somewhere());
	VT_CHECK(AsItWas.Write("swapped-again", Swapped.data(), Swapped.size()) == StoreResult::Ok);
	const std::vector<StoreEntry> Old = AsItWas.List();
	VT_REQUIRE(Old.size() == 1u);
	VT_CHECK_MSG(Old.front().Digest != M.Trailer,
				 "the pre-fix store happened to report the trailer; if that is now true this arm tests nothing");
	VT_CHECK_MSG(Old.front().Digest == Check.Sections.front().Digest,
				 "the pre-fix store reports section 0's digest by position: %016llx against %016llx",
				 static_cast<unsigned long long>(Old.front().Digest),
				 static_cast<unsigned long long>(Check.Sections.front().Digest));

	Sweep(Cold);
}
