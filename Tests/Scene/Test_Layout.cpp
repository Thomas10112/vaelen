// VAELEN - Phase 19 task 19.08: what the scene invents, held to what the views hold.
//
// Houses, squares, roads, pits and figures are invented (Layout.h says which
// and why). These cases hold the invention to its rules on a real world:
// nothing on water or in another region, no two houses overlapping, one house
// per living family counted independently, a figure for every living person
// of a detailed region, figures that move with the day and only with it, and
// houses that do not move when a later family dies out.
#include "Vaelen/Scene/Layout.h"
#include "Vaelen/Scene/Fence.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/View/Take.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <cstring>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Scene;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogSceneLayout);

	struct Views
	{
		Ground G;
		View::MapView Map;
		View::WorldView World_;
		View::NetView Net;
		View::PeopleView People;
		View::LifeView Life;
	};

	/// AELVOR 128 after sixty years of pre-history and ten of history: people,
	/// families, settlements and roads - and the world gone once they are taken.
	const Views& Taken()
	{
		static const Views V = []
		{
			Views Out;
			Run::Options O;
			O.Size = 128;
			O.PreHistory = 60;
			O.Years = 10;
			Run::Aelvor A(O);
			if (!A.Begin())
			{
				return Out;
			}
			View::MapView Map;
			View::TakeMapView(A.Instance(), A.Sources(), Map);
			View::TakeView(A.Instance(), A.Sources(), Out.World_);
			View::TakeNetView(A.Instance(), A.Sources(), Out.Net);
			View::TakePeopleView(A.Instance(), A.Sources(), Out.People);
			BuildGround(Map, SceneScale{}, Out.G);
			Out.Map = Map;
			return Out;
		}();
		return V;
	}

	const View::MapView& Map128Of(const Views& V)
	{
		return V.Map;
	}

	bool OnLandOf(const Ground& G, const Placed& P)
	{
		uint32 Tile = 0;
		return TileOfPoint(G, P.X, P.Y, Tile) && G.Region[Tile] == P.Region && G.Kind[Tile] == GroundKind::Land;
	}
} // namespace

VAELEN_TEST(Layout, NothingStandsOnWaterOrInAnotherRegion)
{
	const Views& V = Taken();
	VT_REQUIRE(V.G.Width == 128u);
	SceneLayout L;
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, L);
	const LayoutStats S = MeasureLayout(L);
	VT_CHECK(S.Houses > 100u && S.Figures > 100u && S.Squares > 0u && S.Roads > 0u);
	// PINNED (the review of 2026-09-27: this digest was logged and never
	// asserted, so "the same digest by another wiring" held for nobody):
	// the layout of AELVOR 128 after 60 + 10 years, on day 100 - the same
	// line as Atlas.SceneLayout128's, re-pinned the same day for the slope
	// rule on the diagonals (8542fca56e445402 before).
	VT_CHECK_DIGEST_EQ(S.Digest, 0x6917612c1344027full);
	for (const std::vector<Placed>* List : {&L.Houses, &L.Figures, &L.Squares, &L.Pits})
	{
		for (const Placed& P : *List)
		{
			VT_CHECK_MSG(OnLandOf(V.G, P), "something at (%d, %d) is not on the land of region %u", P.X, P.Y, P.Region);
		}
	}
	// A house's whole 8 m is on its tile's land, and no two houses overlap.
	for (usize i = 0; i < L.Houses.size(); ++i)
	{
		const Placed& A = L.Houses[i];
		for (const int32 DX : {-HouseHalfCm, HouseHalfCm - 1})
		{
			for (const int32 DY : {-HouseHalfCm, HouseHalfCm - 1})
			{
				Placed Corner = A;
				Corner.X += DX;
				Corner.Y += DY;
				VT_CHECK(OnLandOf(V.G, Corner));
			}
		}
		for (usize j = i + 1u; j < L.Houses.size(); ++j)
		{
			const Placed& B = L.Houses[j];
			const bool Apart = B.X - A.X >= 2 * HouseHalfCm || A.X - B.X >= 2 * HouseHalfCm ||
							   B.Y - A.Y >= 2 * HouseHalfCm || A.Y - B.Y >= 2 * HouseHalfCm;
			VT_CHECK(Apart);
		}
	}
	// Every road tile is land or river, and each step is to a neighbour.
	for (const RoadPath& R : L.Roads)
	{
		for (usize k = 0; k < R.Tiles.size(); ++k)
		{
			const uint32 T = R.Tiles[k];
			VT_CHECK(V.G.Kind[T] == GroundKind::Land || V.G.Kind[T] == GroundKind::River);
			if (k > 0u)
			{
				const uint32 P = R.Tiles[k - 1u];
				const uint32 Step = T > P ? T - P : P - T;
				VT_CHECK(Step == 1u || Step == V.G.Width);
			}
		}
	}
	char Line[LayoutLineBytes];
	VT_CHECK(LayoutLine(128, 0x41454c564f52ull, 100, S, Line, LayoutLineBytes) > 0u);
	VAELEN_LOG_INFO(LogSceneLayout, "%s", Line);
}

VAELEN_TEST(Layout, OneHousePerLivingFamilyAndAFigurePerLivingPerson)
{
	const Views& V = Taken();
	SceneLayout L;
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, L);
	// The second instrument: count families and the living from the people
	// view directly, region by region, detailed and coarse apart.
	std::map<uint32, std::set<uint32>> Families;
	uint32 LivingInDetail = 0;
	std::set<uint32> Detailed;
	uint32 CoarseHouses = 0;
	for (const View::RegionView& R : V.World_.Regions)
	{
		if (R.Detailed != 0u)
		{
			Detailed.insert(R.Index);
		}
		else
		{
			CoarseHouses += (R.People + 4u) / 5u;
		}
	}
	for (const View::PersonView& P : V.People.People)
	{
		if (!View::IsAlive(P) || Detailed.count(P.Region) == 0u)
		{
			continue;
		}
		++LivingInDetail;
		if (P.Family != 0u)
		{
			Families[P.Region].insert(P.Family);
		}
	}
	uint32 FamilyHouses = 0;
	for (const auto& [Region, Set] : Families)
	{
		FamilyHouses += static_cast<uint32>(Set.size());
	}
	VT_CHECK(Detailed.size() > 0u);
	VT_CHECK_EQ(static_cast<uint32>(L.Houses.size()) + L.Unplaced, FamilyHouses + CoarseHouses);
	VT_CHECK_EQ(L.Unplaced, 0u);
	// Nobody is played in this world, so every living person of a detailed region stands somewhere.
	VT_CHECK_EQ(static_cast<uint32>(L.Figures.size()), LivingInDetail);
}

VAELEN_TEST(Layout, FiguresMoveWithTheDayAndOnlyWithIt)
{
	const Views& V = Taken();
	SceneLayout A, Again, Next;
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, A);
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, Again);
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 101, Next);
	VT_CHECK_DIGEST_EQ(MeasureLayout(A).Digest, MeasureLayout(Again).Digest);
	VT_REQUIRE(A.Figures.size() == Next.Figures.size());
	uint32 Moved = 0;
	for (usize i = 0; i < A.Figures.size(); ++i)
	{
		VT_CHECK_EQ(A.Figures[i].Key, Next.Figures[i].Key);
		Moved += A.Figures[i].X != Next.Figures[i].X || A.Figures[i].Y != Next.Figures[i].Y ? 1u : 0u;
	}
	VT_CHECK(Moved * 10u >= static_cast<uint32>(A.Figures.size()) * 9u);
	// The houses and the squares are the day's no more than the figures are the house's.
	VT_CHECK(A.Houses.size() == Next.Houses.size());
	VT_CHECK(std::memcmp(A.Houses.data(), Next.Houses.data(), A.Houses.size() * sizeof(Placed)) == 0);
}

VAELEN_TEST(Layout, AHouseDoesNotMoveWhenALaterFamilyDiesOut)
{
	const Views& V = Taken();
	// The highest family of the detailed region with the most families.
	std::map<uint32, std::set<uint32>> Families;
	std::set<uint32> Detailed;
	for (const View::RegionView& R : V.World_.Regions)
	{
		if (R.Detailed != 0u)
		{
			Detailed.insert(R.Index);
		}
	}
	for (const View::PersonView& P : V.People.People)
	{
		if (View::IsAlive(P) && P.Family != 0u && Detailed.count(P.Region) != 0u)
		{
			Families[P.Region].insert(P.Family);
		}
	}
	uint32 Region = 0, Last = 0;
	usize Most = 0;
	for (const auto& [R, Set] : Families)
	{
		if (Set.size() > Most)
		{
			Most = Set.size();
			Region = R;
			Last = *Set.rbegin();
		}
	}
	VT_REQUIRE(Most >= 3u);
	View::PeopleView Fewer = V.People;
	for (View::PersonView& P : Fewer.People)
	{
		if (P.Family == Last)
		{
			P.State = 1; // no longer alive
		}
	}
	SceneLayout Before, After;
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, Before);
	BuildLayout(V.G, V.World_, V.Net, Fewer, V.Life, 100, After);
	VT_CHECK_EQ(After.Houses.size() + 1u, Before.Houses.size());
	uint32 Kept = 0;
	for (const Placed& H : After.Houses)
	{
		for (const Placed& B : Before.Houses)
		{
			if (B.Region == H.Region && B.Key == H.Key && B.Flags == H.Flags)
			{
				VT_CHECK(B.X == H.X && B.Y == H.Y);
				Kept += B.X == H.X && B.Y == H.Y ? 1u : 0u;
				break;
			}
		}
	}
	VT_CHECK_EQ(Kept, static_cast<uint32>(After.Houses.size()));
	(void)Region;
}

VAELEN_TEST(Layout, WithNobodyThereAreNoHousesOfFamiliesAndNoFigures)
{
	// CONTROL: an empty people view gives no family house and no figure; the
	// coarse regions' houses, the squares and the roads are the world's, not the people's.
	const Views& V = Taken();
	SceneLayout L;
	BuildLayout(V.G, V.World_, V.Net, View::PeopleView{}, V.Life, 100, L);
	VT_CHECK_EQ(static_cast<uint32>(L.Figures.size()), 0u);
	// The floor (ADR-0149 rule 1): the coarse houses, the squares and the
	// roads are there, as many as with the people.
	SceneLayout Full;
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, Full);
	VT_CHECK(L.Houses.size() > 100u);
	VT_CHECK_EQ(L.Squares.size(), Full.Squares.size());
	VT_CHECK_EQ(L.Roads.size(), Full.Roads.size());
	for (const Placed& H : L.Houses)
	{
		VT_CHECK_EQ(H.Flags, 1u);
	}
	// And a world with no settlement has no square.
	View::WorldView NoTowns = V.World_;
	for (View::RegionView& R : NoTowns.Regions)
	{
		R.Settlement = 0;
	}
	SceneLayout Bare;
	BuildLayout(V.G, NoTowns, V.Net, V.People, V.Life, 100, Bare);
	VT_CHECK_EQ(static_cast<uint32>(Bare.Squares.size()), 0u);
}

VAELEN_TEST(Layout, AimPicksTheNearerCompanionInFrontAndNobodyElse)
{
	SceneLayout L;
	const auto Figure = [&](int32 X, int32 Y, uint32 Person, bool Company)
	{
		Placed F;
		F.X = X;
		F.Y = Y;
		F.Key = Person;
		F.Flags = Company ? FigureFlag::Company : 0u;
		L.Figures.push_back(F);
	};
	Figure(200, 0, 7, true);  // ahead, 2 m
	Figure(120, 10, 9, true); // ahead, nearer
	Figure(-100, 0, 3, true); // behind
	Figure(100, 0, 4, false); // ahead, nearest, not company
	Figure(0, 250, 5, true);  // 90 deg off
	Figure(400, 0, 6, true);  // too far
	VT_CHECK_EQ(AimAt(L, 0, 0, 1000, 0), 9u);
	VT_CHECK_EQ(AimAt(L, 0, 0, -1000, 0), 3u);
	VT_CHECK_EQ(AimAt(L, 0, 0, 0, 1000), 5u);
	VT_CHECK_EQ(AimAt(L, 0, 0, 0, -1000), 0u);
	VT_CHECK_EQ(AimAt(L, 0, 0, 0, 0), 0u);

	// The cone's edge, the reach's edge and the tie (the review of
	// 2026-09-27: the figures above sit at 0, 5, 90 and 180 degrees, so a
	// cone of 60 degrees answered every one of them the same).
	const auto One = [](int32 X, int32 Y, uint32 Person)
	{
		SceneLayout Out;
		Placed F;
		F.X = X;
		F.Y = Y;
		F.Key = Person;
		F.Flags = FigureFlag::Company;
		Out.Figures.push_back(F);
		return Out;
	};
	VT_CHECK_EQ(AimAt(One(100, 100, 11), 0, 0, 1000, 0), 0u); // 45 deg: outside the 30
	VT_CHECK_EQ(AimAt(One(100, 50, 12), 0, 0, 1000, 0), 12u); // 26.6 deg: inside
	VT_CHECK_EQ(AimAt(One(100, 57, 15), 0, 0, 1000, 0), 15u); // 29.7 deg: cos^2 = 10000/13249 > 3/4, inside
	VT_CHECK_EQ(AimAt(One(100, 58, 16), 0, 0, 1000, 0), 0u);  // 30.1 deg: cos^2 = 10000/13364 < 3/4, outside
	VT_CHECK_EQ(AimAt(One(300, 0, 13), 0, 0, 1000, 0), 13u);  // at reach exactly
	VT_CHECK_EQ(AimAt(One(301, 0, 14), 0, 0, 1000, 0), 0u);	  // a centimetre past it
	SceneLayout Tie = One(200, 50, 22);
	Tie.Figures.push_back(One(200, -50, 21).Figures[0]);
	VT_CHECK_EQ(AimAt(Tie, 0, 0, 1000, 0), 21u); // equidistant: the lower index, whichever came first
	SceneLayout TieBack = One(200, -50, 21);
	TieBack.Figures.push_back(One(200, 50, 22).Figures[0]);
	VT_CHECK_EQ(AimAt(TieBack, 0, 0, 1000, 0), 21u);
}

VAELEN_TEST(Layout, EveryHouseStandsOnGroundLevelWithinTwentyDegrees)
{
	// The slope rule (Layout.h: at most HouseSlopeCm across the house's 8 m
	// on either axis), asserted for every house through HeightAt - which
	// Terrain.HeightAtIsTheMeshsOwnSurface holds on its own - on the real
	// ground and on one four times steeper (the review of 2026-09-27: no
	// case asserted the rule, and a Level that always said yes passed them all).
	const Views& V = Taken();
	SceneScale Tight;
	Tight.CmPerTile = 6250; // 62.5 m a tile: ratio 1/250, 812 unwalkable pairs at 128
	Tight.Steps = 2;
	Ground Steep;
	VT_REQUIRE(BuildGround(Map128Of(V), Tight, Steep));
	const Ground* Both[2] = {&V.G, &Steep};
	for (const Ground* G : Both)
	{
		SceneLayout L;
		BuildLayout(*G, V.World_, V.Net, V.People, V.Life, 100, L);
		VT_REQUIRE(L.Houses.size() > 100u);
		int64 Steepest = 0;
		const auto Abs = [](int64 A) { return A < 0 ? -A : A; };
		for (const Placed& H : L.Houses)
		{
			const int64 DX = int64{HeightAt(*G, H.X + HouseHalfCm, H.Y)} - HeightAt(*G, H.X - HouseHalfCm, H.Y);
			const int64 DY = int64{HeightAt(*G, H.X, H.Y + HouseHalfCm)} - HeightAt(*G, H.X, H.Y - HouseHalfCm);
			// And the two diagonals, 8 * sqrt 2 m long: 20 degrees is 291 * sqrt 2, as squares.
			const int64 D1 = int64{HeightAt(*G, H.X + HouseHalfCm, H.Y + HouseHalfCm)} -
							 HeightAt(*G, H.X - HouseHalfCm, H.Y - HouseHalfCm);
			const int64 D2 = int64{HeightAt(*G, H.X + HouseHalfCm, H.Y - HouseHalfCm)} -
							 HeightAt(*G, H.X - HouseHalfCm, H.Y + HouseHalfCm);
			const int64 Worst = Abs(DX) > Abs(DY) ? Abs(DX) : Abs(DY);
			VT_CHECK_MSG(Worst <= HouseSlopeCm, "the house of %u in region %u stands on %lld cm across 8 m", H.Key,
						 H.Region, static_cast<long long>(Worst));
			VT_CHECK_MSG(D1 * D1 <= 2 * int64{HouseSlopeCm} * HouseSlopeCm &&
							 D2 * D2 <= 2 * int64{HouseSlopeCm} * HouseSlopeCm,
						 "the house of %u in region %u stands on %lld and %lld cm across its diagonals", H.Key,
						 H.Region, static_cast<long long>(D1), static_cast<long long>(D2));
			Steepest = Worst > Steepest ? Worst : Steepest;
		}
		// The rule had something to refuse: of the plots a house could have
		// taken (the land tiles' centres of the detailed regions), some are
		// too steep - or the check above holds for want of a slope.
		uint32 Plots = 0, TooSteep = 0;
		for (const View::RegionView& R : V.World_.Regions)
		{
			for (uint32 T = 0; R.Detailed != 0u && T < G->Width * G->Height; ++T)
			{
				if (G->Region[T] != R.Index || G->Kind[T] != GroundKind::Land)
				{
					continue;
				}
				int64 X = 0, Y = 0;
				PointOfTile(*G, T, X, Y);
				const int64 DX = int64{HeightAt(*G, X + HouseHalfCm, Y)} - HeightAt(*G, X - HouseHalfCm, Y);
				const int64 DY = int64{HeightAt(*G, X, Y + HouseHalfCm)} - HeightAt(*G, X, Y - HouseHalfCm);
				++Plots;
				TooSteep += (DX < 0 ? -DX : DX) > HouseSlopeCm || (DY < 0 ? -DY : DY) > HouseSlopeCm ? 1u : 0u;
			}
		}
		VT_CHECK(Plots > 100u);
		// On the real ground no plot of these is too steep (measured: the
		// rule refuses nothing at 128 after 70 years); on the ground four
		// times steeper it must have had plots to refuse, or the check
		// above holds there for want of a slope.
		if (G == &Steep)
		{
			VT_CHECK_MSG(TooSteep > 0u, "no plot of %u is too steep: the rule was never asked", Plots);
		}
		VAELEN_LOG_INFO(LogSceneLayout,
						"%u cm a tile: %zu houses, steepest %lld cm across 8 m; %u of %u plots too steep",
						static_cast<unsigned>(G->Scale.CmPerTile), L.Houses.size(), static_cast<long long>(Steepest),
						TooSteep, Plots);
	}
}

VAELEN_TEST(Layout, ARoadJoinsItsRegionsMiddlesByAShortestLandPath)
{
	// Each road: from a land tile of its route's From region to one of its
	// To region, as long as the shortest four-connected walk over land and
	// river between those two tiles - a plain breadth-first search is the
	// second instrument - and every open route is a road or counted
	// unrouted (the review of 2026-09-27: the case above checked each step
	// was a neighbour, which a road of one tile satisfies).
	const Views& V = Taken();
	SceneLayout L;
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, L);
	const LayoutStats S = MeasureLayout(L);
	VT_REQUIRE(S.Roads > 10u);
	VT_CHECK_EQ(S.Roads + L.Unrouted, V.Net.Open);
	const uint32 N = V.G.Width * V.G.Height;
	const auto Passable = [&](uint32 T) { return V.G.Kind[T] == GroundKind::Land || V.G.Kind[T] == GroundKind::River; };
	std::vector<uint32> Dist(N);
	uint32 Longest = 0;
	for (const RoadPath& R : L.Roads)
	{
		const View::RouteView* Route = View::RouteOf(V.Net, R.Route);
		VT_REQUIRE(Route != nullptr && Route->Open == 1u && !R.Tiles.empty());
		VT_CHECK_EQ(V.G.Region[R.Tiles.front()], Route->From);
		VT_CHECK_EQ(V.G.Region[R.Tiles.back()], Route->To);
		VT_CHECK(V.G.Kind[R.Tiles.front()] == GroundKind::Land && V.G.Kind[R.Tiles.back()] == GroundKind::Land);
		// Breadth first from the road's first tile: the road is no longer than the shortest walk.
		std::fill(Dist.begin(), Dist.end(), 0xFFFFFFFFu);
		std::queue<uint32> Next;
		Dist[R.Tiles.front()] = 0;
		Next.push(R.Tiles.front());
		while (!Next.empty() && Dist[R.Tiles.back()] == 0xFFFFFFFFu)
		{
			const uint32 T = Next.front();
			Next.pop();
			const uint32 X = T % V.G.Width;
			const uint32 Y = T / V.G.Width;
			const uint32 Around[4] = {X + 1u < V.G.Width ? T + 1u : T, X > 0u ? T - 1u : T,
									  Y + 1u < V.G.Height ? T + V.G.Width : T, Y > 0u ? T - V.G.Width : T};
			for (const uint32 A : Around)
			{
				if (A != T && Passable(A) && Dist[A] == 0xFFFFFFFFu)
				{
					Dist[A] = Dist[T] + 1u;
					Next.push(A);
				}
			}
		}
		VT_CHECK_EQ(static_cast<uint32>(R.Tiles.size()) - 1u, Dist[R.Tiles.back()]);
		Longest = R.Tiles.size() > Longest ? static_cast<uint32>(R.Tiles.size()) : Longest;
	}
	VT_CHECK(Longest > 2u);

	// A colony's pit (Layout.cpp's one branch no pinned world runs): one
	// pit, on the colony's region, at the region's middle - the very tile
	// its square stands on.
	VT_REQUIRE(!L.Squares.empty());
	const Placed& Square = L.Squares[0];
	View::NetView Mined = V.Net;
	View::ColonyView Colony;
	Colony.Region = Square.Region;
	Colony.Hands = 3;
	Mined.Colonies.push_back(Colony);
	SceneLayout Dug;
	BuildLayout(V.G, V.World_, Mined, V.People, V.Life, 100, Dug);
	VT_REQUIRE(Dug.Pits.size() == 1u);
	VT_CHECK_EQ(Dug.Pits[0].Region, Square.Region);
	VT_CHECK_EQ(Dug.Pits[0].Key, Square.Region);
	VT_CHECK(Dug.Pits[0].X == Square.X && Dug.Pits[0].Y == Square.Y && Dug.Pits[0].Z == Square.Z);
	VT_CHECK(OnLandOf(V.G, Dug.Pits[0]));
	VT_CHECK_EQ(MeasureLayout(Dug).Pits, 1u);
	VT_CHECK(MeasureLayout(Dug).Digest != S.Digest);
}

VAELEN_TEST(Layout, ThePlayedPersonHasNoFigureAndTheCompanyIsFlagged)
{
	// A life played from a detailed region: its person stands nowhere (the
	// walker is them), the two it lists carry the Company flag and nobody
	// else does, and the count MeasureLayout reports is two - through the
	// life's Person and through the world's Played alike (the review of
	// 2026-09-27: every case passed an empty life, so Layout.cpp's played
	// and company branches ran under no unit check).
	const Views& V = Taken();
	uint32 Region = 0;
	std::vector<uint32> Three;
	for (const View::RegionView& R : V.World_.Regions)
	{
		if (R.Detailed == 0u)
		{
			continue;
		}
		Three.clear();
		for (const View::PersonView& P : V.People.People)
		{
			if (View::IsAlive(P) && P.Region == R.Index && Three.size() < 3u)
			{
				Three.push_back(P.Index);
			}
		}
		if (Three.size() == 3u)
		{
			Region = R.Index;
			break;
		}
	}
	VT_REQUIRE(Region != 0u);
	View::LifeView Life = V.Life;
	Life.Person = Three[0];
	Life.Region = Region;
	Life.CompanyCount = 2;
	Life.Company[0].Person = Three[1];
	Life.Company[1].Person = Three[2];
	SceneLayout Base, Lived, Played;
	BuildLayout(V.G, V.World_, V.Net, V.People, V.Life, 100, Base);
	BuildLayout(V.G, V.World_, V.Net, V.People, Life, 100, Lived);
	View::WorldView Theirs = V.World_;
	Theirs.Played = Three[0];
	BuildLayout(V.G, Theirs, V.Net, V.People, V.Life, 100, Played);
	VT_CHECK_EQ(Lived.Figures.size() + 1u, Base.Figures.size());
	VT_CHECK_EQ(Played.Figures.size(), Lived.Figures.size());
	std::set<uint32> Flagged;
	for (const Placed& F : Lived.Figures)
	{
		VT_CHECK(F.Key != Three[0]);
		if ((F.Flags & FigureFlag::Company) != 0u)
		{
			Flagged.insert(F.Key);
		}
	}
	for (const Placed& F : Played.Figures)
	{
		VT_CHECK(F.Key != Three[0]);
		VT_CHECK_EQ(F.Flags & FigureFlag::Company, 0u);
	}
	VT_CHECK_EQ(Flagged.size(), 2u);
	VT_CHECK(Flagged.count(Three[1]) == 1u && Flagged.count(Three[2]) == 1u);
	VT_CHECK_EQ(MeasureLayout(Lived).Company, 2u);
	VT_CHECK_EQ(MeasureLayout(Base).Company, 0u);
	VT_CHECK_EQ(MeasureLayout(Played).Company, 0u);
	// The other figures stand where they stood: a life changes nothing but its own.
	uint32 Same = 0;
	for (const Placed& F : Lived.Figures)
	{
		for (const Placed& B : Base.Figures)
		{
			if (B.Key == F.Key)
			{
				Same += B.X == F.X && B.Y == F.Y ? 1u : 0u;
				break;
			}
		}
	}
	VT_CHECK_EQ(Same, static_cast<uint32>(Lived.Figures.size()));
}
