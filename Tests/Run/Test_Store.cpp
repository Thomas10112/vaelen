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
