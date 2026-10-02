// VAELEN - VaelenScene
// Phase 23 task 23.02: forests - where every tree stands, in integers.
//
// THE SIMULATION HOLDS NO TREE. A tile has a biome (Land.h) and nothing
// smaller, so every tree below is INVENTED, as the layout's houses are
// (Layout.h's rule): a pure function of the ground, the layout and the
// region, the same on every machine, hashed from the tile and the tree's
// index so a tree never moves when a rule elsewhere changes. The engine
// draws what this plants and plants nothing itself.
//
// WHERE: the tiles of the played region at the biome's density, and the
// other tiles of the chunks that hold a tile of it (the land's near chunks,
// Terrain.h ChunkHolds) at a fraction of it - so the wood thins past the
// fence the way the ground coarsens. Never on water, on a road's tile, under
// a house, on a square or in a pit. Densities are per 250 m tile and are the
// FloraRules', data a host may change; the cap is the T400's: when the
// region and its ring would hold more than Cap trees, every tile's count is
// scaled down by the same ratio and the line says how many were dropped.
//
// STATUS: VALIDATED headless (Phase 23 task 23.02)
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Scene/Layout.h"
#include "Vaelen/Scene/SceneApi.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/View/Land.h"

#include <vector>

namespace Vaelen::Scene
{
	namespace TreeKind
	{
		inline constexpr uint8 Conifer = 0;	  ///< boreal: a cone on a trunk
		inline constexpr uint8 Broadleaf = 1; ///< temperate: a sphere on a trunk
		inline constexpr uint8 Palm = 2;	  ///< tropical: a flat sphere high on a thin trunk
		inline constexpr uint8 Shrub = 3;	  ///< scrubland and savanna: a low sphere, no trunk
		inline constexpr uint32 Count = 4;
	} // namespace TreeKind

	/// One tree, 16 bytes, no padding: MeasureFlora hashes it.
	struct Tree
	{
		int32 X = 0; ///< cm, the trunk's foot
		int32 Y = 0;
		int32 Z = 0;		 ///< the ground under it, HeightAt
		uint16 HeightCm = 0; ///< the whole tree, foot to top
		uint8 Kind = 0;		 ///< TreeKind
		uint8 Ring = 0;		 ///< 1 when its tile is of the ring, not the region
	};
	static_assert(sizeof(Tree) == 16, "Tree must have no padding: MeasureFlora hashes it");

	struct FloraRules
	{
		/// Trees per tile by biome, indexed as TileView::Biome (View::BiomeKinds):
		/// Ocean, Ice, Tundra, BorealForest, ColdSteppe, TemperateForest,
		/// Grassland, Scrubland, TropicalForest, Savanna, Desert, Alpine. A
		/// forest's are its wood; the grassland's three and the steppe's one
		/// are the scattered trees a plain reads as alive by (measured
		/// 2026-10-02: region 26's near chunks are mostly plain, and held
		/// 1247 trees with the plains bare).
		uint8 PerTile[View::BiomeKinds] = {0, 0, 0, 40, 1, 32, 3, 6, 48, 4, 0, 0};
		uint32 RingDivisor = 4; ///< a ring tile holds its biome's count over this
		uint32 Cap = 24000;		///< the most trees a region and its ring may hold
		uint32 MarginCm = 200;	///< no trunk nearer than this to its tile's edge
	};

	struct Flora
	{
		uint32 Region = 0;
		uint32 Dropped = 0; ///< trees the cap scaled away
		std::vector<Tree> Trees;
	};

	/// Plants the region's wood. Region 0 plants nothing (nobody's). The
	/// layout gives the tiles a tree may not stand on; a layout of another
	/// day may move a figure and never a tree, since figures exclude nothing.
	VAELEN_SCENE_API void PlantTrees(const Ground& G, const SceneLayout& L, uint32 Region, const FloraRules& Rules,
									 Flora& Out);

	struct FloraStats
	{
		uint32 Trees = 0;
		uint32 Kinds[TreeKind::Count] = {};
		uint32 Ring = 0; ///< trees on ring tiles
		uint32 Dropped = 0;
		uint32 Reserved = 0;
		Hash64 Digest = 0;
	};
	VAELEN_SCENE_API FloraStats MeasureFlora(const Flora& F);

	/// `LogVaelenScene: AELVOR <size> seed <12 hex> flora region R: trees N
	/// (conifer C, broadleaf B, palm P, shrub S), ring G, dropped D; flora <16 hex>`
	/// - composed once for the Atlas and the engine, all or nothing.
	inline constexpr uint32 FloraLineBytes = 256;
	VAELEN_SCENE_API uint32 FloraLine(uint32 Size, uint64 Seed, uint32 Region, const FloraStats& S, char* Out,
									  uint32 Bytes);
} // namespace Vaelen::Scene
