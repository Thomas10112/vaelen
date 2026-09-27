// VAELEN - VaelenRun tests
// Phase 16 task 16.07: a write that cannot destroy the last good save.
//
// The interface is in the kernel and knows nothing about paths; the stdio
// implementation under Tools/Store is the host's. What is tested here is the
// one promise that makes the interface worth having: a write that FAILS must
// leave the previous checkpoint whole, because a full disk taking a player's
// only save is a far worse day than a full disk refusing them a new one.
//
// STATUS: PROTOTYPE (Phase 16)
#include "VaelenTest.h"

#include "StdioCheckpointStore.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Store.h"

#include <cstdio>
#include <filesystem>
#include <cstring>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;
using VaelenHost::StdioCheckpointStore;

namespace
{
	/// A directory this test may fill. VAELEN_STORE_DIR comes from CMake.
	std::string Somewhere()
	{
		return std::string(VAELEN_STORE_DIR) + "/";
	}

	std::vector<uint8> APlausibleCheckpoint()
	{
		Options O;
		O.Size = 16u;
		O.PreHistory = 4u;
		O.Years = 4u;
		Aelvor A(O);
		std::vector<uint8> Bytes;
		if (!A.Begin() || BuildCheckpoint(A, Bytes) != CheckpointResult::Ok)
		{
			Bytes.clear();
		}
		return Bytes;
	}
} // namespace

VAELEN_TEST(Store, NamesThatWouldLeaveTheStoreAreRefused)
{
	// CHECKED IN THE KERNEL so that every implementation refuses the same
	// names. A store is handed names from a save-game menu, and a menu is
	// handed names by a person.
	VT_CHECK(IsUsableCheckpointName("autosave"));
	VT_CHECK(IsUsableCheckpointName("Aelvor 128 - day 400"));
	VT_CHECK_MSG(!IsUsableCheckpointName(""), "empty");
	VT_CHECK_MSG(!IsUsableCheckpointName(nullptr), "null");
	VT_CHECK_MSG(!IsUsableCheckpointName("../escape"), "a parent directory");
	VT_CHECK_MSG(!IsUsableCheckpointName("sub/dir"), "a forward separator");
	// BOTH SEPARATORS. A name checked only against '/' walks out of the store
	// on Windows, which is the one platform this project ships on.
	VT_CHECK_MSG(!IsUsableCheckpointName("sub\\dir"), "a backslash");
	VT_CHECK_MSG(!IsUsableCheckpointName("C:save"), "a drive letter");
	VT_CHECK_MSG(!IsUsableCheckpointName(".hidden"), "a leading dot");
	VT_CHECK_MSG(!IsUsableCheckpointName("bell\x07here"), "a control character");
	VT_CHECK_MSG(!IsUsableCheckpointName("star*"), "shell punctuation");
	// 16.14: A WRITE IN PROGRESS. Every store writes under `<name>.writing`
	// and moves it into place when whole (Run::WritingSuffix); the rule
	// refuses the suffix so an interrupted write's leftover is in no listing
	// and no save can hide behind a name the next write would replace.
	VT_CHECK_MSG(!IsUsableCheckpointName("day-400.writing"), "an interrupted write's temporary");
	VT_CHECK_MSG(IsUsableCheckpointName("writing-desk"), "a name that merely contains the word");
	VT_CHECK_MSG(IsUsableCheckpointName("still.writing.on"), "the suffix anywhere but the end");

	StdioCheckpointStore Store(Somewhere());
	std::vector<uint8> Out;
	VT_CHECK_MSG(Store.Write("../escape", reinterpret_cast<const uint8*>("x"), 1u) == StoreResult::BadName,
				 "and the store refuses them too, rather than trusting its caller");
	VT_CHECK(Store.Read("../escape", Out) == StoreResult::BadName);
}

VAELEN_TEST(Store, AtomicAndComplete)
{
	StdioCheckpointStore Store(Somewhere());
	const std::vector<uint8> Image = APlausibleCheckpoint();
	VT_REQUIRE(!Image.empty());

	VT_CHECK(Store.Write("first", Image.data(), Image.size()) == StoreResult::Ok);
	std::vector<uint8> Back;
	VT_CHECK(Store.Read("first", Back) == StoreResult::Ok);
	VT_CHECK_MSG(Back.size() == Image.size(), "the same length back");
	VT_REQUIRE(Back.size() == Image.size());
	VT_CHECK_MSG(std::memcmp(Back.data(), Image.data(), Image.size()) == 0, "and the same bytes, memcmp 0");

	VT_CHECK(Store.Read("never-written", Back) == StoreResult::NotFound);

	// A TRUNCATED FILE IS REFUSED BY NAME AND NEVER CRASHES. Cut where a reader
	// is mid-section, not just at the trailer.
	for (const uint32 Percent : {10u, 50u, 90u})
	{
		const usize Keep = (Image.size() * Percent) / 100u;
		const std::string Path = Somewhere() + "cut";
		std::FILE* F = std::fopen(Path.c_str(), "wb");
		VT_REQUIRE(F != nullptr);
		std::fwrite(Image.data(), 1, Keep, F);
		std::fclose(F);
		// 17.03 removed `Remember`: the store reads the directory, so a file
		// written behind its back is listed because it is THERE.

		std::vector<uint8> Short;
		VT_CHECK_MSG(Store.Read("cut", Short) == StoreResult::Ok,
					 "the STORE hands back what is there - it is not the store's job to judge bytes");
		CheckpointView View;
		const CheckpointRefusal R = ReadCheckpoint(Short.data(), Short.size(), View);
		VT_CHECK_MSG(R.Result != CheckpointResult::Ok, "and the CONTAINER refuses them at %u%%: %s", Percent,
					 CheckpointResultToString(R.Result));
	}
	Store.Forget("cut");
}

VAELEN_TEST(Store, AFullDiskTakesTheNewSaveAndNotTheOldOne)
{
	// THE SHARP ARM, and the reason this interface exists at all.
	//
	// A store is given a good checkpoint, then asked to overwrite it with one
	// that cannot be written. The new save must fail, and the OLD ONE MUST
	// STILL BE THERE, byte for byte, and must still read as a container.
	//
	// The disk is filled by pointing the store at a place that cannot be
	// written rather than by actually exhausting the volume: a test that fills
	// a CI runner's disk is a test that breaks every other test on the leg.
	StdioCheckpointStore Good(Somewhere());
	const std::vector<uint8> Image = APlausibleCheckpoint();
	VT_REQUIRE(!Image.empty());
	VT_REQUIRE(Good.Write("precious", Image.data(), Image.size()) == StoreResult::Ok);

	// An unwritable directory. The store must refuse and must not have touched
	// anything at the final name.
	StdioCheckpointStore Nowhere(Somewhere() + "no-such-directory");
	const StoreResult R = Nowhere.Write("precious", Image.data(), Image.size());
	VT_CHECK_MSG(R == StoreResult::CannotWrite, "%s", StoreResultToString(R));

	// AND THE ONE THAT WAS ALREADY THERE IS UNTOUCHED.
	std::vector<uint8> Still;
	VT_CHECK(Good.Read("precious", Still) == StoreResult::Ok);
	VT_CHECK_MSG(Still.size() == Image.size() && std::memcmp(Still.data(), Image.data(), Image.size()) == 0,
				 "the previous checkpoint is byte-identical");
	CheckpointView View;
	VT_CHECK_MSG(ReadCheckpoint(Still.data(), Still.size(), View).Result == CheckpointResult::Ok,
				 "and still reads as a container");

	// THE CONTROL THE ROW ASKS FOR, run rather than described. A direct
	// truncating open over the final name is what this class deliberately does
	// NOT do; here it is done by hand, and the previous checkpoint is gone.
	{
		const std::string Path = Somewhere() + "doomed";
		VT_REQUIRE(Good.Write("doomed", Image.data(), Image.size()) == StoreResult::Ok);
		std::FILE* F = std::fopen(Path.c_str(), "wb"); // truncates NOW
		VT_REQUIRE(F != nullptr);
		std::fwrite(Image.data(), 1, Image.size() / 4u, F); // and then "runs out"
		std::fclose(F);

		std::vector<uint8> Ruined;
		VT_CHECK(Good.Read("doomed", Ruined) == StoreResult::Ok);
		VT_CHECK_MSG(Ruined.size() != Image.size(), "a truncating open destroyed the previous save: %zu bytes of %zu",
					 Ruined.size(), Image.size());
		CheckpointView Gone;
		VT_CHECK_MSG(ReadCheckpoint(Ruined.data(), Ruined.size(), Gone).Result != CheckpointResult::Ok,
					 "and what is left is not a container - which is what temp-then-rename prevents");
		Good.Forget("doomed");
	}

	// The list knows what it holds, from the containers' own headers.
	const std::vector<StoreEntry> Held = Good.List();
	VT_CHECK_MSG(!Held.empty(), "the store lists what it wrote");
	bool FoundPrecious = false;
	for (const StoreEntry& E : Held)
	{
		if (E.Name == "precious")
		{
			FoundPrecious = true;
			VT_CHECK_MSG(E.Bytes == Image.size(), "with its length");
			VT_CHECK_MSG(E.ContainerVersion == CheckpointVersion, "and its container version");
			VT_CHECK_MSG(E.Digest != 0u, "and a digest read from the header, not from a re-load");
		}
	}
	VT_CHECK(FoundPrecious);
	Good.Forget("precious");
}

VAELEN_TEST(Store, AListingReadsHeadsAndTrailersAndNotFiles)
{
	// 22.02: StoreEntry promised fields "from the container's own header, so
	// a chooser can show a save's tick and version without loading two
	// gigabytes" - and both stores loaded every file whole and digested it,
	// a hundred megabytes a title page at the ship cell. The instrument is
	// the store's own count of bytes pulled off the disk: a listing of N
	// saves reads under two kilobytes and eight bytes of each, and says of
	// each exactly what a whole read says.
	StdioCheckpointStore Store(Somewhere() + "listing/");
	std::filesystem::create_directories(Somewhere() + "listing/");
	const std::vector<uint8> Image = APlausibleCheckpoint();
	VT_REQUIRE(!Image.empty());
	VT_REQUIRE(Store.Write("alpha", Image.data(), Image.size()) == StoreResult::Ok);
	VT_REQUIRE(Store.Write("beta", Image.data(), Image.size()) == StoreResult::Ok);
	// The whole read's description, the CONTROL this listing is held to.
	CheckpointView View;
	VT_REQUIRE(ReadCheckpoint(Image.data(), Image.size(), View).Result == CheckpointResult::Ok);
	const uint64 WholeDigest = ImageTrailer(View);
	VT_CHECK(WholeDigest != 0u);

	Store.BytesRead = 0;
	const std::vector<StoreEntry> Listed = Store.List();
	VT_REQUIRE(Listed.size() == 2u);
	for (const StoreEntry& E : Listed)
	{
		VT_CHECK_EQ(E.Bytes, static_cast<uint64>(Image.size()));
		VT_CHECK_EQ(E.Tick, View.Tick);
		VT_CHECK_EQ(E.ContainerVersion, View.Version);
		VT_CHECK_EQ(E.SectionCount, static_cast<uint32>(View.Sections.size()));
		VT_CHECK_DIGEST_EQ(E.Digest, WholeDigest);
	}
	// Under two kilobytes plus eight bytes a save, against a save many times that.
	VT_CHECK(Image.size() > 4u * CheckpointListingBytes);
	VT_CHECK_MSG(Store.BytesRead <= 2u * (CheckpointListingBytes + 8u),
				 "the listing read %llu bytes of two saves of %zu", static_cast<unsigned long long>(Store.BytesRead),
				 Image.size());
	VT_CHECK(Store.BytesRead > 0u);
	// CONTROL: a whole read costs the file.
	Store.BytesRead = 0;
	std::vector<uint8> Back;
	VT_REQUIRE(Store.Read("alpha", Back) == StoreResult::Ok);
	VT_CHECK_EQ(Store.BytesRead, static_cast<uint64>(Image.size()));

	// A listing BELIEVES the head: a byte of the payload flipped is not seen
	// by List and IS by Read's container - the division of labour, stated.
	std::vector<uint8> Flipped = Image;
	Flipped[Flipped.size() / 2u] ^= 0xFFu;
	VT_REQUIRE(Store.Write("gamma", Flipped.data(), Flipped.size()) == StoreResult::Ok);
	bool GammaListed = false;
	for (const StoreEntry& E : Store.List())
	{
		if (E.Name == "gamma")
		{
			GammaListed = true;
			VT_CHECK_EQ(E.Tick, View.Tick);
		}
	}
	VT_CHECK(GammaListed);
	VT_REQUIRE(Store.Read("gamma", Back) == StoreResult::Ok);
	CheckpointView Refused;
	VT_CHECK(ReadCheckpoint(Back.data(), Back.size(), Refused).Result == CheckpointResult::Corrupt);

	// A file that is no container - a stream beside the saves, a save cut
	// inside its table - is listed by name and size with every field 0.
	const std::string Stream = Somewhere() + "listing/notes";
	std::FILE* F = std::fopen(Stream.c_str(), "wb");
	VT_REQUIRE(F != nullptr);
	std::fwrite("not a container", 1, 15, F);
	std::fclose(F);
	const std::string Cut = Somewhere() + "listing/cut";
	F = std::fopen(Cut.c_str(), "wb");
	VT_REQUIRE(F != nullptr);
	std::fwrite(Image.data(), 1, CheckpointHeadBytes + 3u, F);
	std::fclose(F);
	// And the first half of a save, whose head and table read whole and whose
	// payload does not: no save either (what a write interrupted mid-copy
	// leaves under a plain name; Run.StoreColdProcess's control).
	const std::string Half = Somewhere() + "listing/half";
	F = std::fopen(Half.c_str(), "wb");
	VT_REQUIRE(F != nullptr);
	std::fwrite(Image.data(), 1, Image.size() / 2u, F);
	std::fclose(F);
	uint32 Zeroed = 0;
	for (const StoreEntry& E : Store.List())
	{
		if (E.Name == "notes" || E.Name == "cut" || E.Name == "half")
		{
			VT_CHECK(E.Tick == 0u && E.ContainerVersion == 0u && E.SectionCount == 0u && E.Digest == 0u);
			VT_CHECK(E.Bytes == (E.Name == "notes" ? 15u
								 : E.Name == "cut" ? CheckpointHeadBytes + 3u
												   : Image.size() / 2u));
			++Zeroed;
		}
	}
	VT_CHECK_EQ(Zeroed, 3u);
	// ReadPart's own edges: past the end is ShortRead, a missing name NotFound.
	std::vector<uint8> Part;
	VT_CHECK(Store.ReadPart("alpha", static_cast<uint64>(Image.size()) - 4u, 8u, Part) == StoreResult::ShortRead);
	VT_CHECK(Store.ReadPart("alpha", static_cast<uint64>(Image.size()) - 8u, 8u, Part) == StoreResult::Ok);
	VT_CHECK(Part.size() == 8u && std::memcmp(Part.data(), Image.data() + Image.size() - 8u, 8u) == 0);
	VT_CHECK(Store.ReadPart("nobody", 0, 8u, Part) == StoreResult::NotFound);
	for (const char* Name : {"alpha", "beta", "gamma", "notes", "cut", "half"})
	{
		Store.Forget(Name);
	}
}
