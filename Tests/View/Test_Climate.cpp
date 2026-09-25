// VAELEN - Tests/View
// Phase 18 task 18.04: the climate as the screen needs it.
//
// The leaf (Climate.h) is four bytes a tile with no way back into the world;
// the frame carries the season and a packed climate word per region; the life
// carries the day's degrees where the played person stands. All of it ONLY
// when the sources say there is a climate: a take with HasClimate false is
// byte for byte the take of the world before Phase 18, and this file holds
// that as hard as it holds the new figures.
//
// STATUS: PROTOTYPE (Phase 18 task 18.04)
#include "Vaelen/View/Climate.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Panel.h"
#include "Vaelen/View/Take.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Climate.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;
using namespace Vaelen::View;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogClimateView);

	Options Small()
	{
		Options O;
		O.Size = 64;
		O.PreHistory = 60;
		O.Years = 10;
		O.Play = true;
		// 18.10: the climate is the default now; these cases are about what the
		// VIEW does with the flag, so the world is the one before Phase 18 and
		// Warm() tells the view the climate by hand, as it did before the flip.
		O.Climate = false;
		return O;
	}

	ViewSources Cold(const Aelvor& A)
	{
		ViewSources S = A.Sources();
		S.HasClimate = false; // explicit: a flipped default must not reach this control
		return S;
	}

	ViewSources Warm(const Aelvor& A)
	{
		ViewSources S = A.Sources();
		S.HasClimate = true;
		return S;
	}

	std::string RowOf(const PanelView& V, RowKind Kind)
	{
		for (uint32 i = 0; i < V.RowCount && i < PanelRows; ++i)
		{
			if (V.Rows[i].Kind == static_cast<uint32>(Kind))
			{
				return std::string(V.Text + V.Rows[i].Begin, V.Rows[i].Length);
			}
		}
		return std::string();
	}
} // namespace

VAELEN_TEST(Climate, TheLeafIsFourBytesATileAndReadsTheKernel)
{
	static_assert(sizeof(TileClimate) == 4, "four bytes a tile");
	static_assert(sizeof(ClimateView) == sizeof(uint64) + 8 * sizeof(uint32) + sizeof(std::vector<TileClimate>),
				  "the header is padding free");

	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const World& W = A.Instance();
	const ViewSources From = Warm(A);

	ClimateView V;
	TakeClimateView(W, From, V);
	const uint32 Tiles = W.Map().Grid().Width * W.Map().Grid().Height;
	VT_CHECK_EQ(V.Width, W.Map().Grid().Width);
	VT_CHECK_EQ(V.Height, W.Map().Grid().Height);
	VT_CHECK_EQ(static_cast<uint32>(V.Tiles.size()), Tiles);
	VT_CHECK_MSG(V.Season >= 1u && V.Season <= 4u, "season %u", V.Season);
	VT_CHECK_EQ(V.Day, static_cast<uint32>((W.Now() % History::TicksPerYear) / 24u));

	// TWO INSTRUMENTS: the leaf's frost count equals the count of tiles the
	// kernel's own TileTemperatureOn puts below freezing today, and every
	// tile's Now is the kernel's, floored, byte for byte.
	const WorldGen::WorldLayers& Layers = From.Types.World.Layers;
	uint32 Frost = 0, Growing = 0, Same = 0, Differ = 0, Flagged = 0;
	for (uint32 Tile = 0; Tile < Tiles; ++Tile)
	{
		const Fix64 Now = WorldGen::TileTemperatureOn(W.Map(), Layers, Tile, V.Day);
		Frost += Now < Fix64::Zero() ? 1u : 0u;
		Growing += Now >= Fix64::FromInt(5) ? 1u : 0u;
		const TileClimate& T = V.Tiles[Tile];
		Same += T.Now == Now.FloorToInt() ? 1u : 0u;
		Differ += T.Winter != T.Summer ? 1u : 0u;
		Flagged += ((T.Flags & ClimateFlag::Frost) != 0u) == (Now < Fix64::Zero()) ? 1u : 0u;
	}
	const ClimateViewStats S = MeasureClimateView(V);
	VT_CHECK_EQ(S.Tiles, Tiles);
	VT_CHECK_MSG(S.Frost == Frost, "the leaf counts %u frost tiles, the kernel %u", S.Frost, Frost);
	VT_CHECK_MSG(S.Growing == Growing, "the leaf counts %u growing tiles, the kernel %u", S.Growing, Growing);
	VT_CHECK_EQ(Same, Tiles);
	VT_CHECK_EQ(Flagged, Tiles);
	VT_CHECK_MSG(Differ == Tiles, "mid-winter and mid-summer differ on %u of %u tiles", Differ, Tiles);
	VT_CHECK_MSG(S.Frost + S.Growing <= Tiles, "a tile cannot both freeze and grow");
	VT_CHECK_MSG(S.Coldest >= -70 && S.Coldest < S.Warmest, "coldest %d warmest %d", S.Coldest, S.Warmest);
	VT_CHECK_EQ(S.Bytes, static_cast<uint32>(sizeof(ClimateView) + Tiles * sizeof(TileClimate)));
	VT_CHECK(S.Digest != 0 && S.Digest != HashConstants::Fnv1a64Offset);
	VT_CHECK(TileClimateIn(V, 0, 0) == &V.Tiles[0]);
	VT_CHECK(TileClimateIn(V, V.Width, 0) == nullptr);
	VT_CHECK(TileClimateIn(V, 0, V.Height) == nullptr);
	VAELEN_LOG_INFO(LogClimateView, "climate: %u tiles, %u frost, %u growing, coldest %d warmest %d, digest %016llx",
					S.Tiles, S.Frost, S.Growing, S.Coldest, S.Warmest, static_cast<unsigned long long>(S.Digest));

	// The leaf outlives the world: a copy measures the same after the run is
	// gone, which is the claim of every view (ADR-0104).
	ClimateView Kept = V;
	{
		std::unique_ptr<Aelvor> Gone = std::make_unique<Aelvor>(Small());
		VT_REQUIRE(Gone->Begin());
		TakeClimateView(Gone->Instance(), Warm(*Gone), Kept);
	}
	VT_CHECK_EQ(MeasureClimateView(Kept).Digest, S.Digest);
}

VAELEN_TEST(Climate, WithoutAClimateEveryViewIsTheWorldBeforePhase18)
{
	// THE CONTROL, and the one that matters most: with HasClimate false the
	// frame's season is 0, every region's climate word is 0, the life's four
	// words are 0, the page has no weather row and its body row no chill -
	// and the digests of frame, life and page equal a take with the FLAG
	// OFF taken twice, under VT_CHECK_DIGEST_EQ so that a zero cannot pass.
	// The frozen pages of Test_Panel, Atlas.PanelFrozen, Atlas.Frozen128 and
	// Test_Aelvor hold the same thing against their old literals.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	// Somebody MUST be played, or "deg here" has nowhere to be: the first
	// version of this took whoever the default rules offered, which at 60+10
	// years is nobody, and the row rightly said only the season.
	Player::StartRules Anyone;
	Anyone.WantBound = 0;
	Anyone.PreferOre = 0;
	Anyone.ToAge = 45;
	Door D(A, Anyone);
	VT_REQUIRE(D.TakeUp() != 0u);
	WorldGen::RegionGraphCache Ways;

	WorldView Frame;
	LifeView Life;
	ChronicleView Told;
	PanelView Page;
	MapView Ground;
	ClimateView Climate;
	TakeView(A.Instance(), Cold(A), Frame);
	TakeLifeView(A.Instance(), Cold(A), Ways, Life);
	TakeChronicleView(A.Instance(), Cold(A), Told);
	TakePanel(Frame, Life, Told, Page);
	TakeMapView(A.Instance(), Cold(A), Ground);
	TakeClimateView(A.Instance(), Cold(A), Climate);

	VT_CHECK_EQ(Frame.Season, 0u);
	uint32 Words = 0;
	for (const RegionView& R : Frame.Regions)
	{
		Words += R.Climate != 0u ? 1u : 0u;
	}
	VT_CHECK_MSG(Words == 0u, "%u regions carry a climate word without a climate", Words);
	VT_CHECK(Life.Season == 0u && Life.Degrees == 0 && Life.Chill == 0u && Life.Winter == 0u);
	VT_CHECK(RowOf(Page, RowKind::Weather).empty());
	VT_CHECK(RowOf(Page, RowKind::Body).find("chill") == std::string::npos);
	VT_CHECK(Climate.Season == 0u && Climate.Tiles.empty());
	const ClimateViewStats Empty = MeasureClimateView(Climate);
	VT_CHECK(Empty.Tiles == 0u && Empty.Frost == 0u && Empty.Coldest == 0 && Empty.Warmest == 0);

	// The same takes, again, must agree with themselves - the pre-phase path
	// is the flag-off path, and the witness refuses an unset digest.
	WorldView Frame2;
	LifeView Life2;
	PanelView Page2;
	TakeView(A.Instance(), Cold(A), Frame2);
	TakeLifeView(A.Instance(), Cold(A), Ways, Life2);
	TakePanel(Frame2, Life2, Told, Page2);
	VT_CHECK_DIGEST_EQ(MeasureView(Frame).Digest, MeasureView(Frame2).Digest);
	VT_CHECK_DIGEST_EQ(MeasureLifeView(Life).Digest, MeasureLifeView(Life2).Digest);
	VT_CHECK_DIGEST_EQ(MeasurePanel(Page).Digest, MeasurePanel(Page2).Digest);

	// AND WITH THE FLAG ON, the same world reads differently: the view sees
	// the climate, so refusing on Options::Climate is not ceremony either.
	WorldView Warmed;
	LifeView Lived;
	PanelView Shown;
	TakeView(A.Instance(), Warm(A), Warmed);
	TakeLifeView(A.Instance(), Warm(A), Ways, Lived);
	TakePanel(Warmed, Lived, Told, Shown);
	VT_CHECK_MSG(Warmed.Season >= 1u && Warmed.Season <= 4u, "season %u", Warmed.Season);
	Words = 0;
	for (const RegionView& R : Warmed.Regions)
	{
		Words += R.Climate != 0u ? 1u : 0u;
	}
	VT_CHECK_MSG(Words == Warmed.Regions.size(), "%u of %zu regions carry a climate word", Words,
				 Warmed.Regions.size());
	VT_CHECK(MeasureView(Warmed).Digest != MeasureView(Frame).Digest);
	VT_CHECK(Lived.Season == Warmed.Season);
	VT_CHECK(MeasureLifeView(Lived).Digest != MeasureLifeView(Life).Digest);
	const std::string Weather = RowOf(Shown, RowKind::Weather);
	VT_CHECK_MSG(!Weather.empty(), "the page has no weather row with a climate");
	VT_CHECK_MSG(Weather.find(" deg here  chill 0") != std::string::npos, "weather row: %s", Weather.c_str());
	VT_CHECK_MSG(RowOf(Shown, RowKind::Body).find("  chill 0") != std::string::npos, "body row: %s",
				 RowOf(Shown, RowKind::Body).c_str());
	// The digest row is still the LAST row of the page.
	VT_CHECK(Shown.RowCount > 0u && Shown.Rows[Shown.RowCount - 1u].Kind == static_cast<uint32>(RowKind::Digest));
	VAELEN_LOG_INFO(LogClimateView, "weather row: %s", Weather.c_str());
}

VAELEN_TEST(Climate, TheRegionWordIsTheKernelsFigures)
{
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const World& W = A.Instance();
	const ViewSources From = Warm(A);
	WorldView Frame;
	TakeView(W, From, Frame);
	VT_REQUIRE(!Frame.Regions.empty());

	// Every region's word unpacks to what the kernel says at its centroid:
	// today's degrees floored, the year's coldest and warmest, the outlook
	// from the growing days, the hard flag from the severity.
	std::vector<WorldGen::YearShape> Years;
	WorldGen::ShapeRegionYears(W, From.Types.World, Frame.Year, From.Climate, Years);
	const uint32 Day = static_cast<uint32>((W.Now() % History::TicksPerYear) / 24u);
	uint32 Agree = 0;
	int32 Coldest = 127, Warmest = -128;
	for (const RegionView& R : Frame.Regions)
	{
		const RegionClimate C = RegionClimateOf(R);
		VT_REQUIRE(R.Index < Years.size());
		const WorldGen::YearShape& Y = Years[R.Index];
		const Fix64 Now = WorldGen::TileTemperatureOn(W.Map(), From.Types.World.Layers, R.CentroidTile, Day);
		const uint32 Outlook = Y.GrowingDays * 100u / From.Climate.GrowFullDays;
		const bool Same =
			C.Now == ClampDegrees(Now.FloorToInt()) && C.Coldest == ClampDegrees(Y.Coldest.FloorToInt()) &&
			C.Warmest == ClampDegrees(Y.Warmest.FloorToInt()) && C.Outlook == (Outlook > 100u ? 100u : Outlook) &&
			C.Hard == (WorldGen::WinterSeverity(Y, From.Climate) >= 2u ? 1u : 0u);
		Agree += Same ? 1u : 0u;
		Coldest = C.Coldest < Coldest ? C.Coldest : Coldest;
		Warmest = C.Warmest > Warmest ? C.Warmest : Warmest;
	}
	VT_CHECK_MSG(Agree == Frame.Regions.size(), "%u of %zu region words agree with the kernel", Agree,
				 Frame.Regions.size());
	VT_CHECK_MSG(Coldest < Warmest, "the regions span no range: coldest %d warmest %d", Coldest, Warmest);
	// The packing round-trips its extremes and clamps what it cannot hold.
	const RegionView Bent{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, PackRegionClimate(-200, -128, 127, 250, true)};
	const RegionClimate Edge = RegionClimateOf(Bent);
	VT_CHECK(Edge.Now == -128 && Edge.Coldest == -128 && Edge.Warmest == 127 && Edge.Outlook == 100u &&
			 Edge.Hard == 1u);
	const RegionView Zero{};
	const RegionClimate None = RegionClimateOf(Zero);
	VT_CHECK(None.Now == 0 && None.Coldest == 0 && None.Warmest == 0 && None.Outlook == 0u && None.Hard == 0u);
}
