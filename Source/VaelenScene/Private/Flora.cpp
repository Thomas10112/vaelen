// VAELEN - VaelenScene
// Phase 23 task 23.02: forests. See Flora.h.
//
// STATUS: VALIDATED headless (Phase 23 task 23.02)
#include "Vaelen/Scene/Flora.h"

#include "Vaelen/Scene/LineWriter.h"

namespace Vaelen::Scene
{
	namespace
	{
		constexpr Hash64 FloraSalt = 0x466c6f7261ull; // "Flora"

		/// The kind a biome grows, and the height band of its trees (cm).
		uint8 KindOf(uint8 Biome)
		{
			switch (Biome)
			{
			case 3:
				return TreeKind::Conifer;
			case 5:
				return TreeKind::Broadleaf;
			case 8:
				return TreeKind::Palm;
			default:
				return TreeKind::Shrub;
			}
		}

		uint32 HeightOf(uint8 Kind, Hash64 H)
		{
			switch (Kind)
			{
			case TreeKind::Conifer:
				return 900u + static_cast<uint32>(H % 1200u); // 9 to 21 m
			case TreeKind::Broadleaf:
				return 700u + static_cast<uint32>(H % 900u); // 7 to 16 m
			case TreeKind::Palm:
				return 800u + static_cast<uint32>(H % 1000u); // 8 to 18 m
			default:
				return 150u + static_cast<uint32>(H % 200u); // 1.5 to 3.5 m
			}
		}

		/// A point inside the tile, Margin from its edges, from the hash's bits
		/// (Layout.cpp's PointIn, the same formula).
		void PointIn(const Ground& G, uint32 Tile, Hash64 H, int32 Margin, int64& X, int64& Y)
		{
			PointOfTile(G, Tile, X, Y);
			const int64 Span = G.Scale.CmPerTile - 2 * int64{Margin};
			if (Span <= 0)
			{
				return;
			}
			X += static_cast<int64>((H >> 20) % static_cast<uint64>(Span)) - Span / 2;
			Y += static_cast<int64>((H >> 42) % static_cast<uint64>(Span)) - Span / 2;
		}

		/// The tiles nothing may be planted on: water, and what the layout put
		/// there - a road's every tile, the tile under a house, a square, a pit.
		void MarkTaken(const Ground& G, const SceneLayout& L, std::vector<uint8>& Taken)
		{
			Taken.assign(static_cast<usize>(G.Width) * G.Height, 0u);
			for (uint32 T = 0; T < G.Width * G.Height; ++T)
			{
				if (G.Kind[T] != GroundKind::Land)
				{
					Taken[T] = 1u;
				}
			}
			const auto Under = [&](const std::vector<Placed>& List)
			{
				for (const Placed& P : List)
				{
					uint32 Tile = 0;
					if (TileOfPoint(G, P.X, P.Y, Tile))
					{
						Taken[Tile] = 1u;
					}
				}
			};
			Under(L.Houses);
			Under(L.Squares);
			Under(L.Pits);
			for (const RoadPath& R : L.Roads)
			{
				for (const uint32 T : R.Tiles)
				{
					if (T < Taken.size())
					{
						Taken[T] = 1u;
					}
				}
			}
		}
	} // namespace

	void PlantTrees(const Ground& G, const SceneLayout& L, uint32 Region, const FloraRules& Rules, Flora& Out)
	{
		Out.Region = Region;
		Out.Dropped = 0;
		Out.Trees.clear();
		if (G.Width == 0u || G.Height == 0u || Region == 0u)
		{
			return;
		}
		// The near chunks: the land's rule, once per chunk.
		const uint32 Across = ChunksAcross(G);
		const uint32 Down = ChunksDown(G);
		std::vector<uint8> Near(static_cast<usize>(Across) * Down, 0u);
		for (uint32 CY = 0; CY < Down; ++CY)
		{
			for (uint32 CX = 0; CX < Across; ++CX)
			{
				Near[static_cast<usize>(CY) * Across + CX] = ChunkHolds(G, CX, CY, Region) ? 1u : 0u;
			}
		}
		std::vector<uint8> Taken;
		MarkTaken(G, L, Taken);
		// First pass: what each tile would hold, and the total; second: the
		// trees, scaled by the cap when the total is over it (every tile by
		// the same ratio, so the wood thins and no tile goes bare first).
		std::vector<uint32> Count(static_cast<usize>(G.Width) * G.Height, 0u);
		uint64 Total = 0;
		const uint32 Divisor = Rules.RingDivisor == 0u ? 1u : Rules.RingDivisor;
		for (uint32 Y = 0; Y < G.Height; ++Y)
		{
			for (uint32 X = 0; X < G.Width; ++X)
			{
				const uint32 T = Y * G.Width + X;
				if (Taken[T] != 0u || Near[static_cast<usize>(Y / ChunkTiles) * Across + X / ChunkTiles] == 0u)
				{
					continue;
				}
				const uint8 B = G.Biome[T];
				const uint32 Full = B < View::BiomeKinds ? Rules.PerTile[B] : 0u;
				Count[T] = G.Region[T] == Region ? Full : Full / Divisor;
				Total += Count[T];
			}
		}
		for (uint32 T = 0; T < G.Width * G.Height; ++T)
		{
			uint32 N = Count[T];
			if (N == 0u)
			{
				continue;
			}
			if (Total > Rules.Cap)
			{
				N = static_cast<uint32>(uint64{N} * Rules.Cap / Total);
				Out.Dropped += Count[T] - N;
			}
			const uint8 Kind = KindOf(G.Biome[T]);
			const uint8 Ring = G.Region[T] == Region ? 0u : 1u;
			for (uint32 I = 0; I < N; ++I)
			{
				const Hash64 H = Mix64(HashCombine(HashCombine(FloraSalt, HashUInt64(T)), HashUInt64(I)));
				int64 X = 0, Y = 0;
				PointIn(G, T, H, static_cast<int32>(Rules.MarginCm), X, Y);
				Tree Planted;
				Planted.X = static_cast<int32>(X);
				Planted.Y = static_cast<int32>(Y);
				Planted.Z = HeightAt(G, X, Y);
				Planted.HeightCm = static_cast<uint16>(HeightOf(Kind, H & 0xFFFFFu));
				Planted.Kind = Kind;
				Planted.Ring = Ring;
				Out.Trees.push_back(Planted);
			}
		}
	}

	FloraStats MeasureFlora(const Flora& F)
	{
		FloraStats S;
		Hash64 H = HashUInt64((uint64{F.Region} << 32) | F.Trees.size());
		for (const Tree& T : F.Trees)
		{
			H = HashBytes(reinterpret_cast<const char*>(&T), sizeof(T), H);
			if (T.Kind < TreeKind::Count)
			{
				++S.Kinds[T.Kind];
			}
			S.Ring += T.Ring != 0u ? 1u : 0u;
		}
		H = HashCombine(H, HashUInt64(F.Dropped));
		S.Trees = static_cast<uint32>(F.Trees.size());
		S.Dropped = F.Dropped;
		S.Digest = H;
		return S;
	}

	uint32 FloraLine(uint32 Size, uint64 Seed, uint32 Region, const FloraStats& S, char* Out, uint32 Bytes)
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
		W.Put(" flora region ");
		W.Unsigned(Region);
		W.Put(": trees ");
		W.Unsigned(S.Trees);
		W.Put(" (conifer ");
		W.Unsigned(S.Kinds[TreeKind::Conifer]);
		W.Put(", broadleaf ");
		W.Unsigned(S.Kinds[TreeKind::Broadleaf]);
		W.Put(", palm ");
		W.Unsigned(S.Kinds[TreeKind::Palm]);
		W.Put(", shrub ");
		W.Unsigned(S.Kinds[TreeKind::Shrub]);
		W.Put("), ring ");
		W.Unsigned(S.Ring);
		W.Put(", dropped ");
		W.Unsigned(S.Dropped);
		W.Put("; flora ");
		W.Hex(S.Digest, 16u);
		return W.Finish();
	}
} // namespace Vaelen::Scene
