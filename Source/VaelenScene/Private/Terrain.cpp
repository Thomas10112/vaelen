// VAELEN - VaelenScene
// Phase 19 task 19.05: the ground one walks on. See Terrain.h for what is
// invented here and why every figure is an integer.
//
// STATUS: VALIDATED headless (Phase 19 task 19.05)
#include "Vaelen/Scene/Terrain.h"

#include "Vaelen/Scene/LineWriter.h"

namespace Vaelen::Scene
{
	namespace
	{
		/// Floor division, for heights that may be below zero.
		int64 FloorDiv(int64 A, int64 B)
		{
			const int64 Q = A / B;
			return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
		}

		int64 Clamp(int64 X, int64 Lo, int64 Hi)
		{
			return X < Lo ? Lo : (X > Hi ? Hi : X);
		}

		/// The integer square root, floor. Deterministic on every compiler,
		/// which is the only reason it is written out.
		uint64 IntSqrt(uint64 N)
		{
			uint64 Root = 0;
			uint64 Bit = uint64{1} << 62;
			while (Bit > N)
			{
				Bit >>= 2;
			}
			while (Bit != 0u)
			{
				if (N >= Root + Bit)
				{
					N -= Root + Bit;
					Root = (Root >> 1) + Bit;
				}
				else
				{
					Root >>= 1;
				}
				Bit >>= 2;
			}
			return Root;
		}

		uint32 TileIndex(const Ground& G, int64 X, int64 Y)
		{
			return static_cast<uint32>(Y) * G.Width + static_cast<uint32>(X);
		}

		/// The colour of a tile: its biome on land, its water otherwise. Indexed by
		/// WorldGen::Biome through View::BiomeKinds, which the static_assert holds.
		struct Rgb
		{
			uint8 R, G, B;
		};
		constexpr Rgb BiomeColours[] = {
			{28, 52, 86},	 // Ocean
			{232, 240, 244}, // Ice
			{150, 156, 128}, // Tundra
			{58, 92, 60},	 // BorealForest
			{168, 160, 110}, // ColdSteppe
			{70, 118, 58},	 // TemperateForest
			{128, 160, 78},	 // Grassland
			{156, 146, 92},	 // Scrubland
			{40, 104, 44},	 // TropicalForest
			{184, 168, 90},	 // Savanna
			{214, 190, 130}, // Desert
			{132, 128, 124}, // Alpine
		};
		static_assert(sizeof(BiomeColours) / sizeof(BiomeColours[0]) == View::BiomeKinds,
					  "one colour per biome: the drawer once painted every tile one biome colder");
		constexpr Rgb SeaFloor = {30, 48, 72};
		constexpr Rgb LakeBed = {44, 70, 96};
		constexpr Rgb RiverBed = {60, 88, 112};

		Rgb ColourOf(const Ground& G, uint32 Tile)
		{
			switch (G.Kind[Tile])
			{
			case GroundKind::Sea:
				return SeaFloor;
			case GroundKind::Lake:
				return LakeBed;
			case GroundKind::River:
				return RiverBed;
			default:
				break;
			}
			const uint8 B = G.Biome[Tile];
			return BiomeColours[B < View::BiomeKinds ? B : 0u];
		}

		/// The hashed wrinkle of a lattice point, in [-DetailCm, DetailCm].
		int32 DetailAt(const Ground& G, int32 U, int32 V)
		{
			const int32 D = G.Scale.DetailCm;
			if (D <= 0)
			{
				return 0;
			}
			const uint64 H = Mix64(HashCombine(HashUInt64(static_cast<uint32>(U)), HashUInt64(static_cast<uint32>(V))));
			return static_cast<int32>(H % static_cast<uint64>(2 * D + 1)) - D;
		}
	} // namespace

	bool IsUsableScale(const SceneScale& S)
	{
		return S.Steps >= 2 && S.Steps % 2 == 0 && S.Steps <= 64 && S.CmPerTile > 0 && S.CmPerTile % S.Steps == 0 &&
			   S.CmPerTile <= 1000000 && S.ReliefPerMille >= 0 && S.ReliefPerMille <= 4000 &&
			   S.SeaFloorCm >= -1000000 && S.SeaFloorCm <= 0 && S.LakeDepthCm >= 0 && S.LakeDepthCm <= 100000 &&
			   S.RiverCm >= 0 && S.RiverCm <= 100000 && S.DetailCm >= 0 && S.DetailCm <= 100000;
	}

	int32 LandCentreCm(int32 Elevation, const SceneScale& Scale)
	{
		// Elevation is metres in Q16.16 (Land.h); centimetres are *100, the
		// relief is /1000 of that. At most 2^31 * 100 * 4000 < 2^63.
		return static_cast<int32>(FloorDiv(int64{Elevation} * 100 * Scale.ReliefPerMille, int64{1000} * 65536));
	}

	bool BuildGround(const View::MapView& Map, const SceneScale& Scale, Ground& Out)
	{
		Out = Ground{};
		const uint64 Count = uint64{Map.Width} * Map.Height;
		if (Map.Width == 0u || Map.Height == 0u || Map.Tiles.size() != Count || !IsUsableScale(Scale) ||
			Map.Width > 8192u || Map.Height > 8192u)
		{
			return false;
		}
		Out.Width = Map.Width;
		Out.Height = Map.Height;
		Out.Scale = Scale;
		const usize N = static_cast<usize>(Count);
		Out.CentreCm.assign(N, 0);
		Out.LakeSurfaceCm.assign(N, 0);
		Out.Region.assign(N, 0);
		Out.Kind.assign(N, GroundKind::Land);
		Out.Biome.assign(N, 0);

		std::vector<int32> Raw(N, 0);
		for (usize i = 0; i < N; ++i)
		{
			const View::TileView& T = Map.Tiles[i];
			Out.Region[i] = T.Region;
			Out.Biome[i] = T.Biome;
			Raw[i] = LandCentreCm(T.Elevation, Scale);
			if ((T.Ground & View::GroundFlag::Lake) != 0u)
			{
				Out.Kind[i] = GroundKind::Lake;
			}
			else if ((T.Ground & View::GroundFlag::Land) == 0u)
			{
				Out.Kind[i] = GroundKind::Sea;
			}
			else if ((T.Ground & View::GroundFlag::River) != 0u)
			{
				Out.Kind[i] = GroundKind::River;
			}
		}

		// Lakes: each 4-connected lake gets ONE surface, the lowest centre of
		// the land around it (INVENTED - the kernel keeps no spill level), and
		// its floor LakeDepthCm under that. A lake with no land shore takes its
		// own lowest raw height.
		std::vector<uint32> Stack;
		std::vector<uint32> Members;
		std::vector<uint8> Seen(N, 0);
		const int32 DX[4] = {1, -1, 0, 0};
		const int32 DY[4] = {0, 0, 1, -1};
		for (usize Start = 0; Start < N; ++Start)
		{
			if (Out.Kind[Start] != GroundKind::Lake || Seen[Start] != 0u)
			{
				continue;
			}
			Members.clear();
			Stack.assign(1, static_cast<uint32>(Start));
			Seen[Start] = 1;
			bool HasShore = false;
			int32 Shore = 0;
			int32 Lowest = Raw[Start];
			while (!Stack.empty())
			{
				const uint32 At = Stack.back();
				Stack.pop_back();
				Members.push_back(At);
				Lowest = Raw[At] < Lowest ? Raw[At] : Lowest;
				const int32 X = static_cast<int32>(At % Map.Width);
				const int32 Y = static_cast<int32>(At / Map.Width);
				for (int32 d = 0; d < 4; ++d)
				{
					const int32 NX = X + DX[d];
					const int32 NY = Y + DY[d];
					if (NX < 0 || NY < 0 || NX >= static_cast<int32>(Map.Width) || NY >= static_cast<int32>(Map.Height))
					{
						continue;
					}
					const uint32 Next = TileIndex(Out, NX, NY);
					if (Out.Kind[Next] == GroundKind::Lake)
					{
						if (Seen[Next] == 0u)
						{
							Seen[Next] = 1;
							Stack.push_back(Next);
						}
					}
					else if (Out.Kind[Next] != GroundKind::Sea)
					{
						Shore = HasShore ? (Raw[Next] < Shore ? Raw[Next] : Shore) : Raw[Next];
						HasShore = true;
					}
				}
			}
			const int32 Surface = HasShore ? Shore : Lowest;
			for (const uint32 M : Members)
			{
				Out.LakeSurfaceCm[M] = Surface;
			}
		}

		for (usize i = 0; i < N; ++i)
		{
			switch (Out.Kind[i])
			{
			case GroundKind::Sea:
				Out.CentreCm[i] = Scale.SeaFloorCm;
				break;
			case GroundKind::Lake:
			{
				const int32 Floor = Out.LakeSurfaceCm[i] - Scale.LakeDepthCm;
				Out.CentreCm[i] = Raw[i] < Floor ? Raw[i] : Floor;
				break;
			}
			case GroundKind::River:
				Out.CentreCm[i] = Raw[i] - Scale.RiverCm;
				break;
			default:
				Out.CentreCm[i] = Raw[i];
				break;
			}
		}
		return true;
	}

	uint32 LatticeAcross(const Ground& G)
	{
		return G.Width * static_cast<uint32>(G.Scale.Steps) + 1u;
	}

	uint32 LatticeDown(const Ground& G)
	{
		return G.Height * static_cast<uint32>(G.Scale.Steps) + 1u;
	}

	int32 LatticeZ(const Ground& G, int32 U, int32 V)
	{
		if (G.Width == 0u || G.Height == 0u)
		{
			return 0;
		}
		const int64 S = G.Scale.Steps;
		const int64 Half = S / 2;
		U = static_cast<int32>(Clamp(U, 0, static_cast<int64>(LatticeAcross(G)) - 1));
		V = static_cast<int32>(Clamp(V, 0, static_cast<int64>(LatticeDown(G)) - 1));
		// Where the point falls between tile centres: centre x is at U = x*S + S/2.
		const int64 PU = int64{U} - Half;
		const int64 PV = int64{V} - Half;
		int64 X0 = FloorDiv(PU, S);
		int64 Y0 = FloorDiv(PV, S);
		int64 FX = PU - X0 * S;
		int64 FY = PV - Y0 * S;
		int64 X1 = X0 + 1;
		int64 Y1 = Y0 + 1;
		// Beyond the outermost centres the ground is the edge tile's.
		const int64 LastX = static_cast<int64>(G.Width) - 1;
		const int64 LastY = static_cast<int64>(G.Height) - 1;
		if (X0 < 0)
		{
			X0 = X1 = 0;
			FX = 0;
		}
		else if (X1 > LastX)
		{
			X1 = LastX;
			if (X0 > LastX)
			{
				X0 = LastX;
			}
		}
		if (Y0 < 0)
		{
			Y0 = Y1 = 0;
			FY = 0;
		}
		else if (Y1 > LastY)
		{
			Y1 = LastY;
			if (Y0 > LastY)
			{
				Y0 = LastY;
			}
		}
		const uint32 T00 = TileIndex(G, X0, Y0);
		const uint32 T10 = TileIndex(G, X1, Y0);
		const uint32 T01 = TileIndex(G, X0, Y1);
		const uint32 T11 = TileIndex(G, X1, Y1);
		const int64 Sum = int64{G.CentreCm[T00]} * (S - FX) * (S - FY) + int64{G.CentreCm[T10]} * FX * (S - FY) +
						  int64{G.CentreCm[T01]} * (S - FX) * FY + int64{G.CentreCm[T11]} * FX * FY;
		int64 Z = FloorDiv(Sum, S * S);
		// Detail on land only, and never on a tile centre: a centre is the kernel's.
		const bool Centre = FX == 0 && FY == 0;
		const bool AllLand = G.Kind[T00] == GroundKind::Land && G.Kind[T10] == GroundKind::Land &&
							 G.Kind[T01] == GroundKind::Land && G.Kind[T11] == GroundKind::Land;
		if (!Centre && AllLand)
		{
			Z += DetailAt(G, U, V);
		}
		return static_cast<int32>(Z);
	}

	int32 HeightAt(const Ground& G, int64 Xcm, int64 Ycm)
	{
		if (G.Width == 0u || G.Height == 0u)
		{
			return 0;
		}
		const int64 S = G.Scale.Steps;
		const int64 L = G.Scale.CmPerTile / S;
		// Lattice point U sits at (U - S/2) * L.
		const int64 GX = Clamp(Xcm + (S / 2) * L, 0, (static_cast<int64>(LatticeAcross(G)) - 1) * L);
		const int64 GY = Clamp(Ycm + (S / 2) * L, 0, (static_cast<int64>(LatticeDown(G)) - 1) * L);
		int64 U0 = FloorDiv(GX, L);
		int64 V0 = FloorDiv(GY, L);
		int64 FU = GX - U0 * L;
		int64 FV = GY - V0 * L;
		// On the far edge the point is the last lattice point, not a quad past it.
		if (U0 >= static_cast<int64>(LatticeAcross(G)) - 1)
		{
			U0 = static_cast<int64>(LatticeAcross(G)) - 2;
			FU = L;
		}
		if (V0 >= static_cast<int64>(LatticeDown(G)) - 1)
		{
			V0 = static_cast<int64>(LatticeDown(G)) - 2;
			FV = L;
		}
		const int32 U = static_cast<int32>(U0);
		const int32 V = static_cast<int32>(V0);
		const int64 Z00 = LatticeZ(G, U, V);
		const int64 Z10 = LatticeZ(G, U + 1, V);
		const int64 Z11 = LatticeZ(G, U + 1, V + 1);
		const int64 Z01 = LatticeZ(G, U, V + 1);
		// The quad's (U, V)-(U+1, V+1) diagonal: the triangle below it holds FU >= FV.
		const int64 Sum =
			FU >= FV ? Z00 * L + (Z10 - Z00) * FU + (Z11 - Z10) * FV : Z00 * L + (Z11 - Z01) * FU + (Z01 - Z00) * FV;
		return static_cast<int32>(FloorDiv(Sum, L));
	}

	void PointOfTile(const Ground& G, uint32 Tile, int64& Xcm, int64& Ycm)
	{
		const uint32 W = G.Width == 0u ? 1u : G.Width;
		Xcm = int64{Tile % W} * G.Scale.CmPerTile;
		Ycm = int64{Tile / W} * G.Scale.CmPerTile;
	}

	bool TileOfPoint(const Ground& G, int64 Xcm, int64 Ycm, uint32& Tile)
	{
		// Half-open: a tile is [centre - half, centre + half).
		const int64 C = G.Scale.CmPerTile;
		const int64 X = FloorDiv(Xcm + C / 2, C);
		const int64 Y = FloorDiv(Ycm + C / 2, C);
		if (X < 0 || Y < 0 || X >= static_cast<int64>(G.Width) || Y >= static_cast<int64>(G.Height))
		{
			return false;
		}
		Tile = TileIndex(G, X, Y);
		return true;
	}

	uint32 ChunksAcross(const Ground& G)
	{
		return (G.Width + ChunkTiles - 1u) / ChunkTiles;
	}

	uint32 ChunksDown(const Ground& G)
	{
		return (G.Height + ChunkTiles - 1u) / ChunkTiles;
	}

	bool IsSteep(int64 Dx, int64 Dy, int64 Run)
	{
		return 119 * Run * Run < 121 * (Dx * Dx + Dy * Dy);
	}

	bool BuildPatch(const Ground& G, uint32 TileX0, uint32 TileY0, uint32 TileX1, uint32 TileY1, uint32 Stride,
					TerrainMesh& Out)
	{
		Out = TerrainMesh{};
		const uint32 S = static_cast<uint32>(G.Scale.Steps);
		if (G.Width == 0u || TileX0 >= TileX1 || TileY0 >= TileY1 || TileX1 > G.Width || TileY1 > G.Height ||
			Stride == 0u || S % Stride != 0u)
		{
			return false;
		}
		const int64 L = G.Scale.CmPerTile / G.Scale.Steps;
		const uint32 U0 = TileX0 * S;
		const uint32 V0 = TileY0 * S;
		const uint32 Across = (TileX1 - TileX0) * S / Stride + 1u;
		const uint32 Down = (TileY1 - TileY0) * S / Stride + 1u;
		Out.TileX0 = TileX0;
		Out.TileY0 = TileY0;
		Out.TileX1 = TileX1;
		Out.TileY1 = TileY1;
		Out.Stride = Stride;
		Out.Across = Across;
		Out.Vertices.resize(static_cast<usize>(Across) * Down);
		const int32 K = static_cast<int32>(Stride);
		for (uint32 j = 0; j < Down; ++j)
		{
			for (uint32 i = 0; i < Across; ++i)
			{
				const int32 U = static_cast<int32>(U0 + i * Stride);
				const int32 V = static_cast<int32>(V0 + j * Stride);
				TerrainVertex& Vx = Out.Vertices[static_cast<usize>(j) * Across + i];
				Vx.X = static_cast<int32>((int64{U} - G.Scale.Steps / 2) * L);
				Vx.Y = static_cast<int32>((int64{V} - G.Scale.Steps / 2) * L);
				Vx.Z = LatticeZ(G, U, V);
				// The normal from the heights a stride either side: (-dz/dx, -dz/dy, 1)
				// scaled by 2KL, in integers, then to a unit vector times 32767.
				const int64 DZX = int64{LatticeZ(G, U + K, V)} - LatticeZ(G, U - K, V);
				const int64 DZY = int64{LatticeZ(G, U, V + K)} - LatticeZ(G, U, V - K);
				const int64 NZ = 2 * K * L;
				const uint64 Len = IntSqrt(static_cast<uint64>(DZX * DZX + DZY * DZY + NZ * NZ));
				const int64 Div = Len == 0u ? 1 : static_cast<int64>(Len);
				Vx.NX = static_cast<int16>(-DZX * 32767 / Div);
				Vx.NY = static_cast<int16>(-DZY * 32767 / Div);
				Vx.NZ = static_cast<int16>(NZ * 32767 / Div);
				// The colour of the tile the point lies in (a shared edge takes the
				// tile below and to the right of it, clamped to the map).
				const uint32 TX = static_cast<uint32>(Clamp(U / G.Scale.Steps, 0, static_cast<int64>(G.Width) - 1));
				const uint32 TY = static_cast<uint32>(Clamp(V / G.Scale.Steps, 0, static_cast<int64>(G.Height) - 1));
				const Rgb C = ColourOf(G, TileIndex(G, TX, TY));
				Vx.R = C.R;
				Vx.G = C.G;
				Vx.B = C.B;
			}
		}
		Out.Triangles.reserve(static_cast<usize>(Across - 1u) * (Down - 1u) * 6u);
		for (uint32 j = 0; j + 1u < Down; ++j)
		{
			for (uint32 i = 0; i + 1u < Across; ++i)
			{
				const uint32 A = j * Across + i;
				const uint32 B = A + 1u;
				const uint32 C = A + Across + 1u;
				const uint32 D = A + Across;
				const uint32 Tri[6] = {A, B, C, A, C, D};
				Out.Triangles.insert(Out.Triangles.end(), Tri, Tri + 6);
			}
		}
		return true;
	}

	bool BuildChunk(const Ground& G, uint32 CX, uint32 CY, uint32 Stride, TerrainMesh& Out)
	{
		const uint32 X0 = CX * ChunkTiles;
		const uint32 Y0 = CY * ChunkTiles;
		if (X0 >= G.Width || Y0 >= G.Height)
		{
			Out = TerrainMesh{};
			return false;
		}
		const uint32 X1 = X0 + ChunkTiles < G.Width ? X0 + ChunkTiles : G.Width;
		const uint32 Y1 = Y0 + ChunkTiles < G.Height ? Y0 + ChunkTiles : G.Height;
		return BuildPatch(G, X0, Y0, X1, Y1, Stride, Out);
	}

	void MeasureTerrain(const TerrainMesh& M, TerrainStats& Into)
	{
		if (M.Vertices.empty())
		{
			return;
		}
		if (Into.Vertices == 0u)
		{
			Into.MinZ = M.Vertices[0].Z;
			Into.MaxZ = M.Vertices[0].Z;
		}
		Hash64 H = HashCombine(Into.Digest, HashUInt64(M.Vertices.size()));
		for (const TerrainVertex& V : M.Vertices)
		{
			H = HashBytes(reinterpret_cast<const char*>(&V), sizeof(V), H);
			Into.MinZ = V.Z < Into.MinZ ? V.Z : Into.MinZ;
			Into.MaxZ = V.Z > Into.MaxZ ? V.Z : Into.MaxZ;
		}
		H = HashCombine(H, HashUInt64(M.Triangles.size()));
		for (usize t = 0; t + 2u < M.Triangles.size(); t += 3u)
		{
			H = HashCombine(H, HashUInt64((uint64{M.Triangles[t]} << 32) ^ M.Triangles[t + 1u]));
			H = HashCombine(H, HashUInt64(M.Triangles[t + 2u]));
			// A triangle of the lattice is right-angled at one corner: its reduced
			// normal is (-dz along x, -dz along y, the run).
			const TerrainVertex& A = M.Vertices[M.Triangles[t]];
			const TerrainVertex& B = M.Vertices[M.Triangles[t + 1u]];
			const TerrainVertex& C = M.Vertices[M.Triangles[t + 2u]];
			const int64 Run = int64{B.X} - A.X != 0 ? int64{B.X} - A.X : int64{C.X} - B.X;
			int64 Dx = 0;
			int64 Dy = 0;
			if (B.Y == A.Y)
			{
				// (u, v), (u+s, v), (u+s, v+s)
				Dx = int64{B.Z} - A.Z;
				Dy = int64{C.Z} - B.Z;
			}
			else
			{
				// (u, v), (u+s, v+s), (u, v+s)
				Dx = int64{B.Z} - C.Z;
				Dy = int64{C.Z} - A.Z;
			}
			Into.Steep += IsSteep(Dx, Dy, Run < 0 ? -Run : Run) ? 1u : 0u;
		}
		Into.Digest = H;
		Into.Chunks += 1u;
		Into.Vertices += static_cast<uint32>(M.Vertices.size());
		Into.Triangles += static_cast<uint32>(M.Triangles.size() / 3u);
	}

	uint32 TerrainLine(uint32 Size, uint64 Seed, uint32 Region, const TerrainStats& S, char* Out, uint32 Bytes)
	{
		if (Out == nullptr || Bytes == 0u)
		{
			return 0u;
		}
		Detail::LineWriter W{Out, Bytes};
		W.Put("LogVaelenScene: AELVOR ");
		W.Unsigned(Size);
		W.Put(" seed ");
		W.Hex(Seed, 12u);
		W.Put(" region ");
		if (Region == 0u)
		{
			W.Put("all");
		}
		else
		{
			W.Unsigned(Region);
		}
		W.Put(": chunks ");
		W.Unsigned(S.Chunks);
		W.Put(", vertices ");
		W.Unsigned(S.Vertices);
		W.Put(", triangles ");
		W.Unsigned(S.Triangles);
		W.Put(", z [");
		W.Signed(S.MinZ);
		W.Put(", ");
		W.Signed(S.MaxZ);
		W.Put("] cm, steep ");
		W.Unsigned(S.Steep);
		W.Put(" of ");
		W.Unsigned(S.Triangles);
		W.Put("; terrain ");
		W.Hex(S.Digest, 16u);
		return W.Finish();
	}
} // namespace Vaelen::Scene
