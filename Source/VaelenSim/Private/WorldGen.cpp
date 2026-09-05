// VAELEN - VaelenSim
// World generation stages.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_WorldGen.cpp
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/Noise.h"

#include <algorithm>
#include <vector>

namespace Vaelen::WorldGen
{
	WorldLayers WorldLayers::Declare(WorldMap& Map)
	{
		WorldLayers L;
		L.Elevation = Map.AddLayer<int64>("elevation");
		L.Terrain = Map.AddLayer<uint8>("terrain");
		L.Slope = Map.AddLayer<int64>("slope");
		L.SeaDistance = Map.AddLayer<uint16>("seadistance");
		L.Temperature = Map.AddLayer<int64>("temperature");
		L.Moisture = Map.AddLayer<int64>("moisture");
		L.Biome = Map.AddLayer<uint8>("biome");
		return L;
	}

	namespace
	{
		Fix64 ParamOr(const WorldGenConfig& Config, uint32 Index, Fix64 Default) noexcept
		{
			return Config.Params[Index] == 0 ? Default : Fix64::FromRaw(Config.Params[Index]);
		}
	} // namespace

	ElevationParams ElevationParams::Resolve(const WorldGenConfig& Config) noexcept
	{
		ElevationParams P;
		P.ContinentFrequency = ParamOr(Config, ParamIndex::ContinentFrequency, P.ContinentFrequency);
		P.ContinentWarp = ParamOr(Config, ParamIndex::ContinentWarp, P.ContinentWarp);
		P.ReliefFrequency = ParamOr(Config, ParamIndex::ReliefFrequency, P.ReliefFrequency);
		P.ReliefAmplitude = ParamOr(Config, ParamIndex::ReliefAmplitude, P.ReliefAmplitude);
		P.RidgeAmplitude = ParamOr(Config, ParamIndex::RidgeAmplitude, P.RidgeAmplitude);
		P.ContinentAmplitude = ParamOr(Config, ParamIndex::ContinentAmplitude, P.ContinentAmplitude);
		P.EdgeFalloff = ParamOr(Config, ParamIndex::EdgeFalloff, P.EdgeFalloff);
		P.ContinentBias = ParamOr(Config, ParamIndex::ContinentBias, P.ContinentBias);
		if (Config.Params[ParamIndex::ElevationOctaves] > 0 && Config.Params[ParamIndex::ElevationOctaves] <= 12)
		{
			P.Octaves = static_cast<uint32>(Config.Params[ParamIndex::ElevationOctaves]);
		}
		return P;
	}

	bool GenerateElevation(WorldMap& Map, const WorldLayers& Layers, uint64 Seed)
	{
		if (!Map.IsReady())
		{
			VAELEN_CHECKF(false, "GenerateElevation: the map has no grid (call WorldMap::Reset first)");
			return false;
		}
		const WorldGrid& Grid = Map.Grid();
		const ElevationParams P = ElevationParams::Resolve(Map.Config());
		TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);

		// Stage seeds derive from the world seed and the stage name, never shared.
		const uint64 ContinentSeed =
			Noise::LatticeHash(Seed, static_cast<int32>(HashString("continent") & 0x7fffffff), 0);
		const uint64 ReliefSeed = Noise::LatticeHash(Seed, static_cast<int32>(HashString("relief") & 0x7fffffff), 1);
		const uint64 RidgeSeed = Noise::LatticeHash(Seed, static_cast<int32>(HashString("ridge") & 0x7fffffff), 2);

		Noise::FractalParams Continent;
		Continent.Octaves = 3;
		Noise::FractalParams Relief;
		Relief.Octaves = P.Octaves;
		Noise::FractalParams Ridge;
		Ridge.Octaves = 4;

		// Normalised coordinates: u = x / Width in [0, 1), v = y / Width so the
		// lattice is square regardless of the aspect ratio.
		const Fix64 InvWidth = Fix64::One() / Fix64::FromInt(static_cast<int32>(Grid.Width));
		const Fix64 HalfW = Fix64::FromInt(static_cast<int32>(Grid.Width)).ShiftRight(1);
		const Fix64 HalfH = Fix64::FromInt(static_cast<int32>(Grid.Height)).ShiftRight(1);
		const Fix64 InvHalfW = Fix64::One() / HalfW;
		const Fix64 InvHalfH = Fix64::One() / HalfH;
		const Fix64 One = Fix64::One();

		for (uint32 Y = 0; Y < Grid.Height; ++Y)
		{
			for (uint32 X = 0; X < Grid.Width; ++X)
			{
				const Fix64 U = Fix64::FromInt(static_cast<int32>(X)) * InvWidth;
				const Fix64 V = Fix64::FromInt(static_cast<int32>(Y)) * InvWidth;

				// Continental mask in [-1, 1], warped so coasts are not blobby.
				const Fix64 Mask = Noise::Warped2D(ContinentSeed, U * P.ContinentFrequency, V * P.ContinentFrequency,
												   P.ContinentWarp, Continent) +
								   P.ContinentBias;

				// Edge falloff: the sea surrounds the world. d = max(|dx|, |dy|) normalised
				// to [0, 1] at the border; below EdgeFalloff nothing changes, then the mask
				// drops linearly to -1 at the border.
				const Fix64 DX = Fix64::Abs((Fix64::FromInt(static_cast<int32>(X)) - HalfW) * InvHalfW);
				const Fix64 DY = Fix64::Abs((Fix64::FromInt(static_cast<int32>(Y)) - HalfH) * InvHalfH);
				const Fix64 D = Fix64::MaxOf(DX, DY);
				Fix64 Falloff; // 0 inside, up to 1 at the border
				if (D > P.EdgeFalloff)
				{
					Falloff = Fix64::Clamp((D - P.EdgeFalloff) / (One - P.EdgeFalloff), Fix64::Zero(), One);
				}
				const Fix64 ShapedMask = Mask - Falloff * 2 - Falloff * Fix64::Abs(Mask);

				// Relief: fractal detail everywhere; ridges: 1 - |n| sharpened, land only.
				const Fix64 ReliefN =
					Noise::Fractal2D(ReliefSeed, U * P.ReliefFrequency, V * P.ReliefFrequency, Relief);
				const Fix64 RidgeN = One - Fix64::Abs(Noise::Fractal2D(RidgeSeed, U * P.ReliefFrequency.ShiftRight(1),
																	   V * P.ReliefFrequency.ShiftRight(1), Ridge));
				const Fix64 RidgeSharp = RidgeN * RidgeN * RidgeN;

				Fix64 Height = ShapedMask * P.ContinentAmplitude + ReliefN * P.ReliefAmplitude;
				if (ShapedMask > Fix64::Zero())
				{
					// Mountains rise where the continent is solid, scaled by the mask.
					Height += RidgeSharp * P.RidgeAmplitude * ShapedMask;
				}
				Elevation[Grid.IndexOf({static_cast<int32>(X), static_cast<int32>(Y)})] = Height.Raw;
			}
		}
		ClassifyTerrain(Map, Layers);
		return true;
	}

	void ClassifyTerrain(WorldMap& Map, const WorldLayers& Layers)
	{
		if (!Map.IsReady())
		{
			return;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		TileLayer<int64>& Slope = Map.GetLayer(Layers.Slope);
		const int64 Sea = Map.Config().SeaLevel;

		for (uint32 i = 0; i < Grid.TileCount(); ++i)
		{
			Terrain[i] = Elevation[i] > Sea ? TerrainFlag::Land : uint8{0};
		}
		for (uint32 i = 0; i < Grid.TileCount(); ++i)
		{
			const TileCoord C = Grid.CoordOf(i);
			const bool Land = (Terrain[i] & TerrainFlag::Land) != 0;
			bool Touches = false;
			int64 MaxDelta = 0;
			Grid.ForEachNeighbour(C, 8,
								  [&](TileCoord N, uint32 Slot)
								  {
									  const uint32 J = Grid.IndexOf(N);
									  const int64 Delta = Fix64::WrapSub(Elevation[J], Elevation[i]);
									  const int64 Magnitude = Delta < 0 ? Fix64::WrapNeg(Delta) : Delta;
									  MaxDelta = Magnitude > MaxDelta ? Magnitude : MaxDelta;
									  if ((Slot & 1u) == 0 && ((Terrain[J] & TerrainFlag::Land) != 0) != Land)
									  {
										  Touches = true;
									  }
								  });
			Slope[i] = MaxDelta;
			uint8 Flags = Terrain[i];
			if (Touches)
			{
				Flags |= Land ? TerrainFlag::Coast : TerrainFlag::Shore;
			}
			if (C.X == 0 || C.Y == 0 || static_cast<uint32>(C.X) + 1 == Grid.Width ||
				static_cast<uint32>(C.Y) + 1 == Grid.Height)
			{
				Flags |= TerrainFlag::Border;
			}
			Terrain[i] = Flags;
		}
	}

	ElevationStats MeasureElevation(const WorldMap& Map, const WorldLayers& Layers)
	{
		ElevationStats S;
		if (!Map.IsReady())
		{
			return S;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<int64>& Slope = Map.GetLayer(Layers.Slope);
		S.MinElevation = Fix64::Max();
		S.MaxElevation = Fix64::Min();
		for (uint32 i = 0; i < Grid.TileCount(); ++i)
		{
			const bool Land = (Terrain[i] & TerrainFlag::Land) != 0;
			S.LandTiles += Land ? 1u : 0u;
			S.SeaTiles += Land ? 0u : 1u;
			S.CoastTiles += (Terrain[i] & TerrainFlag::Coast) != 0 ? 1u : 0u;
			S.BorderLandTiles += (Land && (Terrain[i] & TerrainFlag::Border) != 0) ? 1u : 0u;
			S.MinElevation = Fix64::MinOf(S.MinElevation, Fix64::FromRaw(Elevation[i]));
			S.MaxElevation = Fix64::MaxOf(S.MaxElevation, Fix64::FromRaw(Elevation[i]));
			S.MaxSlope = Fix64::MaxOf(S.MaxSlope, Fix64::FromRaw(Slope[i]));
		}
		// Landmasses: 4-connected flood fill over land, deterministic scan order.
		std::vector<uint8> Visited(Grid.TileCount(), 0);
		std::vector<uint32> Stack;
		for (uint32 Start = 0; Start < Grid.TileCount(); ++Start)
		{
			if (Visited[Start] != 0 || (Terrain[Start] & TerrainFlag::Land) == 0)
			{
				continue;
			}
			uint32 Size = 0;
			Stack.clear();
			Stack.push_back(Start);
			Visited[Start] = 1;
			while (!Stack.empty())
			{
				const uint32 I = Stack.back();
				Stack.pop_back();
				++Size;
				Grid.ForEachNeighbour(Grid.CoordOf(I), 4,
									  [&](TileCoord N, uint32)
									  {
										  const uint32 J = Grid.IndexOf(N);
										  if (Visited[J] == 0 && (Terrain[J] & TerrainFlag::Land) != 0)
										  {
											  Visited[J] = 1;
											  Stack.push_back(J);
										  }
									  });
			}
			++S.LandmassCount;
			S.LargestLandmassTiles = Size > S.LargestLandmassTiles ? Size : S.LargestLandmassTiles;
		}
		return S;
	}

	Hash64 LayerDigest(const WorldMap& Map, uint32 LayerIndex)
	{
		const ITileLayer* L = Map.GetLayerBase(LayerIndex);
		return L != nullptr ? L->Hash() : 0;
	}

	void ExportAscii(const WorldMap& Map, const WorldLayers& Layers, uint32 Columns, std::string& Out)
	{
		Out.clear();
		if (!Map.IsReady() || Columns == 0)
		{
			return;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		const int64 Sea = Map.Config().SeaLevel;
		const uint32 Cols = Columns > Grid.Width ? Grid.Width : Columns;
		const uint32 CellW = Grid.Width / Cols;
		const uint32 CellH = CellW * 2 > Grid.Height ? Grid.Height : CellW * 2; // terminal cells are ~2:1
		const uint32 Rows = Grid.Height / CellH;
		Out.reserve(static_cast<usize>(Rows) * (Cols + 1));
		for (uint32 R = 0; R < Rows; ++R)
		{
			for (uint32 C = 0; C < Cols; ++C)
			{
				// Mean elevation of the cell, relative to sea level, in elevation units.
				int64 Sum = 0;
				uint32 Count = 0;
				for (uint32 Y = R * CellH; Y < (R + 1) * CellH; ++Y)
				{
					for (uint32 X = C * CellW; X < (C + 1) * CellW; ++X)
					{
						Sum += (Elevation[Y * Grid.Width + X] - Sea) >> 32; // integer units
						++Count;
					}
				}
				const int64 Mean = Sum / static_cast<int64>(Count);
				char Glyph = '~';
				if (Mean > 1800)
				{
					Glyph = 'A';
				}
				else if (Mean > 900)
				{
					Glyph = '^';
				}
				else if (Mean > 300)
				{
					Glyph = ':';
				}
				else if (Mean > 0)
				{
					Glyph = '.';
				}
				else if (Mean > -400)
				{
					Glyph = '-';
				}
				Out.push_back(Glyph);
			}
			Out.push_back('\n');
		}
	}
	// ── 02.04 climate and biomes ─────────────────────────────────────────────

	ClimateParams ClimateParams::Resolve(const WorldGenConfig& Config) noexcept
	{
		ClimateParams P;
		P.EquatorTemperature = ParamOr(Config, ParamIndex::EquatorTemperature, P.EquatorTemperature);
		P.PoleTemperature = ParamOr(Config, ParamIndex::PoleTemperature, P.PoleTemperature);
		P.LapseRate = ParamOr(Config, ParamIndex::LapseRate, P.LapseRate);
		P.TemperatureNoise = ParamOr(Config, ParamIndex::TemperatureNoise, P.TemperatureNoise);
		P.RainDecay = ParamOr(Config, ParamIndex::RainDecay, P.RainDecay);
		P.OrographicRain = ParamOr(Config, ParamIndex::OrographicRain, P.OrographicRain);
		P.SeaRecovery = ParamOr(Config, ParamIndex::SeaRecovery, P.SeaRecovery);
		P.MoistureNoise = ParamOr(Config, ParamIndex::MoistureNoise, P.MoistureNoise);
		P.ProximityRange = ParamOr(Config, ParamIndex::ProximityRange, P.ProximityRange);
		P.ProximityWeight = ParamOr(Config, ParamIndex::ProximityWeight, P.ProximityWeight);
		return P;
	}

	const char* BiomeName(Biome B) noexcept
	{
		switch (B)
		{
		case Biome::Ocean:
			return "Ocean";
		case Biome::Ice:
			return "Ice";
		case Biome::Tundra:
			return "Tundra";
		case Biome::BorealForest:
			return "BorealForest";
		case Biome::ColdSteppe:
			return "ColdSteppe";
		case Biome::TemperateForest:
			return "TemperateForest";
		case Biome::Grassland:
			return "Grassland";
		case Biome::Scrubland:
			return "Scrubland";
		case Biome::TropicalForest:
			return "TropicalForest";
		case Biome::Savanna:
			return "Savanna";
		case Biome::Desert:
			return "Desert";
		case Biome::Alpine:
			return "Alpine";
		case Biome::Count:
			break;
		}
		return "Unknown";
	}

	char BiomeGlyph(Biome B) noexcept
	{
		switch (B)
		{
		case Biome::Ocean:
			return '~';
		case Biome::Ice:
			return '*';
		case Biome::Tundra:
			return '-';
		case Biome::BorealForest:
			return 'f';
		case Biome::ColdSteppe:
			return ',';
		case Biome::TemperateForest:
			return 'F';
		case Biome::Grassland:
			return '"';
		case Biome::Scrubland:
			return ';';
		case Biome::TropicalForest:
			return 'T';
		case Biome::Savanna:
			return 's';
		case Biome::Desert:
			return 'd';
		case Biome::Alpine:
			return 'A';
		case Biome::Count:
			break;
		}
		return '?';
	}

	Fix64 LatitudeOfRow(const WorldGrid& Grid, uint32 Y) noexcept
	{
		if (Grid.Height < 2)
		{
			return Fix64::Zero();
		}
		// (2y - (H - 1)) / (H - 1): exactly -1 on the top row and +1 on the bottom row.
		const int32 Span = static_cast<int32>(Grid.Height) - 1;
		return Fix64::FromInt(2 * static_cast<int32>(Y) - Span) / Fix64::FromInt(Span);
	}

	int32 PrevailingWind(Fix64 Latitude) noexcept
	{
		const Fix64 A = Fix64::Abs(Latitude);
		if (A < Fix64::FromRatio(1, 3))
		{
			return -1; // trade winds
		}
		if (A < Fix64::FromRatio(2, 3))
		{
			return +1; // westerlies
		}
		return -1; // polar easterlies
	}

	Fix64 SeasonalOffset(Fix64 Latitude, uint32 Season) noexcept
	{
		const Fix64 Amplitude = Fix64::FromInt(4) + Fix64::Abs(Latitude) * 16;
		switch (Season % 4)
		{
		case 1:
			return Amplitude;
		case 3:
			return -Amplitude;
		default:
			return Fix64::Zero();
		}
	}

	Biome ClassifyBiome(Fix64 Temperature, Fix64 Moisture, Fix64 ElevationAboveSea, bool Land) noexcept
	{
		if (!Land)
		{
			return Biome::Ocean;
		}
		if (ElevationAboveSea > Fix64::FromInt(2500))
		{
			return Biome::Alpine;
		}
		if (Temperature < Fix64::FromInt(-10))
		{
			return Biome::Ice;
		}
		if (Temperature < Fix64::FromInt(0))
		{
			return Biome::Tundra;
		}
		if (Temperature < Fix64::FromInt(8))
		{
			return Moisture > Fix64::FromRatio(7, 20) ? Biome::BorealForest : Biome::ColdSteppe;
		}
		if (Temperature < Fix64::FromInt(20))
		{
			if (Moisture > Fix64::FromRatio(1, 2))
			{
				return Biome::TemperateForest;
			}
			return Moisture > Fix64::FromRatio(1, 4) ? Biome::Grassland : Biome::Scrubland;
		}
		if (Moisture > Fix64::FromRatio(3, 5))
		{
			return Biome::TropicalForest;
		}
		return Moisture > Fix64::FromRatio(3, 10) ? Biome::Savanna : Biome::Desert;
	}

	bool GenerateClimate(WorldMap& Map, const WorldLayers& Layers, uint64 Seed)
	{
		if (!Map.IsReady())
		{
			VAELEN_CHECKF(false, "GenerateClimate: the map has no grid (call WorldMap::Reset first)");
			return false;
		}
		const WorldGrid& Grid = Map.Grid();
		const ClimateParams P = ClimateParams::Resolve(Map.Config());
		const TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		TileLayer<uint16>& SeaDistance = Map.GetLayer(Layers.SeaDistance);
		TileLayer<int64>& Temperature = Map.GetLayer(Layers.Temperature);
		TileLayer<int64>& Moisture = Map.GetLayer(Layers.Moisture);
		TileLayer<uint8>& BiomeLayer = Map.GetLayer(Layers.Biome);
		const int64 Sea = Map.Config().SeaLevel;
		const uint64 TempSeed = Noise::LatticeHash(Seed, static_cast<int32>(HashString("temperature") & 0x7fffffff), 3);
		const uint64 RainSeed = Noise::LatticeHash(Seed, static_cast<int32>(HashString("moisture") & 0x7fffffff), 4);

		// Sea distance: multi-source BFS from every sea tile, 4-connected, in
		// scan order (deterministic); a world without sea gets the maximum.
		{
			std::vector<uint32> Queue;
			Queue.reserve(Grid.TileCount());
			for (uint32 i = 0; i < Grid.TileCount(); ++i)
			{
				const bool IsSea = (Terrain[i] & TerrainFlag::Land) == 0;
				SeaDistance[i] = IsSea ? uint16{0} : uint16{0xffff};
				if (IsSea)
				{
					Queue.push_back(i);
				}
			}
			for (usize Head = 0; Head < Queue.size(); ++Head)
			{
				const uint32 I = Queue[Head];
				const uint16 Next = static_cast<uint16>(SeaDistance[I] == 0xffff ? 0xffff : SeaDistance[I] + 1);
				Grid.ForEachNeighbour(Grid.CoordOf(I), 4,
									  [&](TileCoord N, uint32)
									  {
										  const uint32 J = Grid.IndexOf(N);
										  if (SeaDistance[J] == 0xffff && Next != 0xffff)
										  {
											  SeaDistance[J] = Next;
											  Queue.push_back(J);
										  }
									  });
			}
		}

		// Temperature: latitude band, altitude lapse, local noise.
		const Fix64 NoiseScale = Fix64::FromInt(12) / Fix64::FromInt(static_cast<int32>(Grid.Width));
		Noise::FractalParams Local;
		Local.Octaves = 3;
		for (uint32 Y = 0; Y < Grid.Height; ++Y)
		{
			const Fix64 Lat = Fix64::Abs(LatitudeOfRow(Grid, Y));
			const Fix64 Band = P.EquatorTemperature - (P.EquatorTemperature - P.PoleTemperature) * Lat;
			for (uint32 X = 0; X < Grid.Width; ++X)
			{
				const uint32 I = Y * Grid.Width + X;
				const Fix64 Above = Fix64::MaxOf(Fix64::FromRaw(Elevation[I] - Sea), Fix64::Zero());
				const Fix64 Lapse = (Above / Fix64::FromInt(1000)) * P.LapseRate;
				const Fix64 N = Noise::Fractal2D(TempSeed, Fix64::FromInt(static_cast<int32>(X)) * NoiseScale,
												 Fix64::FromInt(static_cast<int32>(Y)) * NoiseScale, Local);
				Temperature[I] = (Band - Lapse + N * P.TemperatureNoise).Raw;
			}
		}

		// Moisture: per row, a parcel of humidity travels with the prevailing
		// wind; over sea it recovers, over land it rains (a base fraction plus
		// an orographic share of any climb). The base fraction is 1 / (decay
		// distance in tiles) so the model does not depend on the grid
		// resolution. What falls, scaled so a full parcel gives 1, is the
		// advected moisture; sea proximity adds a resolution-independent share
		// so no coast is ever dry and the interior never reaches exactly zero.
		const Fix64 One = Fix64::One();
		const Fix64 WidthF = Fix64::FromInt(static_cast<int32>(Grid.Width));
		const Fix64 DecayTiles = Fix64::MaxOf(P.RainDecay * WidthF, One);
		const Fix64 BaseRate = One / DecayTiles;
		const Fix64 RangeTiles = Fix64::MaxOf(P.ProximityRange * WidthF, One);
		const Fix64 AdvectionWeight = One - P.ProximityWeight;
		for (uint32 Y = 0; Y < Grid.Height; ++Y)
		{
			const int32 Wind = PrevailingWind(LatitudeOfRow(Grid, Y));
			Fix64 Humidity = One;
			int64 Previous = Sea;
			for (uint32 Step = 0; Step < Grid.Width; ++Step)
			{
				const uint32 X = Wind > 0 ? Step : Grid.Width - 1 - Step;
				const uint32 I = Y * Grid.Width + X;
				const bool Land = (Terrain[I] & TerrainFlag::Land) != 0;
				Fix64 Advected = One;
				if (Land)
				{
					const Fix64 Climb = Fix64::MaxOf(Fix64::FromRaw(Elevation[I] - Previous), Fix64::Zero());
					const Fix64 Rate = Fix64::MinOf(BaseRate + (Climb / Fix64::FromInt(1000)) * P.OrographicRain, One);
					const Fix64 Fallen = Humidity * Rate;
					Humidity -= Fallen;
					Advected = Fix64::MinOf(Fallen / BaseRate, One);
				}
				else
				{
					Humidity = Fix64::MinOf(Humidity + P.SeaRecovery, One);
				}
				Previous = Land ? Elevation[I] : Sea;
				// Proximity: 1 on the coast, 1/2 at RangeTiles from the sea, rational decay.
				const Fix64 Dist =
					Fix64::FromInt(static_cast<int32>(SeaDistance[I] == 0xffff ? 0xffffu : SeaDistance[I]));
				const Fix64 Proximity = One / (One + Dist / RangeTiles);
				Fix64 M = Land ? Advected * AdvectionWeight + Proximity * P.ProximityWeight : One;
				const Fix64 N = Noise::Fractal2D(RainSeed, Fix64::FromInt(static_cast<int32>(X)) * NoiseScale,
												 Fix64::FromInt(static_cast<int32>(Y)) * NoiseScale, Local);
				M = Fix64::Clamp(M + N * P.MoistureNoise, Fix64::Zero(), One);
				Moisture[I] = M.Raw;
			}
		}

		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			const bool Land = (Terrain[I] & TerrainFlag::Land) != 0;
			BiomeLayer[I] = static_cast<uint8>(ClassifyBiome(
				Fix64::FromRaw(Temperature[I]), Fix64::FromRaw(Moisture[I]), Fix64::FromRaw(Elevation[I] - Sea), Land));
		}
		return true;
	}

	ClimateStats MeasureClimate(const WorldMap& Map, const WorldLayers& Layers)
	{
		ClimateStats S;
		if (!Map.IsReady())
		{
			return S;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<uint16>& SeaDistance = Map.GetLayer(Layers.SeaDistance);
		const TileLayer<int64>& Temperature = Map.GetLayer(Layers.Temperature);
		const TileLayer<int64>& Moisture = Map.GetLayer(Layers.Moisture);
		const TileLayer<uint8>& BiomeLayer = Map.GetLayer(Layers.Biome);
		S.MinTemperature = Fix64::Max();
		S.MaxTemperature = Fix64::Min();
		int64 MoistureSum = 0;
		uint32 LandCount = 0;
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			const uint8 B = BiomeLayer[I] < static_cast<uint8>(Biome::Count) ? BiomeLayer[I] : 0;
			++S.BiomeTiles[B];
			S.MinTemperature = Fix64::MinOf(S.MinTemperature, Fix64::FromRaw(Temperature[I]));
			S.MaxTemperature = Fix64::MaxOf(S.MaxTemperature, Fix64::FromRaw(Temperature[I]));
			S.MaxSeaDistance =
				SeaDistance[I] > S.MaxSeaDistance && SeaDistance[I] != 0xffff ? SeaDistance[I] : S.MaxSeaDistance;
			if ((Terrain[I] & TerrainFlag::Land) != 0)
			{
				MoistureSum += Moisture[I] >> 16; // keep the sum in range
				++LandCount;
			}
		}
		for (uint32 B = 1; B < static_cast<uint32>(Biome::Count); ++B)
		{
			S.DistinctLandBiomes += S.BiomeTiles[B] > 0 ? 1u : 0u;
		}
		S.MeanLandMoisture = LandCount == 0 ? Fix64::Zero() : Fix64::FromRaw((MoistureSum / LandCount) << 16);
		return S;
	}

	void ExportBiomeAscii(const WorldMap& Map, const WorldLayers& Layers, uint32 Columns, std::string& Out)
	{
		Out.clear();
		if (!Map.IsReady() || Columns == 0)
		{
			return;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint8>& BiomeLayer = Map.GetLayer(Layers.Biome);
		const uint32 Cols = Columns > Grid.Width ? Grid.Width : Columns;
		const uint32 CellW = Grid.Width / Cols;
		const uint32 CellH = CellW * 2 > Grid.Height ? Grid.Height : CellW * 2;
		const uint32 Rows = Grid.Height / CellH;
		Out.reserve(static_cast<usize>(Rows) * (Cols + 1));
		for (uint32 R = 0; R < Rows; ++R)
		{
			for (uint32 C = 0; C < Cols; ++C)
			{
				uint32 Counts[static_cast<uint32>(Biome::Count)] = {};
				for (uint32 Y = R * CellH; Y < (R + 1) * CellH; ++Y)
				{
					for (uint32 X = C * CellW; X < (C + 1) * CellW; ++X)
					{
						const uint8 B = BiomeLayer[Y * Grid.Width + X];
						++Counts[B < static_cast<uint8>(Biome::Count) ? B : 0];
					}
				}
				uint32 Best = 0;
				for (uint32 B = 1; B < static_cast<uint32>(Biome::Count); ++B)
				{
					if (Counts[B] > Counts[Best])
					{
						Best = B;
					}
				}
				Out.push_back(BiomeGlyph(static_cast<Biome>(Best)));
			}
			Out.push_back('\n');
		}
	}
} // namespace Vaelen::WorldGen
