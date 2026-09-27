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
			return Out;
		}();
		return V;
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
}
