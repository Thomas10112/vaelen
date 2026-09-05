// VAELEN - VaelenSim tests
// WorldMap: layer declaration, reset, typed access, digests, snapshot section
// through World, version-1 images rejected, unset and large maps.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Version.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldMap.h"

#include <cstring>
#include <vector>

using namespace Vaelen;

static_assert(VAELEN_SAVE_FORMAT_VERSION == 2, "02.01 introduced the map section as format 2");
static_assert(sizeof(WorldGenConfig) == 4 + 4 + 8 + 16);

namespace
{
	struct MapWorld
	{
		explicit MapWorld(uint64 Seed, bool WithMoisture = true) : Instance(Config(Seed))
		{
			Elevation = Instance.Map().AddLayer<int64>("elevation");
			Biome = Instance.Map().AddLayer<uint8>("biome");
			if (WithMoisture)
			{
				Moisture = Instance.Map().AddLayer<uint16>("moisture");
			}
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		void Generate(uint32 W, uint32 H)
		{
			WorldGenConfig Gen;
			Gen.Width = W;
			Gen.Height = H;
			Gen.SeaLevel = 1234;
			if (!Instance.Map().Reset(Gen))
			{
				return; // the tests check Grid() afterwards
			}
			TileLayer<int64>& E = Instance.Map().GetLayer(Elevation);
			TileLayer<uint8>& B = Instance.Map().GetLayer(Biome);
			for (uint32 i = 0; i < Instance.Map().Grid().TileCount(); ++i)
			{
				E[i] = static_cast<int64>(i) * 31 - 1000;
				B[i] = static_cast<uint8>(i % 7);
			}
		}
		std::vector<uint8> Save() const
		{
			std::vector<uint8> Bytes;
			SaveSnapshot(Instance, Bytes);
			return Bytes;
		}
		World Instance;
		TileLayerId<int64> Elevation;
		TileLayerId<uint8> Biome;
		TileLayerId<uint16> Moisture;
	};
} // namespace

VAELEN_TEST(WorldMap, LayersAreDeclaredResetAndAccessedByTypedId)
{
	WorldMap Map;
	VT_CHECK(!Map.IsReady());
	const TileLayerId<int64> E = Map.AddLayer<int64>("elevation");
	const TileLayerId<uint8> B = Map.AddLayer<uint8>("biome");
	VT_CHECK(E.IsValid() && B.IsValid() && E.Index == 0 && B.Index == 1);
	VT_CHECK_EQ(Map.LayerCount(), 2u);
	VT_CHECK_EQ(Map.GetLayer(E).Count(), 0u);

	WorldGenConfig Gen;
	Gen.Width = 8;
	Gen.Height = 4;
	VT_CHECK(Map.Reset(Gen));
	VT_CHECK(Map.IsReady());
	VT_CHECK((Map.Grid() == WorldGrid{8, 4}));
	VT_CHECK_EQ(Map.GetLayer(E).Count(), 32u);
	VT_CHECK_EQ(Map.GetLayer(B).Count(), 32u);
	Map.GetLayer(E).At(Map.Grid(), {7, 3}) = 99;
	VT_CHECK(Map.GetLayer(E)[31] == 99);
	// A layer added after Reset is sized immediately.
	const TileLayerId<uint16> M = Map.AddLayer<uint16>("moisture");
	VT_CHECK_EQ(Map.GetLayer(M).Count(), 32u);
	// Reset zeroes everything and may change the size.
	Gen.Width = 2;
	Gen.Height = 2;
	VT_CHECK(Map.Reset(Gen));
	VT_CHECK_EQ(Map.GetLayer(E).Count(), 4u);
	VT_CHECK(Map.GetLayer(E)[3] == 0);
}

VAELEN_TEST(WorldMap, DigestsFollowLayoutAndState)
{
	WorldMap A;
	WorldMap B;
	A.AddLayer<int64>("elevation");
	B.AddLayer<int64>("elevation");
	VT_CHECK_EQ(A.LayoutDigest(), B.LayoutDigest());
	VT_CHECK_EQ(A.StateDigest(), B.StateDigest());
	B.AddLayer<uint8>("biome");
	VT_CHECK_NE(A.LayoutDigest(), B.LayoutDigest());
	WorldMap C;
	C.AddLayer<int32>("elevation"); // same name, other element size
	VT_CHECK_NE(A.LayoutDigest(), C.LayoutDigest());

	WorldGenConfig Gen;
	Gen.Width = 3;
	Gen.Height = 3;
	VT_CHECK(A.Reset(Gen));
	const Hash64 Fresh = A.StateDigest();
	A.GetLayer(TileLayerId<int64>{0})[4] = 1;
	VT_CHECK_NE(A.StateDigest(), Fresh);
	A.GetLayer(TileLayerId<int64>{0})[4] = 0;
	VT_CHECK_EQ(A.StateDigest(), Fresh);
	Gen.SeaLevel = 5;
	VT_CHECK(A.Reset(Gen));
	VT_CHECK_NE(A.StateDigest(), Fresh);
}

VAELEN_TEST(WorldMap, MisuseIsReportedAndSafe)
{
	VaelenTest::ScopedAssertCapture Capture;
	WorldMap Map;
	const TileLayerId<int64> E = Map.AddLayer<int64>("elevation");
	VT_CHECK(!Map.AddLayer<int64>("elevation").IsValid()); // duplicate name
	VT_CHECK_EQ(Map.LayerCount(), 1u);
	WorldGenConfig Bad;
	Bad.Width = 0;
	VT_CHECK(!Map.Reset(Bad));
	VT_CHECK(!Map.IsReady());
	// Wrong element size or unknown index: the scratch layer, never a crash.
	VT_CHECK_EQ(Map.GetLayer(TileLayerId<uint8>{E.Index}).Count(), 0u);
	VT_CHECK_EQ(Map.GetLayer(TileLayerId<int64>{42}).Count(), 0u);
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 4);
#endif
}

VAELEN_TEST(WorldMap, SnapshotSectionRoundTripsThroughTheWorld)
{
	MapWorld A(11);
	A.Generate(16, 9);
	A.Instance.Map().GetLayer(A.Moisture)[100] = 0xbeef;
	const std::vector<uint8> Image = A.Save();

	MapWorld B(11);
	VT_REQUIRE(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(B.Instance.Map().Config() == A.Instance.Map().Config());
	VT_CHECK((B.Instance.Map().Grid() == WorldGrid{16, 9}));
	VT_CHECK(B.Instance.Map().GetLayer(B.Elevation).Data() == A.Instance.Map().GetLayer(A.Elevation).Data());
	VT_CHECK(B.Instance.Map().GetLayer(B.Biome).Data() == A.Instance.Map().GetLayer(A.Biome).Data());
	VT_CHECK(B.Instance.Map().GetLayer(B.Moisture)[100] == 0xbeef);
	VT_CHECK_EQ(B.Instance.Map().StateDigest(), A.Instance.Map().StateDigest());
	VT_CHECK(B.Save() == Image);
	VT_CHECK_EQ(ComputeStateDigest(B.Instance), ComputeStateDigest(A.Instance));

	// A different layer set is a layout mismatch, even with the same config.
	MapWorld C(11, /*WithMoisture=*/false);
	VT_CHECK(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::LayoutMismatch);
}

VAELEN_TEST(WorldMap, UnsetMapRoundTripsAndVersionOneIsRejected)
{
	MapWorld A(3);
	const std::vector<uint8> Image = A.Save(); // map never Reset: 0 x 0, empty layers
	MapWorld B(3);
	VT_CHECK(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK(!B.Instance.Map().IsReady());
	VT_CHECK_EQ(B.Instance.Map().GetLayer(B.Elevation).Count(), 0u);
	VT_CHECK(B.Save() == Image);

	// An image stamped with the previous format is refused before any state changes.
	std::vector<uint8> Old = Image;
	uint8* Bytes = Old.data();
	VT_REQUIRE(Bytes != nullptr && Old.size() > 16); // keeps gcc -O2 -Wnull-dereference honest
	uint32 Version = 1;
	std::memcpy(Bytes + 8, &Version, 4);
	const Hash64 Digest = HashBytes(reinterpret_cast<const char*>(Bytes), Old.size() - 8);
	std::memcpy(Bytes + Old.size() - 8, &Digest, 8);
	MapWorld C(3);
	C.Generate(4, 4);
	const Hash64 Before = C.Instance.Map().StateDigest();
	VT_CHECK(LoadSnapshot(C.Instance, Old.data(), Old.size()) == SnapshotResult::VersionMismatch);
	VT_CHECK_EQ(C.Instance.Map().StateDigest(), Before);
}

VAELEN_TEST(WorldMap, LargeMapRoundTrips)
{
	MapWorld A(2024);
	A.Generate(1024, 1024);
	const std::vector<uint8> Image = A.Save();
	VT_CHECK(Image.size() > 1024u * 1024u * (8u + 1u + 2u));
	MapWorld B(2024);
	VT_REQUIRE(LoadSnapshot(B.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(B.Instance.Map().StateDigest(), A.Instance.Map().StateDigest());
	VT_CHECK(B.Instance.Map().GetLayer(B.Elevation)[1048575] == A.Instance.Map().GetLayer(A.Elevation)[1048575]);
}
