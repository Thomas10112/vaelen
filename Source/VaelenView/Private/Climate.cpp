// VAELEN - VaelenView
// Phase 18 task 18.04: the climate leaf, taken and measured.
//
// STATUS: PROTOTYPE (Phase 18 task 18.04)
#include "Vaelen/View/Climate.h"
#include "Vaelen/View/Take.h"

#include "Vaelen/Sim/Climate.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"

namespace Vaelen::View
{
	namespace
	{
		int8 Byte(Fix64 Degrees)
		{
			const int32 Whole = Degrees.FloorToInt();
			return static_cast<int8>(Whole < -128 ? -128 : (Whole > 127 ? 127 : Whole));
		}
	} // namespace

	void TakeClimateView(const World& W, const ViewSources& From, ClimateView& Out)
	{
		Out = ClimateView{};
		Out.Tick = static_cast<uint64>(W.Now());
		Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		const CalendarDate Date = W.Clock().Date();
		Out.Day = Date.DayOfYear;
		// Without a climate the leaf is a header and nothing else: Season 0,
		// no tiles, and a measure that says so. The world before Phase 18.
		if (!From.HasClimate || !W.Map().IsReady())
		{
			return;
		}
		Out.Season = 1u + Date.Season;
		const WorldMap& Map = W.Map();
		const WorldGrid& Grid = Map.Grid();
		Out.Width = Grid.Width;
		Out.Height = Grid.Height;
		const uint32 Tiles = Grid.Width * Grid.Height;
		Out.Tiles.resize(Tiles);
		const WorldGen::WorldLayers& Layers = From.Types.World.Layers;
		// Mid-winter and mid-summer are the wave's extremes for every tile,
		// so a renderer has the year's range beside today without a second
		// take: day 315 and day 135 of the 360-day year (Sim/Climate.h).
		for (uint32 Tile = 0; Tile < Tiles; ++Tile)
		{
			const Fix64 Now = WorldGen::TileTemperatureOn(Map, Layers, Tile, Out.Day);
			TileClimate& T = Out.Tiles[Tile];
			T.Now = Byte(Now);
			T.Winter = Byte(WorldGen::TileTemperatureOn(Map, Layers, Tile, 315u));
			T.Summer = Byte(WorldGen::TileTemperatureOn(Map, Layers, Tile, 135u));
			T.Flags = static_cast<uint8>((Now < From.Climate.ColdLine ? ClimateFlag::Frost : 0u) |
										 (Now >= From.Climate.GrowLine ? ClimateFlag::Growing : 0u));
		}
	}

	const TileClimate* TileClimateIn(const ClimateView& V, uint32 X, uint32 Y)
	{
		if (X >= V.Width || Y >= V.Height)
		{
			return nullptr;
		}
		const usize Index = usize{Y} * V.Width + X;
		return Index < V.Tiles.size() ? &V.Tiles[Index] : nullptr;
	}

	ClimateViewStats MeasureClimateView(const ClimateView& V)
	{
		ClimateViewStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		const uint64 Head[6] = {V.Tick, V.Year, V.Day, V.Season, V.Width, V.Height};
		Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(Head), sizeof(Head)));
		Out.Coldest = V.Tiles.empty() ? 0 : 127;
		Out.Warmest = V.Tiles.empty() ? 0 : -128;
		for (const TileClimate& T : V.Tiles)
		{
			++Out.Tiles;
			Out.Frost += (T.Flags & ClimateFlag::Frost) != 0u ? 1u : 0u;
			Out.Growing += (T.Flags & ClimateFlag::Growing) != 0u ? 1u : 0u;
			Out.Coldest = T.Now < Out.Coldest ? T.Now : Out.Coldest;
			Out.Warmest = T.Now > Out.Warmest ? T.Now : Out.Warmest;
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&T), sizeof(TileClimate)));
		}
		Out.Bytes = static_cast<uint32>(sizeof(ClimateView) + V.Tiles.size() * sizeof(TileClimate));
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::View
