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
		(void)Inner;
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
	RandomStream Draw(0x19050000ull);
	TerrainMesh M;
	uint32 LastChunk = 0xFFFFFFFFu;
	uint32 Checked = 0;
	for (uint32 i = 0; i < 10000u; ++i)
	{
		// A point inside the map, away from its outer half tile.
		const int64 X = static_cast<int64>(Draw.NextU32() % ((G.Width - 1u) * static_cast<uint32>(G.Scale.CmPerTile)));
		const int64 Y = static_cast<int64>(Draw.NextU32() % ((G.Height - 1u) * static_cast<uint32>(G.Scale.CmPerTile)));
		const int32 Got = HeightAt(G, X, Y);
		// The second instrument: the triangle of the BUILT mesh under the point,
		// and the plane through its three vertices.
		uint32 Tile = 0;
		VT_CHECK(TileOfPoint(G, X, Y, Tile));
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
		const TerrainVertex& A = M.Vertices[J * M.Across + I];
		const TerrainVertex& B = M.Vertices[J * M.Across + I + 1u];
		const TerrainVertex& Cc = M.Vertices[(J + 1u) * M.Across + I + 1u];
		const TerrainVertex& D = M.Vertices[(J + 1u) * M.Across + I];
		const int64 FU = X - A.X;
		const int64 FV = Y - A.Y;
		int64 Sum = 0;
		if (FU >= FV)
		{
			Sum = int64{A.Z} * L + (int64{B.Z} - A.Z) * FU + (int64{Cc.Z} - B.Z) * FV;
		}
		else
		{
			Sum = int64{A.Z} * L + (int64{Cc.Z} - D.Z) * FU + (int64{D.Z} - A.Z) * FV;
		}
		int64 Want = Sum / L;
		if (Sum % L != 0 && Sum < 0)
		{
			--Want;
		}
		VT_CHECK_EQ(int64{Got}, Want);
		Checked += int64{Got} == Want ? 1u : 0u;
	}
	VT_CHECK_EQ(Checked, 10000u);
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
	VAELEN_LOG_INFO(LogSceneTerrain, "control: %u steep of %u at ratio 1/250", ST.Steep, ST.Triangles);
	VAELEN_LOG_INFO(LogSceneTerrain,
					"steep: %u of %u at 128 (z %d..%d cm, terrain %016llx), %u of %u at 256 (z %d..%d cm, terrain "
					"%016llx)",
					S128.Steep, S128.Triangles, S128.MinZ, S128.MaxZ, static_cast<unsigned long long>(S128.Digest),
					S256.Steep, S256.Triangles, S256.MinZ, S256.MaxZ, static_cast<unsigned long long>(S256.Digest));
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
}
