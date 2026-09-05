// VAELEN - VaelenSim
// World generation stages.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_WorldGen.cpp
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/Noise.h"

#include <vector>

namespace Vaelen::WorldGen
{
	WorldLayers WorldLayers::Declare(WorldMap& Map)
	{
		WorldLayers L;
		L.Elevation = Map.AddLayer<int64>("elevation");
		L.Terrain = Map.AddLayer<uint8>("terrain");
		L.Slope = Map.AddLayer<int64>("slope");
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
} // namespace Vaelen::WorldGen
