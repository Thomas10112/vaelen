// VAELEN - VaelenScene
// Phase 19 task 19.05: the ground one walks on, built in integers from the ground leaf.
//
// The simulation's map is tiles: an elevation in metres, a biome and a few
// water flags each (View/Land.h). A world one walks needs a surface. This
// builds it - heights at a lattice of points, triangles between them, normals
// and colours - from the MapView ALONE, in integer centimetres, so the same
// ground comes out of every compiler to the bit and is digested and pinned
// like every other figure of this project. The engine only converts the
// integers to floats and uploads them (ADR-0156).
//
// WHAT IS INVENTED HERE, SAID OUT LOUD because an invented thing quietly
// becomes a fact otherwise (VaelenViewDrawer.h's rule for where a person
// stands). The kernel has one height per TILE; everything between tile
// centres is this file's, and none of it is a fact of the world:
//   - the relief between centres (bilinear, one fixed diagonal per quad);
//   - DETAIL: a hashed wrinkle of at most DetailCm on land lattice points,
//     zero on every tile centre so that a centre is exactly the kernel's;
//   - the SEA FLOOR (a constant under a sea plane at 0 - AELVOR's sea level is
//     0 by measurement, and MapView carries none);
//   - a LAKE'S SURFACE: the lowest shore of its lake, the lake's floor clamped
//     LakeDepthCm under it;
//   - a RIVER'S BED: its tile centre lowered by RiverCm.
//
// THE SCALE IS A RULE, NOT A KERNEL FACT. Nothing in the simulation says how
// large a tile is. Slope depends only on the height-to-width ratio: at 1/1000
// (250 m tiles at a quarter of the relief) no pair of neighbouring land tiles
// at 128 or 256 is steeper than CharacterMovement's 44.76 deg (ROADMAP section
// 26). The owner decides the scale (question 1); the default is that.
//
// THE FRAME. Map-local centimetres: tile (x, y)'s CENTRE is at (x * CmPerTile,
// y * CmPerTile) and the tile covers half a tile either side - the convention
// VaelenViewDrawer::PlaceOfTile and RegionUnderGround hold, before the engine
// shifts the map by half its size. Z is up.
//
// STATUS: VALIDATED headless (Phase 19 task 19.05); the winding of the
// triangles is a BELIEF about Unreal until sitting S2 looks at it from above.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Scene/SceneApi.h"
#include "Vaelen/View/Land.h"

#include <vector>

namespace Vaelen::Scene
{
	/// How large the world is, and how finely its ground is cut. A rule of the
	/// scene, chosen by the host; no digest of the simulation depends on it.
	struct SceneScale
	{
		int32 CmPerTile = 25000;	///< one tile's side: 250 m
		int32 ReliefPerMille = 250; ///< the kernel's metres times this / 1000: a quarter
		int32 Steps = 8;			///< lattice points per tile side; even, and divides CmPerTile
		int32 SeaFloorCm = -5000;	///< every sea tile's centre, under a sea plane at 0
		int32 LakeDepthCm = 200;	///< a lake's floor under its (invented) surface
		int32 RiverCm = 300;		///< a river tile's centre, lowered
		int32 DetailCm = 200;		///< the largest hashed wrinkle on a land lattice point
		int32 Reserved = 0;
	};

	/// Steps even and at least 2, CmPerTile a positive multiple of Steps, the
	/// relief and the carvings within bounds that keep every height in int32.
	VAELEN_SCENE_API bool IsUsableScale(const SceneScale& Scale);

	namespace GroundKind
	{
		inline constexpr uint8 Land = 0;
		inline constexpr uint8 Sea = 1;
		inline constexpr uint8 Lake = 2;
		inline constexpr uint8 River = 3;
	} // namespace GroundKind

	/// One height per tile, and what the lattice needs to know about it. Holds
	/// no pointer: built from a MapView, it outlives the world it came from.
	struct Ground
	{
		uint32 Width = 0;
		uint32 Height = 0;
		SceneScale Scale;
		std::vector<int32> CentreCm;	  ///< per tile: the height of its centre
		std::vector<int32> LakeSurfaceCm; ///< per tile: the invented surface of its lake, 0 elsewhere
		std::vector<uint16> Region;		  ///< per tile, as TileView::Region
		std::vector<uint8> Kind;		  ///< per tile, GroundKind
		std::vector<uint8> Biome;		  ///< per tile, as TileView::Biome
	};

	/// False, with Out emptied, when the map is empty or the scale unusable.
	VAELEN_SCENE_API bool BuildGround(const View::MapView& Map, const SceneScale& Scale, Ground& Out);

	/// The kernel's elevation of a land tile, in scene centimetres: the one
	/// formula every land centre is (Scene.Terrain holds centres to it).
	VAELEN_SCENE_API int32 LandCentreCm(int32 Elevation, const SceneScale& Scale);

	/// The lattice: (Width * Steps + 1) by (Height * Steps + 1) points; point
	/// (U, V) is at ((U - Steps/2) * L, (V - Steps/2) * L) with L = CmPerTile /
	/// Steps, so tile (x, y)'s centre is point (x * Steps + Steps/2, ...).
	VAELEN_SCENE_API uint32 LatticeAcross(const Ground& G);
	VAELEN_SCENE_API uint32 LatticeDown(const Ground& G);
	/// The height of a lattice point, clamped to the lattice. A pure function
	/// of the ground and the point: two chunks that share an edge share it.
	VAELEN_SCENE_API int32 LatticeZ(const Ground& G, int32 U, int32 V);

	/// The ground's height under a point, on the SAME triangles the mesh is cut
	/// into (at the full lattice), floor-rounded to a centimetre. Off the
	/// lattice, the nearest edge's.
	VAELEN_SCENE_API int32 HeightAt(const Ground& G, int64 Xcm, int64 Ycm);

	/// A tile's centre, and the tile under a point: half-open, each tile the
	/// half tile either side of its centre. False when the point is off the map.
	VAELEN_SCENE_API void PointOfTile(const Ground& G, uint32 Tile, int64& Xcm, int64& Ycm);
	VAELEN_SCENE_API bool TileOfPoint(const Ground& G, int64 Xcm, int64 Ycm, uint32& Tile);

	/// One vertex, 24 bytes, no padding: MeasureTerrain hashes it.
	struct TerrainVertex
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Z = 0;
		int16 NX = 0; ///< the unit normal times 32767
		int16 NY = 0;
		int16 NZ = 0;
		uint8 R = 0; ///< the tile's colour: its biome, or its water
		uint8 G = 0;
		uint8 B = 0;
		uint8 A = 255;
		int16 Reserved = 0;
	};
	static_assert(sizeof(TerrainVertex) == 24, "TerrainVertex must have no padding: MeasureTerrain hashes it");

	/// The tiles a chunk covers along each side.
	inline constexpr uint32 ChunkTiles = 16;
	VAELEN_SCENE_API uint32 ChunksAcross(const Ground& G);
	VAELEN_SCENE_API uint32 ChunksDown(const Ground& G);

	/// A patch of the lattice, every Stride-th point, as a triangle list: the
	/// quad (u, v)-(u+s, v+s) is cut along its (u, v)-(u+s, v+s) diagonal, the
	/// triangles wound (u, v), (u+s, v), (u+s, v+s) and (u, v), (u+s, v+s),
	/// (u, v+s) - counter-clockwise seen from above in this right-handed
	/// frame, which is the BELIEF S2 settles.
	struct TerrainMesh
	{
		uint32 TileX0 = 0;
		uint32 TileY0 = 0;
		uint32 TileX1 = 0; ///< one past the last tile
		uint32 TileY1 = 0;
		uint32 Stride = 0;
		uint32 Across = 0; ///< vertices per row
		std::vector<TerrainVertex> Vertices;
		std::vector<uint32> Triangles;
	};

	/// The tiles [TileX0, TileX1) x [TileY0, TileY1), every Stride-th lattice
	/// point (Stride divides Steps: Steps for the far land, 1 for the near).
	/// False when the patch or the stride is not one.
	VAELEN_SCENE_API bool BuildPatch(const Ground& G, uint32 TileX0, uint32 TileY0, uint32 TileX1, uint32 TileY1,
									 uint32 Stride, TerrainMesh& Out);
	/// Chunk (CX, CY): tiles [CX * ChunkTiles, ...), clipped to the map.
	VAELEN_SCENE_API bool BuildChunk(const Ground& G, uint32 CX, uint32 CY, uint32 Stride, TerrainMesh& Out);

	/// 44.76 deg, CharacterMovement's default walkable floor, as the rational
	/// test 119 K^2 < 121 (dx^2 + dy^2) on a triangle's reduced normal: cos^2 of
	/// the angle is 121/240, which is 44.77 deg. Integer, so every compiler
	/// counts the same triangles.
	VAELEN_SCENE_API bool IsSteep(int64 Dx, int64 Dy, int64 Run);

	struct TerrainStats
	{
		uint32 Chunks = 0;
		uint32 Vertices = 0;
		uint32 Triangles = 0;
		uint32 Steep = 0; ///< triangles steeper than IsSteep's 44.76 deg
		int32 MinZ = 0;
		int32 MaxZ = 0;
		Hash64 Digest = 0; ///< every mesh measured, in the order measured
	};
	/// Adds one mesh to the tally: its vertices and indices into the digest,
	/// its triangles to the counts.
	VAELEN_SCENE_API void MeasureTerrain(const TerrainMesh& Mesh, TerrainStats& Into);

	/// `LogVaelenScene: AELVOR <size> seed <12 hex> region <R|all>: chunks C,
	/// vertices V, triangles T, z [a, b] cm, steep S of T; terrain <16 hex>` -
	/// composed once for the Atlas and the engine (19.04's rule), all or nothing.
	inline constexpr uint32 TerrainLineBytes = 256;
	VAELEN_SCENE_API uint32 TerrainLine(uint32 Size, uint64 Seed, uint32 Region, const TerrainStats& Stats, char* Out,
										uint32 Bytes);
} // namespace Vaelen::Scene
