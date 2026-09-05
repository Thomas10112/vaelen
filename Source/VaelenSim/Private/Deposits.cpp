// VAELEN - VaelenSim
// Deposits stage implementation.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_Deposits.cpp
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <vector>

namespace Vaelen::WorldGen
{
	const char* ResourceKindName(ResourceKind K) noexcept
	{
		switch (K)
		{
		case ResourceKind::Stone:
			return "Stone";
		case ResourceKind::Timber:
			return "Timber";
		case ResourceKind::Clay:
			return "Clay";
		case ResourceKind::FertileSoil:
			return "FertileSoil";
		case ResourceKind::Salt:
			return "Salt";
		case ResourceKind::IronOre:
			return "IronOre";
		case ResourceKind::CopperOre:
			return "CopperOre";
		case ResourceKind::Gold:
			return "Gold";
		case ResourceKind::Count:
			break;
		}
		return "Unknown";
	}

	DepositTypes DepositTypes::Declare(World& W)
	{
		DepositTypes T;
		T.Deposit = W.Types().Register<DepositInfo>("DepositInfo");
		W.Components().CreatePool(T.Deposit);
		return T;
	}

	DepositLayers DepositLayers::Declare(WorldMap& Map)
	{
		DepositLayers D;
		D.DepositIndex = Map.AddLayer<uint16>("deposit");
		return D;
	}

	DepositParams DepositParams::Resolve(const WorldGenConfig& Config) noexcept
	{
		DepositParams P;
		if (Config.Params[ParamIndex::DepositDensity] > 0)
		{
			P.Density = Fix64::MinOf(Fix64::FromRaw(Config.Params[ParamIndex::DepositDensity]), Fix64::One());
		}
		if (Config.Params[ParamIndex::DepositSpacing] > 0 && Config.Params[ParamIndex::DepositSpacing] < 4096)
		{
			P.Spacing = static_cast<uint32>(Config.Params[ParamIndex::DepositSpacing]);
		}
		return P;
	}

	namespace
	{
		bool IsForest(Biome B) noexcept
		{
			return B == Biome::BorealForest || B == Biome::TemperateForest || B == Biome::TropicalForest;
		}
		bool IsHotDry(Biome B) noexcept
		{
			return B == Biome::Desert || B == Biome::Savanna || B == Biome::Scrubland;
		}
		bool IsArable(Biome B) noexcept
		{
			return B == Biome::Grassland || B == Biome::TemperateForest || B == Biome::Savanna ||
				   B == Biome::TropicalForest || B == Biome::BorealForest;
		}
		/// Linear ramp of X between Lo (0) and Hi (1000), clamped.
		uint32 Ramp(Fix64 X, Fix64 Lo, Fix64 Hi) noexcept
		{
			if (X <= Lo)
			{
				return 0;
			}
			if (X >= Hi)
			{
				return 1000;
			}
			return static_cast<uint32>(((X - Lo) * 1000 / (Hi - Lo)).FloorToInt());
		}
		/// Draw probability per mille per kind and tier.
		constexpr uint32 BaseChance[static_cast<uint32>(ResourceKind::Count)] = {
			55,	 // Stone
			140, // Timber
			160, // Clay
			170, // FertileSoil
			160, // Salt
			24,	 // IronOre
			16,	 // CopperOre
			8,	 // Gold
		};
		constexpr uint32 BaseTier[static_cast<uint32>(ResourceKind::Count)] = {1, 1, 1, 1, 2, 2, 2, 3};
	} // namespace

	uint32 DepositSuitability(ResourceKind Kind, bool Land, bool Coast, Biome B, Fix64 Above, Fix64 Slope, bool River,
							  bool LakeAdjacent, bool RiverAdjacent) noexcept
	{
		if (!Land || B == Biome::Ocean)
		{
			return 0;
		}
		const bool Water = River || LakeAdjacent || RiverAdjacent;
		switch (Kind)
		{
		case ResourceKind::Stone:
			// Slopes and uplands; never on soft river plains.
			return River ? 0
						 : (Ramp(Slope, Fix64::FromInt(40), Fix64::FromInt(300)) +
							Ramp(Above, Fix64::FromInt(400), Fix64::FromInt(1500))) /
							   2;
		case ResourceKind::Timber:
			return IsForest(B) ? (B == Biome::TropicalForest ? 1000 : 800) : 0;
		case ResourceKind::Clay:
			return (Water && Above < Fix64::FromInt(500) && Slope < Fix64::FromInt(200)) ? 900 : 0;
		case ResourceKind::FertileSoil:
			if (!IsArable(B) || Above > Fix64::FromInt(800) || Slope > Fix64::FromInt(150))
			{
				return 0;
			}
			return Water ? 1000 : 500;
		case ResourceKind::Salt:
			if (Coast && IsHotDry(B))
			{
				return 900;
			}
			return (B == Biome::Desert && Slope < Fix64::FromInt(30)) ? 500 : 0;
		case ResourceKind::IronOre:
			return Ramp(Above, Fix64::FromInt(500), Fix64::FromInt(1200)) * (B == Biome::Alpine ? 1000 : 700) / 1000;
		case ResourceKind::CopperOre:
			return Ramp(Above, Fix64::FromInt(300), Fix64::FromInt(1000)) * (IsHotDry(B) ? 1000 : 600) / 1000;
		case ResourceKind::Gold:
			return Above > Fix64::FromInt(1400) ? (River ? 1000 : 600) : 0;
		case ResourceKind::Count:
			break;
		}
		return 0;
	}

	bool GenerateDeposits(World& W, const WorldLayers& Layers, const HydroLayers& Hydro, const RegionLayers& Regions,
						  const DepositLayers& Deposits, const DepositTypes& Types)
	{
		WorldMap& Map = W.Map();
		if (!Map.IsReady())
		{
			VAELEN_CHECKF(false, "GenerateDeposits: the map has no grid (call WorldMap::Reset first)");
			return false;
		}
		const WorldGrid& Grid = Map.Grid();
		const uint32 N = Grid.TileCount();
		const DepositParams P = DepositParams::Resolve(Map.Config());
		const TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<int64>& Slope = Map.GetLayer(Layers.Slope);
		const TileLayer<uint8>& BiomeLayer = Map.GetLayer(Layers.Biome);
		const TileLayer<uint16>& RiverIx = Map.GetLayer(Hydro.RiverIndex);
		const TileLayer<uint16>& LakeIx = Map.GetLayer(Hydro.LakeIndex);
		const TileLayer<uint16>& RegionIx = Map.GetLayer(Regions.RegionIndex);
		TileLayer<uint16>& DepositIx = Map.GetLayer(Deposits.DepositIndex);
		const int64 Sea = Map.Config().SeaLevel;
		const uint64 StageSeed =
			Noise::LatticeHash(W.Config().Seed, static_cast<int32>(HashString("deposits") & 0x7fffffff), 7);

		{
			std::vector<EntityHandle> Doomed;
			W.Components().GetPool(Types.Deposit).ForEach([&](EntityHandle H, DepositInfo&) { Doomed.push_back(H); });
			for (EntityHandle H : Doomed)
			{
				W.DestroyEntity(H);
			}
		}
		for (uint32 I = 0; I < N; ++I)
		{
			DepositIx[I] = 0;
		}

		// One deposit per kind per spacing cell: the cell keeps the best draw.
		const uint32 CellsX = (Grid.Width + P.Spacing - 1) / P.Spacing;
		const uint32 CellsY = (Grid.Height + P.Spacing - 1) / P.Spacing;
		const uint32 KindCount = static_cast<uint32>(ResourceKind::Count);
		struct Candidate
		{
			uint32 Tile = 0xffffffffu;
			uint32 Score = 0; ///< suitability * hash weight
			uint32 Richness = 0;
		};
		std::vector<Candidate> Best(static_cast<usize>(CellsX) * CellsY * KindCount);
		const uint32 DensityPerMille = static_cast<uint32>((P.Density * 1000).FloorToInt());

		for (uint32 I = 0; I < N; ++I)
		{
			const bool Land = (Terrain[I] & TerrainFlag::Land) != 0;
			if (!Land)
			{
				continue;
			}
			const TileCoord C = Grid.CoordOf(I);
			const bool Coast = (Terrain[I] & TerrainFlag::Coast) != 0;
			const Biome B = static_cast<Biome>(BiomeLayer[I] < static_cast<uint8>(Biome::Count) ? BiomeLayer[I] : 0);
			const Fix64 Above = Fix64::FromRaw(Fix64::WrapSub(Elevation[I], Sea));
			const Fix64 SlopeF = Fix64::FromRaw(Slope[I]);
			const bool River = RiverIx[I] != 0;
			bool LakeAdjacent = false;
			bool RiverAdjacent = false;
			Grid.ForEachNeighbour(C, 4,
								  [&](TileCoord NC, uint32)
								  {
									  const uint32 J = Grid.IndexOf(NC);
									  LakeAdjacent = LakeAdjacent || LakeIx[J] != 0;
									  RiverAdjacent = RiverAdjacent || RiverIx[J] != 0;
								  });
			const uint32 Cell = (static_cast<uint32>(C.Y) / P.Spacing) * CellsX + static_cast<uint32>(C.X) / P.Spacing;
			for (uint32 K = 0; K < KindCount; ++K)
			{
				const uint32 Suit = DepositSuitability(static_cast<ResourceKind>(K), Land, Coast, B, Above, SlopeF,
													   River, LakeAdjacent, RiverAdjacent);
				if (Suit == 0)
				{
					continue;
				}
				const uint64 H = Noise::LatticeHash(StageSeed + K, C.X, C.Y);
				const uint32 Roll = static_cast<uint32>(H % 1000000u); // per million
				// Chance per million = base per mille * suitability per mille * density per mille / 1000.
				const uint64 Chance = static_cast<uint64>(BaseChance[K]) * Suit * DensityPerMille / 1000u;
				if (Roll >= Chance)
				{
					continue;
				}
				Candidate& Slot = Best[static_cast<usize>(Cell) * KindCount + K];
				const uint32 Score = Suit * 1000u + static_cast<uint32>((H >> 40) % 1000u);
				if (Slot.Tile == 0xffffffffu || Score > Slot.Score)
				{
					Slot.Tile = I;
					Slot.Score = Score;
					// Richness 1..1000: seven tenths from suitability, the rest from the hash.
					Slot.Richness = 1 + Suit * 7u / 10u + static_cast<uint32>((H >> 20) % 300u);
				}
			}
		}

		// Materialise in tile order (then kind order) so indices follow the scan.
		std::vector<std::pair<uint32, uint32>> Chosen; // (tile, kind)
		for (usize S = 0; S < Best.size(); ++S)
		{
			if (Best[S].Tile != 0xffffffffu)
			{
				Chosen.push_back({Best[S].Tile, static_cast<uint32>(S % KindCount)});
			}
		}
		std::sort(Chosen.begin(), Chosen.end());
		uint32 Count = 0;
		for (const auto& [Tile, Kind] : Chosen)
		{
			if (DepositIx[Tile] != 0 || Count == 0xffff)
			{
				continue; // one deposit per tile: the first kind in kind order wins
			}
			const Candidate& Slot =
				Best[static_cast<usize>((static_cast<uint32>(Grid.CoordOf(Tile).Y) / P.Spacing) * CellsX +
										static_cast<uint32>(Grid.CoordOf(Tile).X) / P.Spacing) *
						 KindCount +
					 Kind];
			DepositInfo Info;
			Info.Index = ++Count;
			Info.Tile = Tile;
			Info.Kind = Kind;
			// Tier: the kind's base tier, raised by one for the richest deposits.
			Info.Tier = BaseTier[Kind] + (Slot.Richness > 950u && BaseTier[Kind] < RarityTiers ? 1u : 0u);
			Info.Richness = Slot.Richness > 1000u ? 1000u : Slot.Richness;
			Info.Region = RegionIx[Tile];
			DepositIx[Tile] = static_cast<uint16>(Info.Index);
			const EntityHandle H = W.CreateEntity(IdKind::ResourceDeposit);
			W.Components().GetPool(Types.Deposit).Add(H, Info);
		}
		return true;
	}

	DepositStats MeasureDeposits(const World& W, const WorldLayers& Layers, const DepositLayers& Deposits,
								 const DepositTypes& Types)
	{
		DepositStats S;
		const WorldMap& Map = W.Map();
		if (!Map.IsReady())
		{
			return S;
		}
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<uint16>& DepositIx = Map.GetLayer(Deposits.DepositIndex);
		for (uint32 I = 0; I < Map.Grid().TileCount(); ++I)
		{
			S.OnSea += (DepositIx[I] != 0 && (Terrain[I] & TerrainFlag::Land) == 0) ? 1u : 0u;
		}
		std::vector<uint8> RegionSeen;
		W.Components()
			.GetPool(Types.Deposit)
			.ForEach(
				[&](EntityHandle, const DepositInfo& D)
				{
					++S.Total;
					if (D.Kind < static_cast<uint32>(ResourceKind::Count))
					{
						++S.ByKind[D.Kind];
					}
					if (D.Tier >= 1 && D.Tier <= RarityTiers)
					{
						++S.ByTier[D.Tier];
					}
					if (D.Region != 0)
					{
						if (RegionSeen.size() <= D.Region)
						{
							RegionSeen.resize(D.Region + 1u, 0);
						}
						if (RegionSeen[D.Region] == 0)
						{
							RegionSeen[D.Region] = 1;
							++S.RegionsWithDeposits;
						}
					}
				});
		return S;
	}
} // namespace Vaelen::WorldGen
