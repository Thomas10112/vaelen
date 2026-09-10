// VAELEN - Tests/View
// Phase 13.07a: the ground itself, as a renderer needs it.
//
// 13.01 proved a frame carries no way back into the world. This file makes the
// same claim about the ground, and adds the one that matters for what comes
// next: the ground the view reports is the ground the world actually has, tile
// for tile. Until that is true, nothing can be asked to stop reading the world
// directly - the atlas actor reads `Map.GetLayer(Layers.Biome)` today for the
// simple reason that it is the only place the coastline was.
//
// STATUS: PROTOTYPE (Phase 13)

#include "Vaelen/View/Land.h"

#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/TileGrid.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Sim/WorldMap.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::View;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLand);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// The smallest world that has ground: a World and its pre-history, and
	/// nothing else. TakeMapView reads layers, so nothing in Phase 04 and after
	/// needs to exist for it to work - and a fixture that proved otherwise
	/// would be hiding a dependency this module must not have.
	struct Ground
	{
		explicit Ground(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{}) { Instance.Build(); }
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		static WorldGenConfig Square(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			return Gen;
		}
		ViewSources Sources() const
		{
			ViewSources S;
			S.Types = Ages.Types();
			return S;
		}
		World Instance;
		PreHistory Ages;
	};
} // namespace

VAELEN_TEST(Land, TheGroundCarriesNoWayBackIntoTheWorld)
{
	// The claim the file exists for, put where the compiler checks it rather
	// than where a comment would.
	static_assert(std::is_trivially_copyable<TileView>::value,
				  "a TileView must be copyable by memcpy: no pointer, no handle, no vtable");
	static_assert(std::is_standard_layout<TileView>::value, "and laid out plainly enough to hand to a GPU");
	VT_CHECK_MSG(sizeof(TileView) == sizeof(int32) + sizeof(uint16) + 2 * sizeof(uint8),
				 "and it has no padding, because MeasureMapView hashes it");

	Ground W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Ground::Square(128), 0, false));
	MapView V;
	TakeMapView(W.Instance, W.Sources(), V);
	const MapStats S = MeasureMapView(V);
	VAELEN_LOG_INFO(LogLand, "%u x %u: %u tiles, %u land, %u coast, %u water, %u regions, %u bytes", V.Width, V.Height,
					S.Tiles, S.Land, S.Coast, S.Water, S.Regions, S.Bytes);
	VT_CHECK_EQ(V.Width, 128u);
	VT_CHECK_EQ(V.Height, 128u);
	VT_CHECK_EQ(S.Tiles, 128u * 128u);
	VT_CHECK_MSG(S.Land > 0, "AELVOR has land");
	VT_CHECK_MSG(S.Land < S.Tiles, "and sea around it");
	VT_CHECK_MSG(S.Coast > 0, "and a coastline between the two, which is the thing worth drawing");
	VT_CHECK_MSG(S.Regions > 0, "and its land is divided into regions");
}

VAELEN_TEST(Land, TheGroundMatchesTheWorldItWasReadFrom)
{
	// The integration test, and the one that licenses everything after it: a
	// renderer given this view draws exactly what a renderer reading the world
	// would have drawn. Every tile, not a sample - a view that is right about
	// 65535 tiles and wrong about one is a view with a hole in the coast.
	Ground W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Ground::Square(96), 0, false));
	MapView V;
	TakeMapView(W.Instance, W.Sources(), V);

	const WorldMap& Map = W.Instance.Map();
	const WorldGrid Grid = Map.Grid();
	const PreHistoryTypes& T = W.Ages.Types();
	const auto& Height = Map.GetLayer(T.World.Layers.Elevation);
	const auto& Terrain = Map.GetLayer(T.World.Layers.Terrain);
	const auto& Biome = Map.GetLayer(T.World.Layers.Biome);
	const auto& Rivers = Map.GetLayer(T.World.Hydro.RiverIndex);
	const auto& Lakes = Map.GetLayer(T.World.Hydro.LakeIndex);
	const auto& Regions = Map.GetLayer(T.World.Regions.RegionIndex);

	VT_REQUIRE_EQ(V.Tiles.size(), static_cast<usize>(Grid.TileCount()));
	uint32 Wrong = 0;
	for (uint32 Y = 0; Y < Grid.Height; ++Y)
	{
		for (uint32 X = 0; X < Grid.Width; ++X)
		{
			const uint32 I = Grid.IndexOf(TileCoord{static_cast<int32>(X), static_cast<int32>(Y)});
			const TileView* Seen = TileIn(V, X, Y);
			if (Seen == nullptr)
			{
				++Wrong;
				continue;
			}
			const bool SameGround =
				((Seen->Ground & GroundFlag::Land) != 0) == ((Terrain[I] & TerrainFlag::Land) != 0) &&
				((Seen->Ground & GroundFlag::Coast) != 0) == ((Terrain[I] & TerrainFlag::Coast) != 0) &&
				((Seen->Ground & GroundFlag::Shore) != 0) == ((Terrain[I] & TerrainFlag::Shore) != 0) &&
				((Seen->Ground & GroundFlag::River) != 0) == (Rivers[I] != 0) &&
				((Seen->Ground & GroundFlag::Lake) != 0) == (Lakes[I] != 0);
			// Elevation is kept in Q16.16, so what the view says is what the
			// world says down to 1/65536 of a unit and no further.
			const bool SameHeight = static_cast<int64>(Seen->Elevation) == (Height[I] >> 16);
			if (!SameGround || !SameHeight || Seen->Biome != Biome[I] || Seen->Region != Regions[I])
			{
				++Wrong;
			}
		}
	}
	VT_CHECK_MSG(Wrong == 0, "every tile of the view is the tile the world has");

	// And the regions the ground mentions are the regions the world made: not
	// a subset that lost one, not a superset that invented one.
	uint32 Made = 0;
	W.Instance.Components()
		.GetPool(T.World.RegionTypes_.Region)
		.ForEach([&](EntityHandle, const RegionInfo& R) { Made += R.Index != 0 ? 1u : 0u; });
	VT_CHECK_EQ(MeasureMapView(V).Regions, Made);
}

VAELEN_TEST(Land, TheGroundOutlivesTheWorldItCameFrom)
{
	// 13.01's design property, used for the first time. A MapView holds no
	// pointer into the world, so the world can be destroyed and the view still
	// read - which is what lets a tool generate a world, take the ground, drop
	// the world and write the ground out.
	MapView V;
	Hash64 Before = 0;
	{
		std::unique_ptr<Ground> W = std::make_unique<Ground>(AelvorSeed);
		VT_REQUIRE(W->Ages.Generate(Ground::Square(64), 0, false));
		TakeMapView(W->Instance, W->Sources(), V);
		Before = MeasureMapView(V).Digest;
		VT_REQUIRE(!V.Tiles.empty());
	} // the world is gone here

	VT_CHECK_EQ(MeasureMapView(V).Digest, Before);
	const TileView* Any = TileIn(V, 32, 32);
	VT_CHECK_MSG(Any != nullptr, "and the ground is still readable with nothing left to read it from");
}

VAELEN_TEST(Land, TheSameSeedGivesTheSameGround)
{
	// Determinism, at the only place it can be checked cheaply: two worlds from
	// one seed have the same ground down to the byte, and two seeds do not.
	Ground A(AelvorSeed);
	Ground B(AelvorSeed);
	Ground C(AelvorSeed + 1u);
	VT_REQUIRE(A.Ages.Generate(Ground::Square(64), 0, false));
	VT_REQUIRE(B.Ages.Generate(Ground::Square(64), 0, false));
	VT_REQUIRE(C.Ages.Generate(Ground::Square(64), 0, false));
	MapView Va;
	MapView Vb;
	MapView Vc;
	TakeMapView(A.Instance, A.Sources(), Va);
	TakeMapView(B.Instance, B.Sources(), Vb);
	TakeMapView(C.Instance, C.Sources(), Vc);
	VT_CHECK_EQ(MeasureMapView(Va).Digest, MeasureMapView(Vb).Digest);
	VT_CHECK_NE(MeasureMapView(Va).Digest, MeasureMapView(Vc).Digest);

	// And taking it twice from one world changes nothing, the way a frame taken
	// twice from a world that has not ticked must not flicker.
	MapView Again;
	TakeMapView(A.Instance, A.Sources(), Again);
	VT_CHECK_EQ(MeasureMapView(Again).Digest, MeasureMapView(Va).Digest);
}

VAELEN_TEST(Land, AWorldWithNoGroundSaysSoInsteadOfGuessing)
{
	// The edge that the layer lengths are checked for. A world whose layers are
	// declared and never generated has a grid of zero and empty layers; reading
	// one of those by tile index is precisely the bug a view exists to prevent.
	Ground W(AelvorSeed);
	MapView V;
	TakeMapView(W.Instance, W.Sources(), V);
	VT_CHECK_MSG(V.Tiles.empty(), "a world with no map has no ground to show");
	const MapStats S = MeasureMapView(V);
	VT_CHECK_EQ(S.Tiles, 0u);
	VT_CHECK_EQ(S.Land, 0u);
	VT_CHECK_EQ(S.Regions, 0u);
	VT_CHECK_MSG(TileIn(V, 0, 0) == nullptr, "and no tile at the origin either");

	// And a view that HAS ground still refuses a tile off the edge of it,
	// rather than folding the coordinate round to the next row.
	VT_REQUIRE(W.Ages.Generate(Ground::Square(32), 0, false));
	TakeMapView(W.Instance, W.Sources(), V);
	VT_REQUIRE(!V.Tiles.empty());
	VT_CHECK_MSG(TileIn(V, 32, 0) == nullptr, "x past the last column is off the map, not the next row's first tile");
	VT_CHECK_MSG(TileIn(V, 0, 32) == nullptr, "and y past the last row is off it too");
	VT_CHECK_MSG(TileIn(V, 31, 31) != nullptr, "while the far corner is on it");
}
