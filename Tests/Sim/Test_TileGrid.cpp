// VAELEN - VaelenSim tests
// TileGrid: coordinates, row-major indexing, fixed neighbour order, bounds;
// TileLayer: reset, fill, hash, serialisation.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/TileGrid.h"

#include <vector>

using namespace Vaelen;

static_assert(IsPlainData<TileCoord>);
static_assert(WorldGrid{1, 1}.IsValid() && !WorldGrid{0, 1}.IsValid() && !WorldGrid{4097, 1}.IsValid());
static_assert(WorldGrid{4096, 4096}.TileCount() == 16777216u);
static_assert(WorldGrid{5, 3}.CoordOf(7) == TileCoord{2, 1});

VAELEN_TEST(TileGrid, IndexAndCoordAreExactInverses)
{
	const WorldGrid G{7, 5};
	VT_CHECK_EQ(G.TileCount(), 35u);
	for (uint32 i = 0; i < G.TileCount(); ++i)
	{
		const TileCoord C = G.CoordOf(i);
		VT_CHECK(G.Contains(C));
		VT_CHECK_EQ(G.IndexOf(C), i);
	}
	VT_CHECK(!G.Contains({-1, 0}) && !G.Contains({0, -1}) && !G.Contains({7, 0}) && !G.Contains({0, 5}));
	VT_CHECK_EQ(G.IndexOf({6, 4}), 34u);
}

VAELEN_TEST(TileGrid, NeighbourOrderIsFixedAndClippedAtTheBorder)
{
	const WorldGrid G{3, 3};
	std::vector<uint32> Slots;
	std::vector<TileCoord> Coords;
	G.ForEachNeighbour({1, 1}, 8,
					   [&](TileCoord N, uint32 Slot)
					   {
						   Slots.push_back(Slot);
						   Coords.push_back(N);
					   });
	VT_REQUIRE_EQ(Slots.size(), 8u);
	for (uint32 i = 0; i < 8; ++i)
	{
		VT_CHECK_EQ(Slots[i], i);
		VT_CHECK((Coords[i] == TileCoord{1 + NeighbourOffsets[i].X, 1 + NeighbourOffsets[i].Y}));
	}
	VT_CHECK((Coords[0] == TileCoord{1, 0})); // N
	VT_CHECK((Coords[2] == TileCoord{2, 1})); // E
	VT_CHECK((Coords[4] == TileCoord{1, 2})); // S
	VT_CHECK((Coords[6] == TileCoord{0, 1})); // W

	Slots.clear();
	G.ForEachNeighbour({0, 0}, 8, [&](TileCoord, uint32 Slot) { Slots.push_back(Slot); });
	VT_CHECK(Slots == std::vector<uint32>({2, 3, 4})); // E, SE, S only
	Slots.clear();
	G.ForEachNeighbour({0, 0}, 4, [&](TileCoord, uint32 Slot) { Slots.push_back(Slot); });
	VT_CHECK(Slots == std::vector<uint32>({2, 4}));
	Slots.clear();
	G.ForEachNeighbour({2, 2}, 4, [&](TileCoord, uint32 Slot) { Slots.push_back(Slot); });
	VT_CHECK(Slots == std::vector<uint32>({0, 6}));
	// A 1x1 grid has no neighbours at all.
	uint32 Visited = 0;
	WorldGrid{1, 1}.ForEachNeighbour({0, 0}, 8, [&](TileCoord, uint32) { ++Visited; });
	VT_CHECK_EQ(Visited, 0u);
}

VAELEN_TEST(TileGrid, LayerResetFillHashAndRoundTrip)
{
	const WorldGrid G{4, 3};
	TileLayer<int64> L(HashString("elevation"));
	VT_CHECK_EQ(L.Count(), 0u);
	L.Reset(G.TileCount());
	VT_CHECK_EQ(L.Count(), 12u);
	VT_CHECK(L[5] == 0);
	const Hash64 Zero = L.Hash();
	L.Fill(7);
	VT_CHECK(L.At(G, {3, 2}) == 7);
	VT_CHECK_NE(L.Hash(), Zero);
	L.At(G, {1, 2}) = -3;
	const Hash64 Before = L.Hash();

	std::vector<uint8> Bytes;
	{
		MemoryWriter W(Bytes);
		VT_CHECK(L.Serialize(W));
	}
	VT_CHECK_EQ(Bytes.size(), 8u + 12u * 8u);
	TileLayer<int64> M(HashString("elevation"));
	MemoryReader R(Bytes.data(), Bytes.size());
	VT_CHECK(M.Serialize(R));
	VT_CHECK(R.AtEnd());
	VT_CHECK(M.Data() == L.Data());
	VT_CHECK_EQ(M.Hash(), Before);
	// The same values under another name hash differently (the name seeds the digest).
	TileLayer<int64> N(HashString("moisture"));
	N.Reset(12);
	N.Fill(7);
	N[9] = -3;
	VT_CHECK_NE(N.Hash(), Before);
	// Truncated image rejected.
	MemoryReader Short(Bytes.data(), Bytes.size() - 1);
	VT_CHECK(!M.Serialize(Short));
}

VAELEN_TEST(TileGrid, OutOfRangeAccessIsReportedAndSafe)
{
	VaelenTest::ScopedAssertCapture Capture;
	const WorldGrid G{2, 2};
	VT_CHECK_EQ(G.IndexOf({5, 5}), 0u);
	TileLayer<uint8> L(1);
	L.Reset(4);
	L[3] = 9;
	VT_CHECK(L[99] == L[0]);
	uint32 Visited = 0;
	G.ForEachNeighbour({0, 0}, 5, [&](TileCoord, uint32) { ++Visited; });
	VT_CHECK_EQ(Visited, 3u); // a bad count is reported, then treated as 8
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 3);
#endif
}
