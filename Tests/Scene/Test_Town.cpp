// VAELEN - Phase 23 task 23.05: roofs that face their street, walls of their
// family's culture - held to the layout and the people on a real world.
#include "Vaelen/Scene/Town.h"
#include "Vaelen/Scene/Layout.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/View/Take.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <set>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Scene;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogSceneTown);

	struct Views
	{
		Ground G;
		View::WorldView World_;
		View::NetView Net;
		View::PeopleView People;
		View::LifeView Life;
		SceneLayout Laid;
	};

	/// AELVOR 128 after sixty years of pre-history and ten of history, laid out
	/// on day 100 - the world and the day of Atlas.SceneLayout128.
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
			BuildLayout(Out.G, Out.World_, Out.Net, Out.People, Out.Life, 100, Out.Laid);
			return Out;
		}();
		return V;
	}

	/// The road tiles and their axes, counted the slow way: for every road
	/// tile, the direction to the next tile of ITS path.
	void RoadsOf(const Views& V, std::set<uint32>& Tiles, std::vector<uint32>& AxisOf)
	{
		AxisOf.assign(static_cast<usize>(V.G.Width) * V.G.Height, 9u);
		for (const RoadPath& R : V.Laid.Roads)
		{
			for (usize I = 0; I < R.Tiles.size(); ++I)
			{
				const uint32 T = R.Tiles[I];
				if (Tiles.count(T) != 0u)
				{
					continue;
				}
				Tiles.insert(T);
				const uint32 Next = I + 1u < R.Tiles.size() ? R.Tiles[I + 1u] : (I > 0u ? R.Tiles[I - 1u] : T);
				const int64 DX = int64{Next % V.G.Width} - int64{T % V.G.Width};
				const int64 DY = int64{Next / V.G.Width} - int64{T / V.G.Width};
				AxisOf[T] = (DY < 0 ? -DY : DY) > (DX < 0 ? -DX : DX) ? 1u : 0u;
			}
		}
	}
} // namespace

VAELEN_TEST(Town, TheRidgeFacesTheStreetAndTheWallsAreTheFamilys)
{
	const Views& V = Taken();
	VT_REQUIRE(V.G.Width == 128u);
	TownLook T;
	LookOfTown(V.G, V.Laid, V.People, T);
	VT_CHECK_EQ(T.Houses.size(), V.Laid.Houses.size());
	const TownStats S = MeasureTown(T);
	char Line[TownLineBytes];
	VT_CHECK(TownLine(128, 0x41454c564f52ull, 100, S, Line, TownLineBytes) != 0u);
	VAELEN_LOG_INFO(LogSceneTown, "%s", Line);
	VT_CHECK(S.Houses > 100u && S.ByRoad > 0u && S.ByRoad < S.Houses && S.Cultured > 0u);
	VT_CHECK_EQ(S.Houses, 565u);
	VT_CHECK_EQ(S.ByRoad, 324u);
	std::set<uint32> RoadTiles;
	std::vector<uint32> AxisOf;
	RoadsOf(V, RoadTiles, AxisOf);
	for (usize I = 0; I < T.Houses.size(); ++I)
	{
		const Placed& H = V.Laid.Houses[I];
		const HouseLook& L = T.Houses[I];
		// The one-house function and the whole-town one agree.
		bool bByRoad = false;
		VT_CHECK_EQ(RidgeOf(V.G, V.Laid, H, bByRoad), static_cast<uint32>(L.RidgeAlongY));
		VT_CHECK_EQ(bByRoad ? 1u : 0u, static_cast<uint32>(L.ByRoad));
		VT_CHECK_EQ(CultureOfHouse(V.People, H), static_cast<uint32>(L.Culture));
		// By a road: the nearest road tile within reach, ties on the lower
		// index, and the ridge is that road's axis. By the hash: no road tile
		// within reach at all.
		uint32 Tile = 0;
		VT_REQUIRE(TileOfPoint(V.G, H.X, H.Y, Tile));
		const int64 HX = Tile % V.G.Width, HY = Tile / V.G.Width;
		int64 Best = -1;
		uint32 BestTile = 0;
		for (const uint32 R : RoadTiles)
		{
			const int64 RX = R % V.G.Width, RY = R / V.G.Width;
			if (RX < HX - 2 || RX > HX + 2 || RY < HY - 2 || RY > HY + 2)
			{
				continue;
			}
			const int64 D = (RX - HX) * (RX - HX) + (RY - HY) * (RY - HY);
			if (Best < 0 || D < Best || (D == Best && R < BestTile))
			{
				Best = D;
				BestTile = R;
			}
		}
		VT_CHECK_EQ(static_cast<uint32>(L.ByRoad), Best >= 0 ? 1u : 0u);
		if (Best >= 0)
		{
			VT_CHECK_EQ(static_cast<uint32>(L.RidgeAlongY), AxisOf[BestTile]);
		}
		// The culture: the lowest-indexed living member of the family, found
		// independently; a coarse house (Flags 1) has none.
		uint32 Expected = 0;
		if (H.Flags == 0u)
		{
			for (const View::PersonView& P : V.People.People)
			{
				if (P.Family == H.Key && View::IsAlive(P))
				{
					Expected = P.Culture;
					break;
				}
			}
		}
		VT_CHECK_EQ(static_cast<uint32>(L.Culture), Expected);
	}
	// The same bytes twice.
	TownLook Again;
	LookOfTown(V.G, V.Laid, V.People, Again);
	VT_CHECK_DIGEST_EQ(MeasureTown(Again).Digest, S.Digest);
	// PINNED: the town of AELVOR 128 after 60 + 10 years, day 100 - the same
	// line as Atlas.SceneTown128's.
	VT_CHECK_DIGEST_EQ(S.Digest, 0x5bff6255ceba5f21ull);
	// CONTROL: with no roads every ridge is the hash's, and the hashed
	// ridges of the houses that had no road are unchanged - a road sets a
	// ridge and never moves another.
	SceneLayout Roadless = V.Laid;
	Roadless.Roads.clear();
	TownLook Lone;
	LookOfTown(V.G, Roadless, V.People, Lone);
	VT_CHECK_EQ(MeasureTown(Lone).ByRoad, 0u);
	for (usize I = 0; I < T.Houses.size(); ++I)
	{
		if (T.Houses[I].ByRoad == 0u)
		{
			VT_CHECK_EQ(Lone.Houses[I].RidgeAlongY, T.Houses[I].RidgeAlongY);
		}
	}
	// CONTROL: nobody alive, no culture on any wall; the ridges as they were.
	View::PeopleView Nobody;
	TownLook Empty;
	LookOfTown(V.G, V.Laid, Nobody, Empty);
	VT_CHECK_EQ(MeasureTown(Empty).Cultured, 0u);
	VT_CHECK_EQ(MeasureTown(Empty).AlongY, S.AlongY);
}
