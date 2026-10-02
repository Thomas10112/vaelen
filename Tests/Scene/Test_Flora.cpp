// VAELEN - Phase 23 task 23.02: forests, held to their rules on a real world.
//
// Trees are invented (Flora.h says how). These cases hold the invention to
// the ground and the layout: every tree on a land tile of a biome that grows
// it, in the region or in the chunks the region touches, never on a road, a
// house, a square or a pit, on the ground's own height; the counts of the
// rules; the same bytes twice; a digest pinned; and the cap, which thins
// every tile by the same ratio and says what it dropped.
#include "Vaelen/Scene/Flora.h"
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
	VAELEN_DEFINE_LOG_CATEGORY(LogSceneFlora);

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

	/// Region 26: where the played life of the 128/120 world stands (Run.Bound).
	constexpr uint32 PlayedRegion = 26;

	std::set<uint32> TakenTiles(const Views& V)
	{
		std::set<uint32> Out;
		const auto Under = [&](const std::vector<Placed>& List)
		{
			for (const Placed& P : List)
			{
				uint32 T = 0;
				if (TileOfPoint(V.G, P.X, P.Y, T))
				{
					Out.insert(T);
				}
			}
		};
		Under(V.Laid.Houses);
		Under(V.Laid.Squares);
		Under(V.Laid.Pits);
		for (const RoadPath& R : V.Laid.Roads)
		{
			for (const uint32 T : R.Tiles)
			{
				Out.insert(T);
			}
		}
		return Out;
	}
} // namespace

VAELEN_TEST(Flora, EveryTreeStandsWhereTheRulesSay)
{
	const Views& V = Taken();
	VT_REQUIRE(V.G.Width == 128u);
	Flora F;
	PlantTrees(V.G, V.Laid, PlayedRegion, FloraRules{}, F);
	const FloraStats S = MeasureFlora(F);
	char Line[FloraLineBytes];
	VT_CHECK(FloraLine(128, 0x41454c564f52ull, PlayedRegion, S, Line, FloraLineBytes) != 0u);
	VAELEN_LOG_INFO(LogSceneFlora, "%s", Line);
	VT_CHECK_EQ(S.Trees, 1266u);
	VT_CHECK(S.Trees <= FloraRules{}.Cap);
	const std::set<uint32> Excluded = TakenTiles(V);
	const FloraRules Rules;
	// Every tree: on land, of a biome that grows it, in a chunk the region
	// touches, on no excluded tile, at the ground's height, of its biome's
	// kind, ring-flagged exactly when its tile is another region's.
	std::vector<uint32> PerTile(static_cast<usize>(V.G.Width) * V.G.Height, 0u);
	for (const Tree& T : F.Trees)
	{
		uint32 Tile = 0;
		VT_REQUIRE(TileOfPoint(V.G, T.X, T.Y, Tile));
		VT_CHECK_EQ(V.G.Kind[Tile], GroundKind::Land);
		VT_CHECK(Rules.PerTile[V.G.Biome[Tile]] > 0u);
		VT_CHECK(ChunkHolds(V.G, (Tile % V.G.Width) / ChunkTiles, (Tile / V.G.Width) / ChunkTiles, PlayedRegion));
		VT_CHECK(Excluded.count(Tile) == 0u);
		VT_CHECK_EQ(T.Z, HeightAt(V.G, T.X, T.Y));
		VT_CHECK_EQ(T.Ring, V.G.Region[Tile] == PlayedRegion ? 0u : 1u);
		VT_CHECK(T.HeightCm >= 150u && T.HeightCm <= 2100u);
		++PerTile[Tile];
	}
	// The counts are the rules': a region tile its biome's, a ring tile a
	// quarter of it - when nothing was dropped.
	if (S.Dropped == 0u)
	{
		for (uint32 Tile = 0; Tile < PerTile.size(); ++Tile)
		{
			if (PerTile[Tile] == 0u)
			{
				continue;
			}
			const uint32 Full = Rules.PerTile[V.G.Biome[Tile]];
			VT_CHECK_EQ(PerTile[Tile], V.G.Region[Tile] == PlayedRegion ? Full : Full / Rules.RingDivisor);
		}
	}
	// The same bytes twice.
	Flora Again;
	PlantTrees(V.G, V.Laid, PlayedRegion, FloraRules{}, Again);
	VT_CHECK_DIGEST_EQ(MeasureFlora(Again).Digest, S.Digest);
	// PINNED: the wood of region 26 on AELVOR 128 after 60 + 10 years, day
	// 100 - the same line as Atlas.SceneFlora128's. A tree moved one
	// centimetre moves it (shown on the debug build, 2026-10-02).
	VT_CHECK_DIGEST_EQ(S.Digest, 0x982297351bd5dfb9ull);
	// CONTROL: another region is another wood.
	Flora Other;
	PlantTrees(V.G, V.Laid, PlayedRegion + 1u, FloraRules{}, Other);
	VT_CHECK(MeasureFlora(Other).Digest != S.Digest);
	// CONTROL: region 0 is nobody's and grows nothing.
	Flora Nobody;
	PlantTrees(V.G, V.Laid, 0u, FloraRules{}, Nobody);
	VT_CHECK_EQ(MeasureFlora(Nobody).Trees, 0u);
}

VAELEN_TEST(Flora, TheCapThinsEveryTileAlikeAndSaysWhatItDropped)
{
	const Views& V = Taken();
	VT_REQUIRE(V.G.Width == 128u);
	FloraRules Free;
	Free.Cap = 0xFFFFFFFFu;
	Flora Whole;
	PlantTrees(V.G, V.Laid, PlayedRegion, Free, Whole);
	const uint32 All = MeasureFlora(Whole).Trees;
	VT_CHECK(All > 100u);
	FloraRules Half = Free;
	Half.Cap = All / 2u;
	Flora Thinned;
	PlantTrees(V.G, V.Laid, PlayedRegion, Half, Thinned);
	const FloraStats S = MeasureFlora(Thinned);
	VT_CHECK(S.Trees <= Half.Cap);
	// Integer scaling drops at most one tree per tile beyond the ratio.
	VT_CHECK(S.Trees + S.Dropped == All);
	VT_CHECK(S.Trees * 2u + 2u * 1024u >= All);
	// Every tile keeps a share: no planted tile of four or more trees is bare.
	std::vector<uint32> Before(static_cast<usize>(V.G.Width) * V.G.Height, 0u);
	std::vector<uint32> After = Before;
	for (const Tree& T : Whole.Trees)
	{
		uint32 Tile = 0;
		VT_REQUIRE(TileOfPoint(V.G, T.X, T.Y, Tile));
		++Before[Tile];
	}
	for (const Tree& T : Thinned.Trees)
	{
		uint32 Tile = 0;
		VT_REQUIRE(TileOfPoint(V.G, T.X, T.Y, Tile));
		++After[Tile];
	}
	for (uint32 Tile = 0; Tile < Before.size(); ++Tile)
	{
		VT_CHECK(After[Tile] <= Before[Tile]);
		VT_CHECK(Before[Tile] < 4u || After[Tile] > 0u);
	}
	// The thinned wood's first trees are the whole wood's first trees: a
	// tree never moves when the cap changes, it is dropped or kept.
	std::set<uint64> Spots;
	for (const Tree& T : Whole.Trees)
	{
		Spots.insert((static_cast<uint64>(static_cast<uint32>(T.X)) << 32) | static_cast<uint32>(T.Y));
	}
	for (const Tree& T : Thinned.Trees)
	{
		VT_CHECK(Spots.count((static_cast<uint64>(static_cast<uint32>(T.X)) << 32) | static_cast<uint32>(T.Y)) != 0u);
	}
	// CONTROL: a cap the wood fits under drops nothing and is the same wood.
	FloraRules Roomy = Free;
	Roomy.Cap = All;
	Flora Same;
	PlantTrees(V.G, V.Laid, PlayedRegion, Roomy, Same);
	VT_CHECK_EQ(MeasureFlora(Same).Dropped, 0u);
	VT_CHECK_DIGEST_EQ(MeasureFlora(Same).Digest, MeasureFlora(Whole).Digest);
}
