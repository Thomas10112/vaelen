// VAELEN - Phase 19 task 19.07: the fence, over every region of the real map.
//
// The walker is fenced to the walkable tiles of the region its life names
// (ADR-0155). These cases hold the fence to the grid it is cut from, by an
// independent count, and the arrival point to the region it names.
#include "Vaelen/Scene/Fence.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/View/Take.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Scene;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogSceneFence);

	Ground GroundOf(uint32 Size)
	{
		View::MapView Map;
		{
			Run::Options O;
			O.Size = Size;
			O.PreHistory = 1;
			O.Years = 0;
			O.Climate = false;
			Run::Aelvor A(O);
			if (A.Begin())
			{
				View::TakeMapView(A.Instance(), A.Sources(), Map);
			}
		}
		Ground G;
		BuildGround(Map, SceneScale{}, G);
		return G;
	}

	const Ground& G128()
	{
		static const Ground G = GroundOf(128);
		return G;
	}

	const Ground& G256()
	{
		static const Ground G = GroundOf(256);
		return G;
	}

	std::set<uint32> RegionsOf(const Ground& G)
	{
		std::set<uint32> Out;
		for (const uint16 R : G.Region)
		{
			if (R != 0u)
			{
				Out.insert(R);
			}
		}
		return Out;
	}

	/// The second instrument: the fence's length counted by scanning rows and
	/// columns for changes between walkable and not, the map's edge counted as
	/// not - no neighbour lookup, no per-tile loop over four sides.
	uint32 ScannedPerimeter(const Ground& G, uint32 Region)
	{
		uint32 Count = 0;
		for (uint32 Y = 0; Y < G.Height; ++Y)
		{
			bool Was = false;
			for (uint32 X = 0; X <= G.Width; ++X)
			{
				const bool Is = X < G.Width && IsWalkable(G, Y * G.Width + X, Region);
				Count += Is != Was ? 1u : 0u;
				Was = Is;
			}
		}
		for (uint32 X = 0; X < G.Width; ++X)
		{
			bool Was = false;
			for (uint32 Y = 0; Y <= G.Height; ++Y)
			{
				const bool Is = Y < G.Height && IsWalkable(G, Y * G.Width + X, Region);
				Count += Is != Was ? 1u : 0u;
				Was = Is;
			}
		}
		return Count;
	}
} // namespace

VAELEN_TEST(Fence, EveryRegionIsClosedAndCountedTwice)
{
	for (const Ground* G : {&G128(), &G256()})
	{
		uint32 Regions = 0;
		uint32 Edges = 0;
		uint32 LakeTilesKept = 0;
		for (const uint32 R : RegionsOf(*G))
		{
			std::vector<FenceEdge> Fence;
			BuildFence(*G, R, Fence);
			// Every edge has a walkable tile on one side and not on the other.
			std::map<std::pair<int32, int32>, uint32> Degree;
			for (const FenceEdge& E : Fence)
			{
				VT_CHECK(IsWalkable(*G, E.Inside, R));
				VT_CHECK(E.Outside == NoTile || !IsWalkable(*G, E.Outside, R));
				++Degree[{E.X0, E.Y0}];
				++Degree[{E.X1, E.Y1}];
				LakeTilesKept += E.Outside != NoTile && G->Kind[E.Outside] == GroundKind::Lake ? 1u : 0u;
			}
			// Closed: every corner is met an even number of times.
			for (const auto& [Corner, Count] : Degree)
			{
				VT_CHECK_MSG(Count % 2u == 0u, "region %u: corner (%d, %d) met %u times", R, Corner.first,
							 Corner.second, Count);
			}
			// Two instruments on one length.
			VT_CHECK_EQ(static_cast<uint32>(Fence.size()), ScannedPerimeter(*G, R));
			Edges += static_cast<uint32>(Fence.size());
			++Regions;
		}
		VT_CHECK(Regions > 90u);
		// A lake inside a region is outside the fence: the walker does not walk on water.
		VT_CHECK(LakeTilesKept > 0u);
		VAELEN_LOG_INFO(LogSceneFence, "%u: %u regions, %u fence edges, %u of them against a lake", G->Width, Regions,
						Edges, LakeTilesKept);
	}
}

VAELEN_TEST(Fence, APointIsInsideExactlyOnTheWalkableTiles)
{
	const Ground& G = G128();
	uint32 In = 0;
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		int64 X = 0, Y = 0;
		PointOfTile(G, T, X, Y);
		const uint32 R = G.Region[T];
		VT_CHECK_EQ(RegionAt(G, X, Y), static_cast<uint32>(R));
		if (R != 0u)
		{
			VT_CHECK_EQ(Inside(G, R, X, Y), IsWalkable(G, T, R));
			In += Inside(G, R, X, Y) ? 1u : 0u;
		}
	}
	VT_CHECK(In > 5000u);
	VT_CHECK_EQ(RegionAt(G, -1000000, 0), 0u);
}

VAELEN_TEST(Fence, AnArrivalLandsInItsRegionNextToWhereItCameFrom)
{
	const Ground& G = G128();
	uint32 Pairs = 0;
	uint32 Adjacent = 0;
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		const uint32 From = G.Region[T];
		if (From == 0u || !IsWalkable(G, T, From) || (T % G.Width) + 1u >= G.Width)
		{
			continue;
		}
		const uint32 N = T + 1u; // the tile east
		const uint32 To = G.Region[N];
		if (To == 0u || To == From || !IsWalkable(G, N, To))
		{
			continue;
		}
		int64 X = 0, Y = 0;
		PointOfTile(G, T, X, Y);
		uint32 Landed = 0;
		VT_CHECK(Arrival(G, To, X, Y, Landed));
		VT_CHECK(IsWalkable(G, Landed, To));
		// Standing on the border, the nearest tile of the other side is one across
		// it: four-adjacent to where the walker stood (a tie goes to the lower index).
		const int64 DX = static_cast<int64>(Landed % G.Width) - static_cast<int64>(T % G.Width);
		const int64 DY = static_cast<int64>(Landed / G.Width) - static_cast<int64>(T / G.Width);
		const bool Next = (DX == 0 && (DY == 1 || DY == -1)) || (DY == 0 && (DX == 1 || DX == -1));
		VT_CHECK(Next);
		Adjacent += Next ? 1u : 0u;
		++Pairs;
		// And a walker put back after a day that took it there stands on that tile's centre.
		int64 PX = X, PY = Y;
		VT_CHECK(PlaceAfterDay(G, To, PX, PY));
		VT_CHECK(Inside(G, To, PX, PY));
		// Already inside: not moved.
		VT_CHECK(!PlaceAfterDay(G, To, PX, PY));
	}
	VT_CHECK(Pairs > 100u);
	VT_CHECK_EQ(Adjacent, Pairs);
}

VAELEN_TEST(Fence, CrossingOnlyTowardsARegionTheLifeListsAsNear)
{
	const Ground& G = G128();
	// Find a border between two walkable regions A | B.
	uint32 A = 0, B = 0, TA = 0, TB = 0;
	for (uint32 T = 0; T + 1u < G.Width * G.Height && A == 0u; ++T)
	{
		if ((T % G.Width) + 1u < G.Width && G.Region[T] != 0u && G.Region[T + 1u] != 0u &&
			G.Region[T] != G.Region[T + 1u] && IsWalkable(G, T, G.Region[T]) && IsWalkable(G, T + 1u, G.Region[T + 1u]))
		{
			A = G.Region[T];
			B = G.Region[T + 1u];
			TA = T;
			TB = T + 1u;
		}
	}
	VT_REQUIRE(A != 0u);
	View::LifeView Life;
	Life.Region = A;
	int64 AX = 0, AY = 0, BX = 0, BY = 0;
	PointOfTile(G, TA, AX, AY);
	PointOfTile(G, TB, BX, BY);
	// B not listed: the page's answer, nothing for the world.
	Crossing C = CrossingOf(G, Life, BX, BY);
	VT_CHECK_EQ(C.Why, CrossingWhy::NotNear);
	VT_CHECK_EQ(C.Region, 0u);
	// B listed: a crossing, naming B.
	Life.Near[0] = B;
	Life.NearCount = 1;
	C = CrossingOf(G, Life, BX, BY);
	VT_CHECK_EQ(C.Why, CrossingWhy::Crossing);
	VT_CHECK_EQ(C.Region, B);
	// Facing home: nothing to cross.
	VT_CHECK_EQ(CrossingOf(G, Life, AX, AY).Why, CrossingWhy::Home);
	// Facing off the map.
	VT_CHECK_EQ(CrossingOf(G, Life, -10000000, AY).Why, CrossingWhy::OffMap);
	// Facing the sea: find any sea tile.
	for (uint32 T = 0; T < G.Width * G.Height; ++T)
	{
		if (G.Kind[T] == GroundKind::Sea)
		{
			int64 SX = 0, SY = 0;
			PointOfTile(G, T, SX, SY);
			VT_CHECK_EQ(CrossingOf(G, Life, SX, SY).Why, CrossingWhy::Water);
			break;
		}
	}
}
