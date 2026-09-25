// VAELEN - VaelenRun tests
// Phase 17 task 17.08: the cache that is safe by accident, and the doors that
// keep it so.
//
// ADR-0150 records that `DiplomacySystem` (Diplomacy.cpp:72-75) rebuilds its
// region graph ONLY WHEN THE REGION COUNT CHANGES:
//
//   if (GraphRegions != static_cast<uint32>(N)) { Graph = BuildRegionGraph(...); }
//
// A map replaced by one with the same region count and a different shape would
// leave that graph stale, and nothing would say so. It is safe today only
// because a different map needs a different seed or a different size, and both
// are refused at every door into a world - `Aelvor::Adopt` by eight names,
// `LoadSnapshot` by two. The safety is a CONSEQUENCE OF CHECKS ELSEWHERE, not a
// property of the cache.
//
// GUARD NOW, FIX IF ASKED. Whether the cache should key on the map's revision
// instead (as `WorldGen::RegionGraphCache` does) is the owner's question 4,
// still open. Until it is answered, this test is what fails the day somebody
// loosens one of the ten refusals - and it fails NAMING THE DOOR and NAMING
// THE CACHE, so the person who loosened it finds this file and ADR-0150 rather
// than a stale graph in a diplomacy bug three phases later.
//
// The refusals themselves are tested elsewhere by name (Test_Checkpoint.cpp for
// Adopt, Test_Snapshot.cpp and Test_WorldMap.cpp for the kernel). This file is
// not that; it is the one place that says WHY all ten must stay shut together.
//
// STATUS: PROTOTYPE (Phase 17)
#include "VaelenTest.h"

#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/Snapshot.h"

#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	constexpr const char* TheCache =
		"DiplomacySystem's region graph (Diplomacy.cpp:72) is rebuilt only when the region COUNT changes, and this "
		"refusal is one of the ten that keep a same-count different map out of it (ADR-0150)";

	Options Declaring(uint32 Size, uint32 PreHistory, uint32 Years, uint64 Seed, bool Colony, bool Play, bool Lively,
					  bool Stream)
	{
		Options O;
		O.Size = Size;
		O.PreHistory = PreHistory;
		O.Years = Years;
		O.Seed = Seed;
		O.Colony = Colony;
		O.Play = Play;
		O.Lively = Lively;
		O.Stream = Stream;
		return O;
	}

	/// The count the cache keys on, computed the way DiplomacySystem computes
	/// it: the region graph's own count.
	uint32 RegionsOf(const Aelvor& A)
	{
		return WorldGen::BuildRegionGraph(A.Instance().Map(), A.Ages().World.Regions).RegionCount();
	}
} // namespace

VAELEN_TEST(RefusalsAreTheCachesSafety, TheDangerIsRealAndMeasured)
{
	// THE PREMISE, MEASURED RATHER THAN QUOTED. The cache is dangerous exactly
	// when two DIFFERENT maps have the SAME region count, because that is the
	// case its key cannot see. Six worlds at 32 tiles, seeds 1 to 6: how many
	// share a count with another?
	//
	// Nothing here asserts how many, because that is demography and would
	// make this a test of the generator. What is asserted is that the maps
	// differ - by state digest, pairwise - so that whatever the counts say,
	// they say it about different worlds.
	constexpr uint32 Worlds = 6;
	uint32 Counts[Worlds] = {};
	Hash64 Digests[Worlds] = {};
	for (uint32 I = 0; I < Worlds; ++I)
	{
		Aelvor A(Declaring(32u, 4u, 1u, 1u + I, false, false, false, false));
		VT_REQUIRE(A.Begin());
		Counts[I] = RegionsOf(A);
		Digests[I] = ComputeStateDigest(A.Instance());
		VT_CHECK_MSG(Counts[I] > 1u, "seed %u made a world of %u region(s)", 1u + I, Counts[I]);
	}
	uint32 SameCountPairs = 0;
	for (uint32 I = 0; I < Worlds; ++I)
	{
		for (uint32 J = I + 1; J < Worlds; ++J)
		{
			VT_CHECK_MSG(Digests[I] != Digests[J], "seeds %u and %u made the same world", 1u + I, 1u + J);
			if (Counts[I] == Counts[J])
			{
				++SameCountPairs;
			}
		}
	}
	std::printf("    refusals: six 32-tile worlds have %u, %u, %u, %u, %u, %u regions; %u pair(s) share a count and "
				"differ as worlds - each such pair is a stale graph the cache's key cannot see\n",
				Counts[0], Counts[1], Counts[2], Counts[3], Counts[4], Counts[5], SameCountPairs);
}

VAELEN_TEST(RefusalsAreTheCachesSafety, EveryDoorIntoAWorldIsShutAndSaysWhy)
{
	const Options Source = Declaring(32u, 6u, 6u, AelvorSeed, false, true, false, true);
	Aelvor A(Source);
	VT_REQUIRE(A.Begin());
	std::vector<uint8> Container;
	VT_REQUIRE(BuildCheckpoint(A, Container) == CheckpointResult::Ok);
	std::vector<uint8> Image;
	VT_REQUIRE(SaveSnapshot(A.Instance(), Image) == SnapshotResult::Ok);

	// THE CONTROL FIRST: the world it is actually of is accepted at both doors,
	// so that a test which refused everything could not pass.
	{
		Aelvor Same(Source);
		const Aelvor::AdoptResult Got = Same.Adopt(Container.data(), Container.size());
		VT_CHECK_MSG(Got == Aelvor::AdoptResult::Ok, "the matching world was refused by Adopt: %s",
					 Aelvor::AdoptResultToString(Got));
	}
	{
		Aelvor Same(Source);
		VT_REQUIRE(Same.Begin());
		const SnapshotResult Got = LoadSnapshot(Same.Instance(), Image.data(), Image.size());
		VT_CHECK_MSG(Got == SnapshotResult::Ok, "the matching world was refused by LoadSnapshot: %s",
					 SnapshotResultToString(Got));
	}

	// THE EIGHT DOORS OF ADOPT. Each host declares a world that differs from the
	// container's in exactly one respect, and each must be refused under its
	// own name. The seed is listed with the seven Options fields because it is
	// the OTHER way to get a different map with the same count.
	struct AdoptDoor
	{
		const char* Name;
		Options Host;
		Aelvor::AdoptResult Want;
	};
	const AdoptDoor AdoptDoors[] = {
		{"WrongSeed", Declaring(32u, 6u, 6u, AelvorSeed + 1u, false, true, false, true),
		 Aelvor::AdoptResult::WrongSeed},
		{"WorldSizeDiffers", Declaring(16u, 6u, 6u, AelvorSeed, false, true, false, true),
		 Aelvor::AdoptResult::WorldSizeDiffers},
		{"PreHistoryDiffers", Declaring(32u, 40u, 6u, AelvorSeed, false, true, false, true),
		 Aelvor::AdoptResult::PreHistoryDiffers},
		{"YearsDiffers", Declaring(32u, 6u, 40u, AelvorSeed, false, true, false, true),
		 Aelvor::AdoptResult::YearsDiffers},
		{"ColonyDiffers", Declaring(32u, 6u, 6u, AelvorSeed, true, true, false, true),
		 Aelvor::AdoptResult::ColonyDiffers},
		{"PlayDiffers", Declaring(32u, 6u, 6u, AelvorSeed, false, false, false, true),
		 Aelvor::AdoptResult::PlayDiffers},
		{"LivelyDiffers", Declaring(32u, 6u, 6u, AelvorSeed, false, true, true, true),
		 Aelvor::AdoptResult::LivelyDiffers},
		{"StreamDiffers", Declaring(32u, 6u, 6u, AelvorSeed, false, true, false, false),
		 Aelvor::AdoptResult::StreamDiffers},
	};
	uint32 Shut = 0;
	for (const AdoptDoor& D : AdoptDoors)
	{
		Aelvor Host(D.Host);
		const Aelvor::AdoptResult Got = Host.Adopt(Container.data(), Container.size());
		VT_CHECK_MSG(Got == D.Want, "the %s door is OPEN: Adopt answered %s where %s was required - %s", D.Name,
					 Aelvor::AdoptResultToString(Got), Aelvor::AdoptResultToString(D.Want), TheCache);
		VT_CHECK_MSG(!Host.Begun() && Host.Generations() == 0u,
					 "the %s door refused but left the host begun or generated", D.Name);
		Shut += Got == D.Want ? 1u : 0u;
	}

	// THE KERNEL'S OWN DOOR, which a host could call directly and bypass Adopt
	// altogether. The world is BEGUN, because the danger is a live world with a
	// real map taking on a different one's image. One of the two kernel
	// refusals the plan listed is here; the other is the next test, and it is
	// open.
	{
		Aelvor Other(Declaring(32u, 6u, 6u, AelvorSeed + 1u, false, true, false, true));
		VT_REQUIRE(Other.Begin());
		const SnapshotResult Got = LoadSnapshot(Other.Instance(), Image.data(), Image.size());
		VT_CHECK_MSG(Got == SnapshotResult::SeedMismatch,
					 "the SeedMismatch door is OPEN: LoadSnapshot answered %s - %s", SnapshotResultToString(Got),
					 TheCache);
		Shut += Got == SnapshotResult::SeedMismatch ? 1u : 0u;
	}

	VT_CHECK_MSG(Shut == 9u, "%u of 9 guarded doors shut; ADR-0150's cache is safe only while all of them are", Shut);
}

VAELEN_TEST(RefusalsAreTheCachesSafety, TheTenthDoorIsOpenAndPinnedHere)
{
	// THE FIRST RUN OF THIS FILE FOUND A DOOR OPEN, and this is where it is
	// written down rather than closed.
	//
	// The plan listed WorldShapeDiffers among the ten refusals. Handed a
	// 32-tile image, a BEGUN 16-tile world of the same seed answers `Ok` -
	// because `WorldMap::LayoutDigest` folds layer names and element sizes and
	// NO EXTENTS, which is Phase 16's defect 5, still pinned by
	// `Golden.TheLayoutDigestCannotTellTwoMapSizesApart`. 16.08 named the six
	// causes apart; it did not put the map's size into the digest.
	//
	// WHY IT IS NOT CLOSED HERE. The layout digest is written INTO the image
	// (Snapshot.cpp:435), so changing what it folds changes every image's
	// bytes, every trailer, and every frozen state digest in the repository -
	// gate clause (i). That is a re-freeze, like ADR-0131, and it lands as its
	// own commit or with the v4 bump, not inside a task called "guard now".
	//
	// WHAT IT EXPOSES, MEASURED. In production the kernel door is reached only
	// through `Aelvor::Adopt`, which refuses a different size by the HOST
	// section (16.10) before `LoadSnapshot` sees a byte - the second case of
	// the test above. A host that called `LoadSnapshot` directly would get a
	// world whose map is 32 wide while its Aelvor still declares 16: ADR-0150's
	// chimera. And DiplomacySystem's graph goes stale across that load exactly
	// when the two region counts coincide, which is measured below and printed
	// rather than asserted, because it is demography.
	//
	// THIS TEST FAILS THE DAY THE DOOR CLOSES. That is the point: whoever puts
	// the extents into the digest must find this file, ADR-0150, the golden
	// test and the re-freeze together, and rewrite all four to say the
	// opposite. Until then it says what is true.
	const Options Source = Declaring(32u, 6u, 6u, AelvorSeed, false, true, false, true);
	Aelvor A(Source);
	VT_REQUIRE(A.Begin());
	std::vector<uint8> Image;
	VT_REQUIRE(SaveSnapshot(A.Instance(), Image) == SnapshotResult::Ok);
	const uint32 RegionsInTheImage = RegionsOf(A);

	Aelvor Other(Declaring(16u, 6u, 6u, AelvorSeed, false, true, false, true));
	VT_REQUIRE(Other.Begin());
	const uint32 RegionsBefore = RegionsOf(Other);
	VT_CHECK_MSG(Other.Instance().Map().Config().Width == 16u, "the target really is 16 wide before the load");

	const SnapshotResult Got = LoadSnapshot(Other.Instance(), Image.data(), Image.size());
	VT_CHECK_MSG(Got == SnapshotResult::Ok,
				 "the kernel door has CLOSED: LoadSnapshot answered %s to a 32-tile image in a 16-tile world. If "
				 "that is by design now, defect 5 is fixed - rewrite this test, "
				 "Golden.TheLayoutDigestCannotTellTwoMapSizesApart and ADR-0150 to say so, in the commit that "
				 "re-froze the digests",
				 SnapshotResultToString(Got));
	VT_REQUIRE(Got == SnapshotResult::Ok);

	// The chimera, stated: the map is the image's, the declaration is the host's.
	VT_CHECK_MSG(Other.Instance().Map().Config().Width == 32u, "after the load the map is %u wide",
				 Other.Instance().Map().Config().Width);
	VT_CHECK_MSG(Other.Header().Size == 16u,
				 "and the Aelvor still declares %u - a walk recorded now names a world that does not exist",
				 Other.Header().Size);
	const uint32 RegionsAfter = RegionsOf(Other);
	VT_CHECK_MSG(RegionsAfter == RegionsInTheImage, "the loaded map has the image's %u regions, not %u",
				 RegionsInTheImage, RegionsAfter);

	std::printf("    refusals: the kernel door is open - 16-tile world (%u regions) took a 32-tile image (%u "
				"regions) of the same seed; DiplomacySystem's graph %s across that load\n",
				RegionsBefore, RegionsAfter,
				RegionsBefore == RegionsAfter ? "IS STALE (same count, different map)"
											  : "would be rebuilt (the counts differ, this time)");
}
