// VAELEN - VaelenScene
// Phase 19 task 19.07: where the walker may stand. See Fence.h.
//
// STATUS: VALIDATED headless (Phase 19 task 19.07)
#include "Vaelen/Scene/Fence.h"

namespace Vaelen::Scene
{
	bool IsWalkable(const Ground& G, uint32 Tile, uint32 Region)
	{
		if (Region == 0u || Tile >= G.Region.size() || G.Region[Tile] != Region)
		{
			return false;
		}
		return G.Kind[Tile] == GroundKind::Land || G.Kind[Tile] == GroundKind::River;
	}

	uint32 RegionAt(const Ground& G, int64 Xcm, int64 Ycm)
	{
		uint32 Tile = 0;
		return TileOfPoint(G, Xcm, Ycm, Tile) ? G.Region[Tile] : 0u;
	}

	bool Inside(const Ground& G, uint32 Region, int64 Xcm, int64 Ycm)
	{
		uint32 Tile = 0;
		return TileOfPoint(G, Xcm, Ycm, Tile) && IsWalkable(G, Tile, Region);
	}

	void BuildFence(const Ground& G, uint32 Region, std::vector<FenceEdge>& Out)
	{
		Out.clear();
		const int64 C = G.Scale.CmPerTile;
		const int64 H = C / 2;
		for (uint32 Y = 0; Y < G.Height; ++Y)
		{
			for (uint32 X = 0; X < G.Width; ++X)
			{
				const uint32 T = Y * G.Width + X;
				if (!IsWalkable(G, T, Region))
				{
					continue;
				}
				const int64 CX = int64{X} * C;
				const int64 CY = int64{Y} * C;
				// West, east, north (y - 1), south (y + 1), each side's two corners
				// wound so the walkable tile is on the same hand of every edge.
				const uint32 Beyond[4] = {X > 0u ? T - 1u : NoTile, X + 1u < G.Width ? T + 1u : NoTile,
										  Y > 0u ? T - G.Width : NoTile, Y + 1u < G.Height ? T + G.Width : NoTile};
				const int64 Corners[4][4] = {{CX - H, CY + H, CX - H, CY - H},
											 {CX + H, CY - H, CX + H, CY + H},
											 {CX - H, CY - H, CX + H, CY - H},
											 {CX + H, CY + H, CX - H, CY + H}};
				for (uint32 s = 0; s < 4u; ++s)
				{
					if (Beyond[s] != NoTile && IsWalkable(G, Beyond[s], Region))
					{
						continue;
					}
					FenceEdge E;
					E.X0 = static_cast<int32>(Corners[s][0]);
					E.Y0 = static_cast<int32>(Corners[s][1]);
					E.X1 = static_cast<int32>(Corners[s][2]);
					E.Y1 = static_cast<int32>(Corners[s][3]);
					E.Inside = T;
					E.Outside = Beyond[s];
					Out.push_back(E);
				}
			}
		}
	}

	Crossing CrossingOf(const Ground& G, const View::LifeView& Life, int64 AheadX, int64 AheadY)
	{
		Crossing Out;
		uint32 Tile = 0;
		if (!TileOfPoint(G, AheadX, AheadY, Tile))
		{
			Out.Why = CrossingWhy::OffMap;
			return Out;
		}
		const uint32 There = G.Region[Tile];
		if (There == Life.Region && G.Kind[Tile] != GroundKind::Sea && G.Kind[Tile] != GroundKind::Lake)
		{
			Out.Why = CrossingWhy::Home;
			return Out;
		}
		if (There == 0u || G.Kind[Tile] == GroundKind::Sea || G.Kind[Tile] == GroundKind::Lake)
		{
			Out.Why = CrossingWhy::Water;
			return Out;
		}
		const uint32 Count = Life.NearCount < View::MostNear ? Life.NearCount : View::MostNear;
		for (uint32 i = 0; i < Count; ++i)
		{
			if (Life.Near[i] == There)
			{
				Out.Region = There;
				Out.Why = CrossingWhy::Crossing;
				return Out;
			}
		}
		Out.Why = CrossingWhy::NotNear;
		return Out;
	}

	bool Arrival(const Ground& G, uint32 To, int64 Xcm, int64 Ycm, uint32& Tile)
	{
		bool Found = false;
		uint64 Best = 0;
		for (uint32 T = 0; T < G.Width * G.Height; ++T)
		{
			if (!IsWalkable(G, T, To))
			{
				continue;
			}
			int64 CX = 0;
			int64 CY = 0;
			PointOfTile(G, T, CX, CY);
			const uint64 DX = static_cast<uint64>(CX > Xcm ? CX - Xcm : Xcm - CX);
			const uint64 DY = static_cast<uint64>(CY > Ycm ? CY - Ycm : Ycm - CY);
			const uint64 D = DX * DX + DY * DY;
			if (!Found || D < Best)
			{
				Found = true;
				Best = D;
				Tile = T;
			}
		}
		return Found;
	}

	bool PlaceAfterDay(const Ground& G, uint32 After, int64& Xcm, int64& Ycm)
	{
		if (Inside(G, After, Xcm, Ycm))
		{
			return false;
		}
		uint32 Tile = 0;
		if (!Arrival(G, After, Xcm, Ycm, Tile))
		{
			return false;
		}
		PointOfTile(G, Tile, Xcm, Ycm);
		return true;
	}
} // namespace Vaelen::Scene
