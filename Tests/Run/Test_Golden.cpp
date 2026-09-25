// VAELEN - VaelenRun tests
// Phase 16 task 16.01: the golden save corpus still loads, and still says the
// same thing about itself.
//
// A golden is an image written by a build that no longer exists. These three
// were written at 867a129 on 2026-09-21, before this phase touched anything,
// because 16.08 changes what a refusal means and 16.09 puts a container round
// the image - and after those, no build in this repository can write an honest
// v3 file again. Tests/Run/Golden/README.md has the table and the sizes.
//
// WHAT THIS TEST IS FOR, and what it is not. It is the instrument every later
// task of Phase 16 is measured by: the day a migration lands, this is what says
// the old files still read. It is NOT evidence that the format handles a world
// with a long history - 16 tiles at ten years of pre-history is 78 KB and the
// same world at thirty is 11.4 MB, so the corpus is deliberately young.
//
// STATUS: PROTOTYPE (Phase 16)
#include "VaelenTest.h"

#include "Vaelen/Core/Version.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Sim/Snapshot.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	struct Golden
	{
		const char* Name;
		uint32 Size;
		uint32 PreHistory;
		uint32 Years;
		bool Full;
		Hash64 State;
		usize Bytes;
	};

	// The table of Tests/Run/Golden/README.md, in code, so that a file whose
	// bytes changed and a file whose MEANING changed fail differently.
	constexpr Golden Corpus[] = {
		{"bare-16.snapshot", 16, 10, 1, false, 0xd17d7fd9a6f09ea6ull, 77478},
		{"full-16.snapshot", 16, 10, 1, true, 0x0a88aacecd9aaa23ull, 78036},
		{"full-32.snapshot", 32, 10, 1, true, 0x3803ec5a28729144ull, 173414},
	};

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

	Options OptionsOf(const Golden& G)
	{
		Options O;
		O.Size = G.Size;
		O.PreHistory = G.PreHistory;
		O.Years = G.Years;
		// 18.10: the goldens are images of the world before Phase 18 and are
		// never regenerated (README.md:4); they load into the world they were
		// written by.
		O.Climate = false;
		if (G.Full)
		{
			O.Play = true;
			O.Stream = true;
			O.Lively = true;
			O.Colony = true;
		}
		return O;
	}
} // namespace

VAELEN_TEST(Golden, V3RoundTrips)
{
	// VAELEN_GOLDEN_DIR is handed in by Tests/Run/CMakeLists.txt so the test
	// runs from any working directory, which is what the CI presets do.
	const std::string Where = VAELEN_GOLDEN_DIR;
	VT_CHECK_MSG(VAELEN_SAVE_FORMAT_VERSION == 3u,
				 "the corpus is v3; a bumped version needs a migration, not a rewrite");

	for (const Golden& G : Corpus)
	{
		std::vector<uint8> OnDisk;
		VT_REQUIRE(ReadWhole(Where + "/" + G.Name, OnDisk)); // the file is where the README says
		VT_CHECK_MSG(OnDisk.size() == G.Bytes, "the file is the size the README records");

		// A world wired the same way, which today means generated the same way:
		// LoadSnapshot needs the component types, and the only way to get them
		// is Begin(). That cost - a whole world generation to load a save - is
		// what 16.06's Adopt exists to remove, and this line is where a reader
		// meets it.
		Aelvor A(OptionsOf(G));
		VT_REQUIRE(A.Begin());
		const SnapshotResult Res = LoadSnapshot(A.Instance(), OnDisk.data(), OnDisk.size());
		VT_CHECK_MSG(Res == SnapshotResult::Ok, "%s", SnapshotResultToString(Res));
		VT_REQUIRE(Res == SnapshotResult::Ok);
		VT_CHECK_MSG(A.StateDigest() == G.State, "and it is the world the README says it is");

		// AND IT WRITES ITSELF BACK. A loader that dropped a section would pass
		// every line above - the digest is the image's own trailer, so a world
		// that lost something and re-saved would agree with itself. Comparing
		// the BYTES against the file is what catches that.
		std::vector<uint8> Again;
		SaveSnapshot(A.Instance(), Again);
		VT_CHECK_MSG(Again.size() == OnDisk.size(), "a re-save is the same length");
		VT_CHECK_MSG(Again == OnDisk, "and the same bytes, so nothing was lost in the round trip");
	}
}

VAELEN_TEST(Golden, TheDoorTellsTwoMapSizesApartThoughTheDigestCannot)
{
	// Defect 5 of the twelve Phase 16 opened on: full-16 and full-32 are
	// worlds whose maps differ by a factor of four in area, and the layout
	// digest LoadSnapshot compares is equal for both - WorldMap::LayoutDigest
	// folds layer name hashes and element sizes, and no extents. THAT IS
	// STILL TRUE, and asserted below as a fact: folding extents would change
	// every image's bytes (a v4 bump, the owner's question).
	//
	// 18.09 closed the door the defect left open without touching the digest:
	// the map's reader refuses an image of another shape into a begun map, so
	// the larger world's image offered to the smaller answers WorldShapeDiffers
	// and leaves it as it was.
	Aelvor Small(OptionsOf(Corpus[1]));
	Aelvor Large(OptionsOf(Corpus[2]));
	VT_REQUIRE(Small.Begin());
	VT_REQUIRE(Large.Begin());
	const Hash64 A = HashCombine(Small.Instance().Types().LayoutDigest(), Small.Instance().Map().LayoutDigest());
	const Hash64 B = HashCombine(Large.Instance().Types().LayoutDigest(), Large.Instance().Map().LayoutDigest());
	VT_CHECK_MSG(A == B, "the two layout digests are equal - the digest cannot tell the sizes apart");
	VT_CHECK_MSG(Small.Instance().Map().Config().Width != Large.Instance().Map().Config().Width,
				 "though the maps plainly differ");
	std::vector<uint8> Image;
	VT_REQUIRE(SaveSnapshot(Large.Instance(), Image) == SnapshotResult::Ok);
	const Hash64 Before = ComputeStateDigest(Small.Instance());
	const SnapshotResult Got = LoadSnapshot(Small.Instance(), Image.data(), Image.size());
	VT_CHECK_MSG(Got == SnapshotResult::WorldShapeDiffers, "the door answered %s", SnapshotResultToString(Got));
	VT_CHECK_DIGEST_EQ(ComputeStateDigest(Small.Instance()), Before);
}
