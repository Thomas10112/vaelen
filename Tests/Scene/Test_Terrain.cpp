// VAELEN - Phase 19 task 19.05: the ground one walks on, built in integers.
//
// The scene reads a MapView and nothing else, so every clause here takes the
// real AELVOR map at 128 (and at 256 where the row asks), destroys the world,
// and holds the ground built from what is left to what the kernel said:
// centres exactly the kernel's heights, chunks that agree on every shared
// point, a closed surface, one point-to-tile arithmetic with the engine's,
// HeightAt on the mesh's own triangles, and the steep triangles counted
// against a prediction written before the first run (ROADMAP 19.05).
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Take.h"
#include "Vaelen/Core/Log.h"
#include "Vaelen/Core/Random.h"
#include "VaelenTest.h"

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Scene;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogSceneTerrain);

	/// The AELVOR map at Size, taken and then outliving its world: the scene
	/// is built from the leaf alone.
	View::MapView MapOf(uint32 Size)
	{
		View::MapView Map;
		{
			Run::Options O;
			O.Size = Size;
			O.PreHistory = 1;
			O.Years = 0;
			O.Climate = false;
			Run::Aelvor A(O);
			if (!A.Begin())
			{
				return Map;
			}
			View::TakeMapView(A.Instance(), A.Sources(), Map);
		}
		return Map;
	}

	const View::MapView& Map128()
	{
		static const View::MapView Map = MapOf(128);
		return Map;
	}

	const View::MapView& Map256()
	{
		static const View::MapView Map = MapOf(256);
		return Map;
	}

	/// The independent reading of a land centre: metres in Q16.16, times 100,
	/// times the relief per mille, floored - written out here rather than
	/// asked of LandCentreCm, so the two are two instruments.
	int32 Expected(int32 Elevation, int32 ReliefPerMille)
	{
		const int64 Num = int64{Elevation} * 100 * ReliefPerMille;
		const int64 Den = int64{1000} * 65536;
		int64 Q = Num / Den;
		if (Num % Den != 0 && Num < 0)
		{
			--Q;
		}
		return static_cast<int32>(Q);
	}

	int32 CentreU(const Ground& G, uint32 X)
	{
		return static_cast<int32>(X) * G.Scale.Steps + G.Scale.Steps / 2;
	}

	TerrainStats Everything(const Ground& G, uint32 Stride)
	{
		TerrainStats S;
		TerrainMesh M;
		for (uint32 CY = 0; CY < ChunksDown(G); ++CY)
		{
			for (uint32 CX = 0; CX < ChunksAcross(G); ++CX)
			{
				if (BuildChunk(G, CX, CY, Stride, M))
				{
					MeasureTerrain(M, S);
				}
			}
		}
		return S;
	}
} // namespace

VAELEN_TEST(Terrain, EveryCentreIsTheKernelsHeight)
{
	for (const View::MapView* Map : {&Map128(), &Map256()})
	{
		VT_CHECK(!Map->Tiles.empty());
		Ground G;
		VT_CHECK(BuildGround(*Map, SceneScale{}, G));
		uint32 Land = 0, Sea = 0, Lake = 0, River = 0;
		for (uint32 Y = 0; Y < G.Height; ++Y)
		{
			for (uint32 X = 0; X < G.Width; ++X)
			{
				const uint32 T = Y * G.Width + X;
				const View::TileView& Tile = Map->Tiles[T];
				const int32 Z = LatticeZ(G, CentreU(G, X), CentreU(G, Y));
				const int32 Raw = Expected(Tile.Elevation, G.Scale.ReliefPerMille);
				switch (G.Kind[T])
				{
				case GroundKind::Land:
					VT_CHECK_EQ(Z, Raw);
					++Land;
					break;
				case GroundKind::Sea:
					VT_CHECK_EQ(Z, G.Scale.SeaFloorCm);
					++Sea;
					break;
				case GroundKind::River:
					VT_CHECK_EQ(Z, Raw - G.Scale.RiverCm);
					++River;
					break;
				default:
					VT_CHECK(Z <= G.LakeSurfaceCm[T] - G.Scale.LakeDepthCm);
					++Lake;
					break;
				}
			}
		}
		// Every kind of ground is exercised, or the clause above says nothing about it.
		VT_CHECK(Land > 0u && Sea > 0u && Lake > 0u && River > 0u);
		VAELEN_LOG_INFO(LogSceneTerrain, "%u: %u land, %u sea, %u lake, %u river tiles, every centre the kernel's",
						G.Width, Land, Sea, Lake, River);
	}
	// CONTROL: the map the scene read is the pinned ground (Atlas.Frozen128's).
	VT_CHECK_DIGEST_EQ(View::MeasureMapView(Map128()).Digest, 0x8f7f4948f49b6e86ull);
}

VAELEN_TEST(Terrain, EveryLakeHasOneInventedSurfaceItsLowestShore)
{
	// What 19.05 says it INVENTS - one surface per four-connected lake, the
	// lowest raw height of the land or river around it, its own lowest
	// height when it has no shore, and a floor LakeDepthCm under it -
	// asserted from a flood fill of the map's own lake flags (the review
	// of 2026-09-27: the lake clause of the case below compared the centre
	// to a bound the centre is DEFINED as, and could not fail).
	for (const View::MapView* Map : {&Map128(), &Map256()})
	{
		Ground G;
		VT_REQUIRE(BuildGround(*Map, SceneScale{}, G));
		std::vector<uint8> Seen(G.Width * G.Height, 0);
		uint32 Lakes = 0, Wide = 0, Shored = 0, Members = 0;
		for (uint32 Start = 0; Start < G.Width * G.Height; ++Start)
		{
			if ((Map->Tiles[Start].Ground & View::GroundFlag::Lake) == 0u || Seen[Start] != 0u)
			{
				continue;
			}
			std::vector<uint32> Lake;
			std::vector<uint32> Stack(1, Start);
			Seen[Start] = 1;
			bool HasShore = false;
			int32 Shore = 0, Lowest = 0;
			while (!Stack.empty())
			{
				const uint32 T = Stack.back();
				Stack.pop_back();
				Lake.push_back(T);
				const int32 Raw = Expected(Map->Tiles[T].Elevation, G.Scale.ReliefPerMille);
				Lowest = Lake.size() == 1u || Raw < Lowest ? Raw : Lowest;
				const uint32 X = T % G.Width;
				const uint32 Y = T / G.Width;
				const uint32 Around[4] = {X + 1u < G.Width ? T + 1u : T, X > 0u ? T - 1u : T,
										  Y + 1u < G.Height ? T + G.Width : T, Y > 0u ? T - G.Width : T};
				for (const uint32 A : Around)
				{
					if (A == T)
					{
						continue;
					}
					const uint8 Flags = Map->Tiles[A].Ground;
					if ((Flags & View::GroundFlag::Lake) != 0u)
					{
						if (Seen[A] == 0u)
						{
							Seen[A] = 1;
							Stack.push_back(A);
						}
					}
					else if ((Flags & View::GroundFlag::Land) != 0u)
					{
						// Land or river: a shore. Sea (no Land flag) is not one.
						const int32 RawA = Expected(Map->Tiles[A].Elevation, G.Scale.ReliefPerMille);
						Shore = !HasShore || RawA < Shore ? RawA : Shore;
						HasShore = true;
					}
				}
			}
			const int32 Surface = HasShore ? Shore : Lowest;
			for (const uint32 T : Lake)
			{
				VT_CHECK_EQ(G.Kind[T], GroundKind::Lake);
				VT_CHECK_EQ(G.LakeSurfaceCm[T], Surface);
				const int32 Raw = Expected(Map->Tiles[T].Elevation, G.Scale.ReliefPerMille);
				const int32 Floor = Surface - G.Scale.LakeDepthCm;
				VT_CHECK_EQ(G.CentreCm[T], Raw < Floor ? Raw : Floor);
				VT_CHECK_EQ(LatticeZ(G, CentreU(G, T % G.Width), CentreU(G, T / G.Width)), G.CentreCm[T]);
			}
			++Lakes;
			Wide += Lake.size() > 1u ? 1u : 0u;
			Shored += HasShore ? 1u : 0u;
			Members += static_cast<uint32>(Lake.size());
		}
		// The clause was exercised: lakes, lakes of more than one tile (one
		// surface over several tiles), and lakes with a shore to take it from.
		VT_CHECK(Lakes > 0u && Wide > 0u && Shored > 0u);
		VAELEN_LOG_INFO(LogSceneTerrain, "%u: %u lakes over %u tiles, %u wider than a tile, %u with a shore", G.Width,
						Lakes, Members, Wide, Shored);
	}
	// CONTROL: a map drawn by hand, its answers worked out by hand. Five by
	// five; at the default relief a metre of elevation is 25 cm. Lake A is
	// (1,1) at 10 m and (2,1) at 25 m, with land at 30, 20, 35 and 40 m
	// around it and a sea tile at (2,0): its surface is the 20 m shore, 500,
	// so (1,1) keeps its own 250 and (2,1) is held to the floor, 300. Lake
	// B is (3,3) at 12 m, ringed by sea: no shore, its own 300, floor 100.
	View::MapView Hand;
	Hand.Width = 5;
	Hand.Height = 5;
	Hand.Tiles.resize(25);
	const auto Tile = [&](uint32 X, uint32 Y, uint8 Flags, int32 Metres)
	{
		Hand.Tiles[Y * 5u + X].Ground = Flags;
		Hand.Tiles[Y * 5u + X].Elevation = Metres * 65536;
	};
	Tile(0, 1, View::GroundFlag::Land, 30);
	Tile(1, 0, View::GroundFlag::Land, 20);
	Tile(1, 1, View::GroundFlag::Lake, 10);
	Tile(2, 1, View::GroundFlag::Lake, 25);
	Tile(1, 2, View::GroundFlag::Land, 35);
	Tile(2, 2, View::GroundFlag::Land, 40);
	Tile(3, 1, View::GroundFlag::Land, 40);
	Tile(3, 3, View::GroundFlag::Lake, 12);
	Ground G;
	VT_REQUIRE(BuildGround(Hand, SceneScale{}, G));
	VT_CHECK_EQ(G.LakeSurfaceCm[1u * 5u + 1u], 500);
	VT_CHECK_EQ(G.LakeSurfaceCm[1u * 5u + 2u], 500);
	VT_CHECK_EQ(G.CentreCm[1u * 5u + 1u], 250);
	VT_CHECK_EQ(G.CentreCm[1u * 5u + 2u], 300);
	VT_CHECK_EQ(G.LakeSurfaceCm[3u * 5u + 3u], 300);
	VT_CHECK_EQ(G.CentreCm[3u * 5u + 3u], 100);
	VT_CHECK_EQ(G.Kind[3u * 5u + 3u], GroundKind::Lake);
	VT_CHECK_EQ(G.CentreCm[0u * 5u + 0u], G.Scale.SeaFloorCm);
	// A chunk index past the map is refused, not wrapped onto chunk 0 (the
	// review of 2026-09-27: CX * 16 at CX = 2^28 is 0 in uint32).
	TerrainMesh M;
	VT_CHECK(!BuildChunk(G, 1u << 28, 0, 1, M));
	VT_CHECK(M.Vertices.empty());
	VT_CHECK(!BuildChunk(G, 0, 1u << 28, 1, M));
	VT_CHECK(!ChunkHolds(G, 1u << 28, 0, 0u));
	VT_CHECK(ChunkHolds(G, 0, 0, 0u) && BuildChunk(G, 0, 0, 1, M) && M.Vertices.size() == 41u * 41u);
}

VAELEN_TEST(Terrain, NoPairOfNeighbouringLandTilesIsTooSteepToWalk)
{
	// The measurement the scale was chosen by (ROADMAP section 26), re-made
	// here on the centres the scene actually uses: 0 at 128 and at 256.
	for (const View::MapView* Map : {&Map128(), &Map256()})
	{
		Ground G;
		VT_CHECK(BuildGround(*Map, SceneScale{}, G));
		uint32 Pairs = 0, Steep = 0;
		for (uint32 Y = 0; Y < G.Height; ++Y)
		{
			for (uint32 X = 0; X < G.Width; ++X)
			{
				const uint32 T = Y * G.Width + X;
				const uint32 Right = X + 1u < G.Width ? T + 1u : T;
				const uint32 Down = Y + 1u < G.Height ? T + G.Width : T;
				for (const uint32 N : {Right, Down})
				{
					if (N == T || G.Kind[T] == GroundKind::Sea || G.Kind[N] == GroundKind::Sea)
					{
						continue;
					}
					++Pairs;
					Steep += IsSteep(int64{G.CentreCm[N]} - G.CentreCm[T], 0, G.Scale.CmPerTile) ? 1u : 0u;
				}
			}
		}
		VT_CHECK(Pairs > 10000u);
		VT_CHECK_EQ(Steep, 0u);
	}
	// IsSteep's own threshold, by known answers (the review of 2026-09-27: the
	// clause above and its control both count WITH IsSteep, so a threshold
	// moved to 50 degrees would still give 0 here and many below). 44.76 deg
	// is 119 K^2 < 121 (dx^2 + dy^2): a gradient of 0.9917 is not steep,
	// 0.9918 is, and the diagonal 0.7020 * sqrt 2 is, 0.7000 * sqrt 2 not.
	VT_CHECK(!IsSteep(9917, 0, 10000));
	VT_CHECK(IsSteep(9918, 0, 10000));
	VT_CHECK(IsSteep(0, 9918, 10000));
	VT_CHECK(IsSteep(7020, 7020, 10000));
	VT_CHECK(!IsSteep(7000, 7000, 10000));
	VT_CHECK(!IsSteep(0, 0, 1));
	// CONTROL: the instrument can see a slope - at a quarter the tile size, the same map is not walkable.
	SceneScale Small;
	Small.CmPerTile = 6250; // 62.5 m: ratio 1/250
	Small.Steps = 2;
	Ground G;
	VT_CHECK(BuildGround(Map128(), Small, G));
	uint32 Steep = 0;
	for (uint32 T = 0; T + 1u < G.CentreCm.size(); ++T)
	{
		if (G.Kind[T] != GroundKind::Sea && G.Kind[T + 1u] != GroundKind::Sea && (T + 1u) % G.Width != 0u)
		{
			Steep += IsSteep(int64{G.CentreCm[T + 1u]} - G.CentreCm[T], 0, Small.CmPerTile) ? 1u : 0u;
		}
	}
	VT_CHECK(Steep > 0u);
}

VAELEN_TEST(Terrain, ChunksAgreeOnEverySharedPoint)
{
	Ground G;
	VT_CHECK(BuildGround(Map128(), SceneScale{}, G));
	// A 32 x 32-tile patch built at once, and the four chunks that partition it.
	TerrainMesh Whole;
	VT_CHECK(BuildPatch(G, 16, 16, 48, 48, 1, Whole));
	std::map<std::pair<int32, int32>, TerrainVertex> ByPoint;
	for (const TerrainVertex& V : Whole.Vertices)
	{
		ByPoint[{V.X, V.Y}] = V;
	}
	uint32 Compared = 0;
	for (uint32 CY = 1; CY <= 2; ++CY)
	{
		for (uint32 CX = 1; CX <= 2; ++CX)
		{
			TerrainMesh Part;
			VT_CHECK(BuildChunk(G, CX, CY, 1, Part));
			for (const TerrainVertex& V : Part.Vertices)
			{
				const auto Found = ByPoint.find({V.X, V.Y});
				VT_CHECK(Found != ByPoint.end());
				if (Found != ByPoint.end())
				{
					VT_CHECK(std::memcmp(&Found->second, &V, sizeof(V)) == 0);
					++Compared;
				}
			}
		}
	}
	// Four chunks of 129 x 129 points; the shared edges are compared twice.
	VT_CHECK_EQ(Compared, 4u * 129u * 129u);

	// The far land (a stride of Steps) stands on the same points as the near.
	TerrainMesh Far;
	TerrainMesh Near;
	VT_CHECK(BuildChunk(G, 3, 2, static_cast<uint32>(G.Scale.Steps), Far));
	VT_CHECK(BuildChunk(G, 3, 2, 1, Near));
	uint32 Same = 0;
	for (const TerrainVertex& F : Far.Vertices)
	{
		for (const TerrainVertex& N : Near.Vertices)
		{
			if (N.X == F.X && N.Y == F.Y)
			{
				VT_CHECK_EQ(N.Z, F.Z);
				Same += N.Z == F.Z ? 1u : 0u;
				break;
			}
		}
	}
	VT_CHECK_EQ(Same, 17u * 17u);
}

VAELEN_TEST(Terrain, EveryChunkIsAClosedSheet)
{
	Ground G;
	VT_CHECK(BuildGround(Map128(), SceneScale{}, G));
	// The winding, too (the review of 2026-09-27): every interior edge is
	// walked once in each direction and a border edge once in one, so no
	// triangle faces the other way from its neighbours - which the unordered
	// count below cannot see, since a quad cut the wrong way round still
	// uses each edge twice. Which way is UP is a sitting's (Terrain.h).
	const auto Consistent = [](const std::vector<uint32>& Triangles)
	{
		std::map<std::pair<uint32, uint32>, uint32> Directed;
		for (usize t = 0; t + 2u < Triangles.size(); t += 3u)
		{
			for (uint32 e = 0; e < 3u; ++e)
			{
				++Directed[{Triangles[t + e], Triangles[t + (e + 1u) % 3u]}];
			}
		}
		for (const auto& [Edge, Count] : Directed)
		{
			const auto Back = Directed.find({Edge.second, Edge.first});
			if (Count != 1u || (Back != Directed.end() && Back->second != 1u))
			{
				return false;
			}
		}
		return true;
	};
	for (const uint32 Stride : {1u, 8u})
	{
		TerrainMesh M;
		VT_CHECK(BuildChunk(G, 2, 3, Stride, M));
		std::map<std::pair<uint32, uint32>, uint32> Uses;
		for (usize t = 0; t < M.Triangles.size(); t += 3u)
		{
			const uint32 C[3] = {M.Triangles[t], M.Triangles[t + 1u], M.Triangles[t + 2u]};
			VT_CHECK(C[0] != C[1] && C[1] != C[2] && C[0] != C[2]);
			for (uint32 e = 0; e < 3u; ++e)
			{
				const uint32 A = C[e];
				const uint32 B = C[(e + 1u) % 3u];
				++Uses[{A < B ? A : B, A < B ? B : A}];
			}
		}
		uint32 Border = 0;
		uint32 Inner = 0;
		for (const auto& [Edge, Count] : Uses)
		{
			VT_CHECK(Count == 1u || Count == 2u);
			Border += Count == 1u ? 1u : 0u;
			Inner += Count == 2u ? 1u : 0u;
		}
		const uint32 Side = M.Across - 1u;
		VT_CHECK_EQ(Border, 4u * Side);
		// A disc: vertices - edges + faces = 1.
		const int64 Euler = static_cast<int64>(M.Vertices.size()) - static_cast<int64>(Uses.size()) +
							static_cast<int64>(M.Triangles.size() / 3u);
		VT_CHECK_EQ(Euler, int64{1});
		VT_CHECK(Inner > 0u);
		VT_CHECK(Consistent(M.Triangles));
		// CONTROL: one triangle turned over is seen, and the unordered count
		// above would not have seen it (the same border, the same Euler number).
		std::vector<uint32> Turned = M.Triangles;
		std::swap(Turned[7], Turned[8]);
		VT_CHECK(!Consistent(Turned));
		std::map<std::pair<uint32, uint32>, uint32> Blind;
		for (usize t = 0; t < Turned.size(); t += 3u)
		{
			for (uint32 e = 0; e < 3u; ++e)
			{
				const uint32 A = Turned[t + e];
				const uint32 B = Turned[t + (e + 1u) % 3u];
				++Blind[{A < B ? A : B, A < B ? B : A}];
			}
		}
		VT_CHECK(Blind == Uses);
	}
}

VAELEN_TEST(Terrain, APointIsInTheTileTheEngineThinksItIsIn)
{
	Ground G;
	VT_CHECK(BuildGround(Map128(), SceneScale{}, G));
	const int64 C = G.Scale.CmPerTile;
	uint32 Agreed = 0;
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		int64 X = 0, Y = 0;
		PointOfTile(G, T, X, Y);
		uint32 Back = 0;
		VT_CHECK(TileOfPoint(G, X, Y, Back));
		VT_CHECK_EQ(Back, T);
		// A 5 x 5 grid over the tile, its edges included: the engine's
		// RegionUnderGround reads the tile as floor(X / size + W/2 + 0.5) in
		// its frame, which is this one shifted by W/2 tiles.
		for (int64 i = -2; i <= 2; ++i)
		{
			for (int64 j = -2; j <= 2; ++j)
			{
				const int64 PX = X + i * C / 4;
				const int64 PY = Y + j * C / 4;
				const double GroundX = static_cast<double>(PX) - 0.5 * G.Width * static_cast<double>(C);
				const double GroundY = static_cast<double>(PY) - 0.5 * G.Height * static_cast<double>(C);
				const int64 EX = static_cast<int64>(std::floor(GroundX / static_cast<double>(C) + G.Width * 0.5 + 0.5));
				const int64 EY =
					static_cast<int64>(std::floor(GroundY / static_cast<double>(C) + G.Height * 0.5 + 0.5));
				uint32 Mine = 0;
				const bool On = TileOfPoint(G, PX, PY, Mine);
				const bool EngineOn = EX >= 0 && EY >= 0 && EX < G.Width && EY < G.Height;
				VT_CHECK_EQ(On, EngineOn);
				if (On && EngineOn)
				{
					VT_CHECK_EQ(Mine, static_cast<uint32>(EY * G.Width + EX));
					++Agreed;
				}
			}
		}
	}
	VT_CHECK(Agreed > 128u * 128u * 16u);
}

VAELEN_TEST(Terrain, HeightAtIsTheMeshsOwnSurface)
{
	Ground G;
	VT_CHECK(BuildGround(Map128(), SceneScale{}, G));
	const int64 L = G.Scale.CmPerTile / G.Scale.Steps;
	const int64 C = G.Scale.CmPerTile;
	RandomStream Draw(0x19050000ull);
	TerrainMesh M;
	uint32 LastChunk = 0xFFFFFFFFu;
	uint32 Checked = 0, Rim = 0, OnDiagonal = 0;
	for (uint32 i = 0; i < 10000u; ++i)
	{
		// A point anywhere on the map, its outer half tiles included (the
		// review of 2026-09-27: they were never drawn, and they are where
		// Vaelen.Probe puts its points).
		const int64 X = static_cast<int64>(Draw.NextU32() % (G.Width * static_cast<uint32>(C))) - C / 2;
		const int64 Y = static_cast<int64>(Draw.NextU32() % (G.Height * static_cast<uint32>(C))) - C / 2;
		const int32 Got = HeightAt(G, X, Y);
		uint32 Tile = 0;
		VT_CHECK(TileOfPoint(G, X, Y, Tile));
		Rim += Tile % G.Width == 0u || Tile / G.Width == 0u || Tile % G.Width == G.Width - 1u ||
					   Tile / G.Width == G.Height - 1u
				   ? 1u
				   : 0u;
		// The second instrument: the quad's two triangles read from the BUILT
		// mesh's own index list, the one that holds the point found by the
		// integer sign of its edges, and the plane through THAT triangle's
		// three vertices (the review of 2026-09-27: the diagonal used to be
		// this test's belief, not the mesh's).
		const int64 GX = X + (G.Scale.Steps / 2) * L;
		const int64 GY = Y + (G.Scale.Steps / 2) * L;
		const uint32 U = static_cast<uint32>(GX / L);
		const uint32 V = static_cast<uint32>(GY / L);
		const uint32 CX = U / (ChunkTiles * static_cast<uint32>(G.Scale.Steps));
		const uint32 CY = V / (ChunkTiles * static_cast<uint32>(G.Scale.Steps));
		if (CX * 1000u + CY != LastChunk)
		{
			VT_CHECK(BuildChunk(G, CX, CY, 1, M));
			LastChunk = CX * 1000u + CY;
		}
		const uint32 I = U - M.TileX0 * static_cast<uint32>(G.Scale.Steps);
		const uint32 J = V - M.TileY0 * static_cast<uint32>(G.Scale.Steps);
		const usize Quad = (static_cast<usize>(J) * (M.Across - 1u) + I) * 6u;
		VT_REQUIRE(Quad + 6u <= M.Triangles.size());
		bool Found = false;
		int64 Want = 0;
		for (usize t = Quad; t < Quad + 6u && !Found; t += 3u)
		{
			const TerrainVertex& P0 = M.Vertices[M.Triangles[t]];
			const TerrainVertex& P1 = M.Vertices[M.Triangles[t + 1u]];
			const TerrainVertex& P2 = M.Vertices[M.Triangles[t + 2u]];
			// Inside or on an edge: the three cross products share a sign (or are 0).
			const int64 S0 = (int64{P1.X} - P0.X) * (Y - P0.Y) - (int64{P1.Y} - P0.Y) * (X - P0.X);
			const int64 S1 = (int64{P2.X} - P1.X) * (Y - P1.Y) - (int64{P2.Y} - P1.Y) * (X - P1.X);
			const int64 S2 = (int64{P0.X} - P2.X) * (Y - P2.Y) - (int64{P0.Y} - P2.Y) * (X - P2.X);
			const bool NonNegative = S0 >= 0 && S1 >= 0 && S2 >= 0;
			const bool NonPositive = S0 <= 0 && S1 <= 0 && S2 <= 0;
			if (!NonNegative && !NonPositive)
			{
				continue;
			}
			Found = true;
			OnDiagonal += S0 == 0 || S1 == 0 || S2 == 0 ? 1u : 0u;
			// The plane through the three: n = (P1 - P0) x (P2 - P0), and
			// Z = Z0 - (nx (X - X0) + ny (Y - Y0)) / nz, floored as the scene floors.
			const int64 AX = int64{P1.X} - P0.X, AY = int64{P1.Y} - P0.Y, AZ = int64{P1.Z} - P0.Z;
			const int64 BX = int64{P2.X} - P0.X, BY = int64{P2.Y} - P0.Y, BZ = int64{P2.Z} - P0.Z;
			const int64 NX = AY * BZ - AZ * BY;
			const int64 NY = AZ * BX - AX * BZ;
			const int64 NZ = AX * BY - AY * BX;
			VT_REQUIRE(NZ != 0);
			const int64 Num = int64{P0.Z} * NZ - NX * (X - P0.X) - NY * (Y - P0.Y);
			Want = Num / NZ;
			if (Num % NZ != 0 && ((Num < 0) != (NZ < 0)))
			{
				--Want;
			}
		}
		VT_CHECK(Found);
		VT_CHECK_EQ(int64{Got}, Want);
		Checked += Found && int64{Got} == Want ? 1u : 0u;
	}
	VT_CHECK_EQ(Checked, 10000u);
	VT_CHECK(Rim > 100u);
	VAELEN_LOG_INFO(LogSceneTerrain, "10000 points, %u on the rim tiles, %u on a triangle's edge, every one the mesh's",
					Rim, OnDiagonal);
}

VAELEN_TEST(Terrain, MeasureChunksTakesEveryChunkTouchingTheRegionAndOnlyThose)
{
	// The function behind the engine's near-terrain line and the Atlas's
	// `--scene-terrain R` (the review of 2026-09-27: no case called it, and
	// the engine's own chunk selection is a second copy of its loop): the
	// chunks it measures are exactly the chunks holding a tile of the
	// region, found here by a plain scan of the tiles, in row-major order;
	// region 0 is the whole map; and region 9 at 128 is pinned.
	Ground G;
	VT_REQUIRE(BuildGround(Map128(), SceneScale{}, G));
	// A few regions: 9 (the pinned one), the one owning the first region
	// tile in row-major order (nearest the map's top-left rim; the rim
	// itself is sea at 128), and the one with the most land.
	std::vector<uint32> Regions = {9u};
	std::map<uint32, uint32> LandOf;
	uint32 Nearest = 0;
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		const uint32 R = G.Region[T];
		if (R == 0u)
		{
			continue;
		}
		++LandOf[R];
		Nearest = Nearest == 0u ? R : Nearest;
	}
	uint32 Largest = 0;
	for (const auto& [R, Count] : LandOf)
	{
		Largest = Largest == 0u || Count > LandOf[Largest] ? R : Largest;
	}
	VT_REQUIRE(Nearest != 0u && Largest != 0u);
	Regions.push_back(Nearest);
	Regions.push_back(Largest);
	for (const uint32 R : Regions)
	{
		std::vector<uint8> Wanted(ChunksAcross(G) * ChunksDown(G), 0);
		uint32 Chunks = 0;
		for (uint32 T = 0; T < G.Width * G.Height; ++T)
		{
			if (G.Region[T] == R)
			{
				uint8& W = Wanted[(T / G.Width / ChunkTiles) * ChunksAcross(G) + (T % G.Width) / ChunkTiles];
				Chunks += W == 0u ? 1u : 0u;
				W = 1;
			}
		}
		TerrainStats Expect;
		TerrainMesh M;
		for (uint32 CY = 0; CY < ChunksDown(G); ++CY)
		{
			for (uint32 CX = 0; CX < ChunksAcross(G); ++CX)
			{
				if (Wanted[CY * ChunksAcross(G) + CX] != 0u && BuildChunk(G, CX, CY, 1, M))
				{
					MeasureTerrain(M, Expect);
				}
			}
		}
		TerrainStats Got;
		MeasureChunks(G, R, Got);
		VT_CHECK_EQ(Got.Chunks, Chunks);
		VT_CHECK_EQ(Got.Chunks, Expect.Chunks);
		VT_CHECK_DIGEST_EQ(Got.Digest, Expect.Digest);
		VT_CHECK(Got.Vertices == Expect.Vertices && Got.Triangles == Expect.Triangles && Got.Steep == Expect.Steep);
		VT_CHECK(Chunks > 0u && Chunks < ChunksAcross(G) * ChunksDown(G));
		VAELEN_LOG_INFO(LogSceneTerrain, "region %u: %u chunks, terrain %016llx", R, Got.Chunks,
						static_cast<unsigned long long>(Got.Digest));
	}
	TerrainStats All;
	MeasureChunks(G, 0u, All);
	VT_CHECK_EQ(All.Chunks, ChunksAcross(G) * ChunksDown(G));
	VT_CHECK_DIGEST_EQ(All.Digest, Everything(G, 1).Digest);
	// PINNED: region 9 at 128, two chunks (ROADMAP 19.05, and the Atlas's
	// `--scene-terrain 9` line).
	TerrainStats Nine;
	MeasureChunks(G, 9u, Nine);
	VT_CHECK_EQ(Nine.Chunks, 2u);
	VT_CHECK_DIGEST_EQ(Nine.Digest, 0x05f5e0cefbe78608ull);
	// A region the map does not have measures nothing.
	TerrainStats None;
	MeasureChunks(G, 65535u, None);
	VT_CHECK_EQ(None.Chunks, 0u);
	VT_CHECK_EQ(None.Digest, Hash64{0});
}

VAELEN_TEST(Terrain, TheSteepTrianglesAreWithinThePrediction)
{
	// Predicted in writing before VaelenScene existed: at most 0.1% of the
	// map's triangles, 2097 of 2,097,152 at 128 and 8388 of 8,388,608 at 256.
	Ground G128;
	VT_CHECK(BuildGround(Map128(), SceneScale{}, G128));
	const TerrainStats S128 = Everything(G128, 1);
	VT_CHECK_EQ(S128.Triangles, 2097152u);
	VT_CHECK(S128.Steep <= 2097u);
	Ground G256;
	VT_CHECK(BuildGround(Map256(), SceneScale{}, G256));
	const TerrainStats S256 = Everything(G256, 1);
	VT_CHECK_EQ(S256.Triangles, 8388608u);
	VT_CHECK(S256.Steep <= 8388u);
	// PINNED (the review of 2026-09-27: logged since 19.05, asserted by
	// nobody but Atlas.SceneTerrain128 - and the 256 mesh by nobody at all).
	VT_CHECK_DIGEST_EQ(S128.Digest, 0x105208c54e3c6ea9ull);
	VT_CHECK_DIGEST_EQ(S256.Digest, 0x5b29727b01436706ull);
	// CONTROL: the counter can count. At a quarter of the tile size (62.5 m,
	// ratio 1/250 - 812 unwalkable tile pairs at 128), the same map's mesh
	// must hold steep triangles, and many.
	SceneScale Tight;
	Tight.CmPerTile = 6250;
	Tight.Steps = 2;
	Ground GT;
	VT_CHECK(BuildGround(Map128(), Tight, GT));
	const TerrainStats ST = Everything(GT, 1);
	VT_CHECK(ST.Steep > 812u);
	VAELEN_LOG_INFO(LogSceneTerrain, "control: %u steep of %llu at ratio 1/250", ST.Steep,
					static_cast<unsigned long long>(ST.Triangles));
	VAELEN_LOG_INFO(LogSceneTerrain,
					"steep: %u of %llu at 128 (z %d..%d cm, terrain %016llx), %u of %llu at 256 (z %d..%d cm, terrain "
					"%016llx)",
					S128.Steep, static_cast<unsigned long long>(S128.Triangles), S128.MinZ, S128.MaxZ,
					static_cast<unsigned long long>(S128.Digest), S256.Steep,
					static_cast<unsigned long long>(S256.Triangles), S256.MinZ, S256.MaxZ,
					static_cast<unsigned long long>(S256.Digest));
}

VAELEN_TEST(Terrain, WithoutReliefTheLandIsFlat)
{
	// CONTROL: the instrument can see relief. No relief, no carving, no detail:
	// every lattice point between land (or lake) tiles is at 0.
	SceneScale Flat;
	Flat.ReliefPerMille = 0;
	Flat.RiverCm = 0;
	Flat.LakeDepthCm = 0;
	Flat.DetailCm = 0;
	Ground G;
	VT_CHECK(BuildGround(Map128(), Flat, G));
	uint32 Zero = 0;
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		if (G.Kind[T] != GroundKind::Sea)
		{
			VT_CHECK_EQ(G.CentreCm[T], 0);
			Zero += G.CentreCm[T] == 0 ? 1u : 0u;
		}
	}
	VT_CHECK(Zero > 1000u);
	// And the wrinkle alone moves the land: detail on, relief off.
	Flat.DetailCm = 200;
	Ground Wrinkled;
	VT_CHECK(BuildGround(Map128(), Flat, Wrinkled));
	VT_CHECK(Everything(Wrinkled, 1).Digest != Everything(G, 1).Digest);
}

VAELEN_TEST(Terrain, TheOuterHalfTileIsWrinkledOnEverySide)
{
	// LatticeZ's wrinkle reaches the map's outer half tile on all four sides
	// alike (the review of 2026-09-27: the west and north bands lost it on
	// every centre row and column, the east and south kept it - a point past
	// the first centre was written as a centre by the rim's clamp). A flat
	// all-land map, 4 x 4 at 10 m, detail on: on each centre row the points
	// of the west band and of the east band that differ from the centre's
	// height are counted, and both bands have some.
	View::MapView Flat;
	Flat.Width = 4;
	Flat.Height = 4;
	Flat.Tiles.resize(16);
	for (View::TileView& Tile : Flat.Tiles)
	{
		Tile.Ground = View::GroundFlag::Land;
		Tile.Elevation = 10 * 65536;
	}
	Ground G;
	VT_REQUIRE(BuildGround(Flat, SceneScale{}, G));
	const int32 S = G.Scale.Steps;
	const int32 Half = S / 2;
	uint32 West = 0, East = 0, North = 0, South = 0, Centres = 0;
	for (uint32 Y = 0; Y < G.Height; ++Y)
	{
		const int32 V = CentreU(G, Y);
		Centres += LatticeZ(G, CentreU(G, 0), V) == G.CentreCm[Y * G.Width] ? 1u : 0u;
		for (int32 U = 0; U < Half; ++U)
		{
			West += LatticeZ(G, U, V) != G.CentreCm[Y * G.Width] ? 1u : 0u;
			const int32 Mirror = static_cast<int32>(LatticeAcross(G)) - 1 - U;
			East += LatticeZ(G, Mirror, V) != G.CentreCm[Y * G.Width + G.Width - 1u] ? 1u : 0u;
		}
	}
	for (uint32 X = 0; X < G.Width; ++X)
	{
		const int32 U = CentreU(G, X);
		for (int32 V = 0; V < Half; ++V)
		{
			North += LatticeZ(G, U, V) != G.CentreCm[X] ? 1u : 0u;
			const int32 Mirror = static_cast<int32>(LatticeDown(G)) - 1 - V;
			South += LatticeZ(G, U, Mirror) != G.CentreCm[(G.Height - 1u) * G.Width + X] ? 1u : 0u;
		}
	}
	VT_CHECK_EQ(Centres, G.Height);
	VT_CHECK_MSG(West > 0u && East > 0u && North > 0u && South > 0u,
				 "wrinkled rim points: west %u east %u north %u south %u", West, East, North, South);
	VAELEN_LOG_INFO(LogSceneTerrain, "rim wrinkles on centre rows and columns: west %u east %u north %u south %u", West,
					East, North, South);
}

VAELEN_TEST(Terrain, TheGroundOutlivesItsWorld)
{
	// Built from a MapView whose world is gone (MapOf's scope), and again from
	// a copy: one ground, one digest.
	View::MapView Copy = Map128();
	Ground A, B;
	VT_CHECK(BuildGround(Map128(), SceneScale{}, A));
	VT_CHECK(BuildGround(Copy, SceneScale{}, B));
	Copy.Tiles.clear();
	VT_CHECK_DIGEST_EQ(Everything(A, 8).Digest, Everything(B, 8).Digest);
	// An empty map and an unusable scale are refused, not built.
	Ground None;
	VT_CHECK(!BuildGround(View::MapView{}, SceneScale{}, None));
	SceneScale Odd;
	Odd.Steps = 3;
	VT_CHECK(!BuildGround(Map128(), Odd, None));
	VT_CHECK(None.CentreCm.empty());
}

VAELEN_TEST(Terrain, TheLineIsComposedOnceAllOrNothing)
{
	TerrainStats S;
	S.Chunks = 64;
	S.Vertices = 1065024;
	S.Triangles = 2097152;
	S.Steep = 12;
	S.MinZ = -5000;
	S.MaxZ = 50630;
	S.Digest = 0x123456789abcdefull; // fifteen digits: a sample, not a pin (printed zero-padded)
	char Line[TerrainLineBytes];
	const uint32 Wrote = TerrainLine(128, 0x41454c564f52ull, 0, S, Line, TerrainLineBytes);
	VT_CHECK_EQ(std::string(Line, Wrote),
				std::string("LogVaelenScene: AELVOR 128 seed 41454c564f52 region all: chunks 64, vertices 1065024, "
							"triangles 2097152, z [-5000, 50630] cm, steep 12 of 2097152; terrain 0123456789abcdef"));
	VT_CHECK_EQ(TerrainLine(128, 0x41454c564f52ull, 7, S, Line, 40u), 0u);
	VT_CHECK_EQ(Line[0], '\0');
	// The counts are 64-bit (the review of 2026-09-27): a map BuildGround
	// admits, 8192 tiles a side, has 2^33 triangles, and the tally must not
	// wrap at 2^32 - nor the line print it wrapped.
	Ground G;
	VT_REQUIRE(BuildGround(Map128(), SceneScale{}, G));
	TerrainMesh Small;
	VT_REQUIRE(BuildChunk(G, 0, 0, 8, Small));
	TerrainStats Tall;
	Tall.Vertices = 0xFFFFFFFFull;
	Tall.Triangles = 0xFFFFFFFFull;
	MeasureTerrain(Small, Tall);
	VT_CHECK(Tall.Vertices == 0xFFFFFFFFull + Small.Vertices.size());
	VT_CHECK(Tall.Triangles == 0xFFFFFFFFull + Small.Triangles.size() / 3u);
	VT_CHECK(Tall.Vertices > 0xFFFFFFFFull && Tall.Triangles > 0xFFFFFFFFull);
	const uint32 Wide = TerrainLine(8192, 0x41454c564f52ull, 0, Tall, Line, TerrainLineBytes);
	VT_CHECK(std::string(Line, Wide).find("vertices 4294967584, triangles 4294967807,") != std::string::npos);
}

VAELEN_TEST(Terrain, AMapWhoseFarEdgeOverflowsACentimetreIsRefused)
{
	// 19.09b, found by the review: a vertex's X is an int32 of centimetres,
	// and 2148 tiles at 1e6 cm each wrapped one to the wrong side of the map.
	// The scale stays usable; the map at that scale does not.
	View::MapView Wide;
	Wide.Width = 2148;
	Wide.Height = 1;
	Wide.Tiles.resize(2148);
	SceneScale Huge;
	Huge.CmPerTile = 1000000;
	VT_CHECK(IsUsableScale(Huge));
	Ground G;
	VT_CHECK(!BuildGround(Wide, Huge, G));
	VT_CHECK_EQ(G.Width, 0u);
	// CONTROL: the same map two tiles narrower fits, and builds.
	Wide.Width = 2146;
	Wide.Tiles.resize(2146);
	VT_CHECK(BuildGround(Wide, Huge, G));
	VT_CHECK_EQ(G.Width, 2146u);
}
