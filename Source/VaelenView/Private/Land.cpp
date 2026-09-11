// VAELEN - VaelenView
// Phase 13 task 13.07a: the ground itself, as a renderer needs it.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/deterministic tests in Tests/View/Test_Land.cpp
#include "Vaelen/View/Land.h"

#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/TileGrid.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Sim/WorldMap.h"

#include <algorithm>
#include <vector>

namespace Vaelen::View
{
	// The one place allowed to see both the enum and the view constant.
	static_assert(BiomeKinds == static_cast<uint32>(WorldGen::Biome::Count),
				  "View::BiomeKinds must match WorldGen::Biome::Count");
} // namespace Vaelen::View

namespace Vaelen::View
{
	// The first four GroundFlag bits are WorldGen::TerrainFlag, copied. Copied
	// values drift; asserted values cannot. If Phase 02 ever renumbers a flag,
	// this file stops compiling instead of quietly painting the coast inland.
	static_assert(GroundFlag::Land == WorldGen::TerrainFlag::Land, "GroundFlag::Land must mirror TerrainFlag::Land");
	static_assert(GroundFlag::Coast == WorldGen::TerrainFlag::Coast,
				  "GroundFlag::Coast must mirror TerrainFlag::Coast");
	static_assert(GroundFlag::Shore == WorldGen::TerrainFlag::Shore,
				  "GroundFlag::Shore must mirror TerrainFlag::Shore");
	static_assert(GroundFlag::Border == WorldGen::TerrainFlag::Border,
				  "GroundFlag::Border must mirror TerrainFlag::Border");
	static_assert(
		(GroundFlag::River & (GroundFlag::Land | GroundFlag::Coast | GroundFlag::Shore | GroundFlag::Border)) == 0 &&
			(GroundFlag::Lake & (GroundFlag::Land | GroundFlag::Coast | GroundFlag::Shore | GroundFlag::Border)) == 0,
		"the hydrology bits must not collide with the terrain bits");

	namespace
	{
		/// Fix64 raw (Q32.32) to Q16.16, saturated rather than wrapped. An
		/// elevation past 32767 units is not a mountain this can draw, and
		/// silently wrapping it to a trench is the worse of the two answers.
		int32 Units16(int64 Raw) noexcept
		{
			// C++20 defines >> on a negative signed value as an arithmetic shift.
			const int64 Shifted = Raw >> 16;
			const int64 Low = static_cast<int64>(-2147483647 - 1);
			const int64 High = static_cast<int64>(2147483647);
			return static_cast<int32>(std::clamp(Shifted, Low, High));
		}
	} // namespace

	void TakeMapView(const World& W, const ViewSources& From, MapView& Out)
	{
		Out.Tiles.clear();
		Out.Tick = static_cast<uint64>(W.Now());
		Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		Out.Width = 0;
		Out.Height = 0;

		const WorldMap& Map = W.Map();
		const WorldGrid Grid = Map.Grid();
		if (!Grid.IsValid())
		{
			return;
		}

		// Every layer is asked for its own length rather than trusted to match
		// the grid. A world that declared its layers and never generated has a
		// valid grid and empty layers, and reading one of those by tile index
		// is exactly the bug this view exists to make impossible.
		const WorldGen::WorldSetup& Setup = From.Types.World;
		const auto& Height = Map.GetLayer(Setup.Layers.Elevation);
		const auto& Terrain = Map.GetLayer(Setup.Layers.Terrain);
		const auto& Biome = Map.GetLayer(Setup.Layers.Biome);
		const auto& Rivers = Map.GetLayer(Setup.Hydro.RiverIndex);
		const auto& Lakes = Map.GetLayer(Setup.Hydro.LakeIndex);
		const auto& Regions = Map.GetLayer(Setup.Regions.RegionIndex);
		const uint32 Tiles = Grid.TileCount();
		if (Terrain.Count() < Tiles || Height.Count() < Tiles || Biome.Count() < Tiles)
		{
			return;
		}
		// Hydrology and regions are OPTIONAL, the way ViewSources treats trade
		// and bondage: a world generated only as far as Elevation still has
		// ground worth drawing, and it says "no rivers" rather than refusing.
		const bool HasWater = Rivers.Count() >= Tiles && Lakes.Count() >= Tiles;
		const bool HasRegions = Regions.Count() >= Tiles;

		Out.Width = Grid.Width;
		Out.Height = Grid.Height;
		Out.Tiles.resize(Tiles);
		for (uint32 I = 0; I < Tiles; ++I)
		{
			TileView& T = Out.Tiles[I];
			T.Elevation = Units16(Height[I]);
			T.Biome = Biome[I];
			T.Ground = static_cast<uint8>(
				Terrain[I] & (GroundFlag::Land | GroundFlag::Coast | GroundFlag::Shore | GroundFlag::Border));
			if (HasWater)
			{
				T.Ground = static_cast<uint8>(T.Ground | (Rivers[I] != 0 ? GroundFlag::River : uint8{0}));
				T.Ground = static_cast<uint8>(T.Ground | (Lakes[I] != 0 ? GroundFlag::Lake : uint8{0}));
			}
			T.Region = HasRegions ? Regions[I] : uint16{0};
		}
	}

	const TileView* TileIn(const MapView& V, uint32 X, uint32 Y)
	{
		if (X >= V.Width || Y >= V.Height)
		{
			return nullptr;
		}
		const usize At = static_cast<usize>(Y) * V.Width + X;
		return At < V.Tiles.size() ? &V.Tiles[At] : nullptr;
	}

	MapStats MeasureMapView(const MapView& V)
	{
		MapStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		uint32 Most = 0;
		for (const TileView& T : V.Tiles)
		{
			Most = T.Region > Most ? T.Region : Most;
		}
		std::vector<uint8> Seen(static_cast<usize>(Most) + 1u, uint8{0});
		for (const TileView& T : V.Tiles)
		{
			++Out.Tiles;
			Out.Land += (T.Ground & GroundFlag::Land) != 0 ? 1u : 0u;
			Out.Coast += (T.Ground & GroundFlag::Coast) != 0 ? 1u : 0u;
			Out.Water += (T.Ground & (GroundFlag::River | GroundFlag::Lake)) != 0 ? 1u : 0u;
			if (T.Biome < BiomeKinds)
			{
				++Out.Biomes[T.Biome];
			}
			if (T.Region != 0 && Seen[T.Region] == 0)
			{
				Seen[T.Region] = 1;
				++Out.Regions;
			}
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&T), sizeof(TileView)));
		}
		Out.Bytes = static_cast<uint32>(sizeof(MapView) + V.Tiles.size() * sizeof(TileView));
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::View
