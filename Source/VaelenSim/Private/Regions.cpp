// VAELEN - VaelenSim
// Regions stage implementation.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_Regions.cpp
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <queue>

namespace Vaelen::WorldGen
{
	RegionTypes RegionTypes::Declare(World& W)
	{
		RegionTypes T;
		T.Region = W.Types().Register<RegionInfo>("RegionInfo");
		W.Components().CreatePool(T.Region);
		return T;
	}

	RegionLayers RegionLayers::Declare(WorldMap& Map)
	{
		RegionLayers R;
		R.RegionIndex = Map.AddLayer<uint16>("region");
		return R;
	}

	RegionParams RegionParams::Resolve(const WorldGenConfig& Config) noexcept
	{
		RegionParams P;
		if (Config.Params[ParamIndex::RegionSpacing] != 0)
		{
			P.Spacing = Fix64::FromRaw(Config.Params[ParamIndex::RegionSpacing]);
		}
		if (Config.Params[ParamIndex::RegionMinTiles] > 0 && Config.Params[ParamIndex::RegionMinTiles] < 1000000)
		{
			P.MinTiles = static_cast<uint32>(Config.Params[ParamIndex::RegionMinTiles]);
		}
		if (Config.Params[ParamIndex::RegionSlopeCost] != 0)
		{
			P.SlopeCost = Fix64::FromRaw(Config.Params[ParamIndex::RegionSlopeCost]);
		}
		if (Config.Params[ParamIndex::RegionRiverCost] != 0)
		{
			P.RiverCost = Fix64::FromRaw(Config.Params[ParamIndex::RegionRiverCost]);
		}
		return P;
	}

	bool RegionGraph::AreAdjacent(uint16 A, uint16 B) const noexcept
	{
		if (A == 0 || B == 0 || A >= Neighbours.size())
		{
			return false;
		}
		return std::binary_search(Neighbours[A].begin(), Neighbours[A].end(), B);
	}

	namespace
	{
		struct GrowItem
		{
			int64 Cost;
			uint32 Index;
			uint16 Region;
		};
		struct GrowOrder
		{
			bool operator()(const GrowItem& A, const GrowItem& B) const noexcept
			{
				return A.Cost != B.Cost ? A.Cost > B.Cost : A.Index > B.Index;
			}
		};
	} // namespace

	bool GenerateRegions(World& W, const WorldLayers& Layers, const HydroLayers& Hydro, const RegionLayers& Regions,
						 const RegionTypes& Types)
	{
		WorldMap& Map = W.Map();
		if (!Map.IsReady())
		{
			VAELEN_CHECKF(false, "GenerateRegions: the map has no grid (call WorldMap::Reset first)");
			return false;
		}
		const WorldGrid& Grid = Map.Grid();
		const uint32 N = Grid.TileCount();
		const RegionParams P = RegionParams::Resolve(Map.Config());
		const TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<uint8>& BiomeLayer = Map.GetLayer(Layers.Biome);
		const TileLayer<uint16>& RiverIx = Map.GetLayer(Hydro.RiverIndex);
		const TileLayer<uint16>& LakeIx = Map.GetLayer(Hydro.LakeIndex);
		TileLayer<uint16>& RegionIx = Map.GetLayer(Regions.RegionIndex);
		auto IsLand = [&](uint32 I) { return (Terrain[I] & TerrainFlag::Land) != 0; };

		// Previous run.
		{
			std::vector<EntityHandle> Doomed;
			W.Components().GetPool(Types.Region).ForEach([&](EntityHandle H, RegionInfo&) { Doomed.push_back(H); });
			for (EntityHandle H : Doomed)
			{
				W.DestroyEntity(H);
			}
		}
		for (uint32 I = 0; I < N; ++I)
		{
			RegionIx[I] = 0;
		}

		// 1. Seeds on a jittered lattice: one per cell, the land tile nearest to
		//    the jittered cell centre (scan order breaks ties); then one for every
		//    landmass that received none.
		std::vector<uint32> Seeds;
		{
			const int32 SpacingTiles = (P.Spacing * Fix64::FromInt(static_cast<int32>(Grid.Width))).FloorToInt();
			const uint32 Step = SpacingTiles < 4 ? 4u : static_cast<uint32>(SpacingTiles);
			const uint64 SeedHash =
				Noise::LatticeHash(W.Config().Seed, static_cast<int32>(HashString("regions") & 0x7fffffff), 6);
			for (uint32 CY = 0; CY * Step < Grid.Height; ++CY)
			{
				for (uint32 CX = 0; CX * Step < Grid.Width; ++CX)
				{
					const uint64 H = Noise::LatticeHash(SeedHash, static_cast<int32>(CX), static_cast<int32>(CY));
					const uint32 JX = static_cast<uint32>(H % Step);
					const uint32 JY = static_cast<uint32>((H >> 32) % Step);
					const int32 TX = static_cast<int32>(CX * Step + JX);
					const int32 TY = static_cast<int32>(CY * Step + JY);
					// Nearest land tile to (TX, TY) within the cell, by squared distance then index.
					uint32 Best = 0xffffffffu;
					int64 BestD = 0;
					for (uint32 Y = CY * Step; Y < (CY + 1) * Step && Y < Grid.Height; ++Y)
					{
						for (uint32 X = CX * Step; X < (CX + 1) * Step && X < Grid.Width; ++X)
						{
							const uint32 I = Y * Grid.Width + X;
							if (!IsLand(I))
							{
								continue;
							}
							const int64 DX = static_cast<int64>(X) - TX;
							const int64 DY = static_cast<int64>(Y) - TY;
							const int64 D = DX * DX + DY * DY;
							if (Best == 0xffffffffu || D < BestD)
							{
								Best = I;
								BestD = D;
							}
						}
					}
					if (Best != 0xffffffffu)
					{
						Seeds.push_back(Best);
					}
				}
			}
			// Landmasses without a seed.
			std::vector<uint8> Reached(N, 0);
			std::vector<uint32> Stack;
			auto Flood = [&](uint32 Start)
			{
				Stack.clear();
				Stack.push_back(Start);
				Reached[Start] = 1;
				while (!Stack.empty())
				{
					const uint32 I = Stack.back();
					Stack.pop_back();
					Grid.ForEachNeighbour(Grid.CoordOf(I), 4,
										  [&](TileCoord NC, uint32)
										  {
											  const uint32 J = Grid.IndexOf(NC);
											  if (Reached[J] == 0 && IsLand(J))
											  {
												  Reached[J] = 1;
												  Stack.push_back(J);
											  }
										  });
				}
			};
			for (uint32 S : Seeds)
			{
				if (Reached[S] == 0)
				{
					Flood(S);
				}
			}
			for (uint32 I = 0; I < N; ++I)
			{
				if (IsLand(I) && Reached[I] == 0)
				{
					Seeds.push_back(I);
					Flood(I);
				}
			}
			std::sort(Seeds.begin(), Seeds.end());
			Seeds.erase(std::unique(Seeds.begin(), Seeds.end()), Seeds.end());
		}
		if (Seeds.size() > 0xfffe)
		{
			Seeds.resize(0xfffe);
		}

		// 2. Multi-source least-cost growth. Cost of stepping onto a tile:
		//    1 + SlopeCost * |dz| / 1000 + RiverCost if the tile is a river.
		{
			std::vector<int64> Cost(N, -1);
			std::priority_queue<GrowItem, std::vector<GrowItem>, GrowOrder> Queue;
			for (usize S = 0; S < Seeds.size(); ++S)
			{
				Cost[Seeds[S]] = 0;
				Queue.push({0, Seeds[S], static_cast<uint16>(S + 1)});
			}
			const Fix64 Thousandth = Fix64::One() / Fix64::FromInt(1000);
			while (!Queue.empty())
			{
				const GrowItem C = Queue.top();
				Queue.pop();
				if (RegionIx[C.Index] != 0)
				{
					continue; // settled by a cheaper path
				}
				RegionIx[C.Index] = C.Region;
				Grid.ForEachNeighbour(Grid.CoordOf(C.Index), 4,
									  [&](TileCoord NC, uint32)
									  {
										  const uint32 J = Grid.IndexOf(NC);
										  if (!IsLand(J) || RegionIx[J] != 0)
										  {
											  return;
										  }
										  const int64 DZ = Fix64::WrapSub(Elevation[J], Elevation[C.Index]);
										  const Fix64 Climb =
											  Fix64::FromRaw(DZ < 0 ? Fix64::WrapNeg(DZ) : DZ) * Thousandth;
										  Fix64 Step = Fix64::One() + Climb * P.SlopeCost;
										  if (RiverIx[J] != 0)
										  {
											  Step += P.RiverCost;
										  }
										  const int64 NewCost = C.Cost + Step.Raw;
										  if (Cost[J] < 0 || NewCost < Cost[J])
										  {
											  Cost[J] = NewCost;
											  Queue.push({NewCost, J, C.Region});
										  }
									  });
			}
		}

		// 3. Merge regions below the floor into the neighbour with the longest
		//    shared border (ties: lower index), smallest region first, until none
		//    is below the floor or no neighbour exists (an island smaller than
		//    the floor keeps its own region).
		{
			uint32 RegionCount = static_cast<uint32>(Seeds.size());
			std::vector<uint32> Size(RegionCount + 1, 0);
			for (uint32 I = 0; I < N; ++I)
			{
				++Size[RegionIx[I]];
			}
			Size[0] = 0;
			std::vector<uint16> Remap(RegionCount + 1);
			for (uint32 R = 0; R <= RegionCount; ++R)
			{
				Remap[R] = static_cast<uint16>(R);
			}
			auto Resolve = [&](uint16 R)
			{
				while (Remap[R] != R)
				{
					R = Remap[R];
				}
				return R;
			};
			while (true)
			{
				// Smallest region below the floor.
				uint16 Small = 0;
				for (uint16 R = 1; R <= RegionCount; ++R)
				{
					if (Resolve(R) == R && Size[R] > 0 && Size[R] < P.MinTiles && (Small == 0 || Size[R] < Size[Small]))
					{
						Small = R;
					}
				}
				if (Small == 0)
				{
					break;
				}
				// Border lengths with neighbours.
				std::vector<uint32> Border(RegionCount + 1, 0);
				for (uint32 I = 0; I < N; ++I)
				{
					if (Resolve(RegionIx[I]) != Small)
					{
						continue;
					}
					Grid.ForEachNeighbour(Grid.CoordOf(I), 4,
										  [&](TileCoord NC, uint32)
										  {
											  const uint16 Other = Resolve(RegionIx[Grid.IndexOf(NC)]);
											  if (Other != 0 && Other != Small)
											  {
												  ++Border[Other];
											  }
										  });
				}
				uint16 Target = 0;
				for (uint16 R = 1; R <= RegionCount; ++R)
				{
					if (Border[R] > 0 && (Target == 0 || Border[R] > Border[Target]))
					{
						Target = R;
					}
				}
				if (Target == 0)
				{
					Size[Small] = 0xffffffffu; // isolated: keep, never revisit
					continue;
				}
				Remap[Small] = Target;
				Size[Target] += Size[Small];
				Size[Small] = 0;
			}
			// Compact indices in increasing order of surviving seed index.
			std::vector<uint16> Final(RegionCount + 1, 0);
			uint16 Next = 0;
			for (uint16 R = 1; R <= RegionCount; ++R)
			{
				if (Resolve(R) == R && Size[R] != 0)
				{
					Final[R] = ++Next;
				}
			}
			for (uint32 I = 0; I < N; ++I)
			{
				RegionIx[I] = RegionIx[I] == 0 ? uint16{0} : Final[Resolve(RegionIx[I])];
			}
			RegionCount = Next;

			// 4. Entities with statistics.
			std::vector<RegionInfo> Info(RegionCount + 1);
			std::vector<uint64> SumX(RegionCount + 1, 0);
			std::vector<uint64> SumY(RegionCount + 1, 0);
			std::vector<int64> SumZ(RegionCount + 1, 0);
			std::vector<std::vector<uint32>> BiomeCounts(RegionCount + 1,
														 std::vector<uint32>(static_cast<uint32>(Biome::Count), 0));
			std::vector<uint32> SeedOf(RegionCount + 1, 0xffffffffu);
			for (usize S = 0; S < Seeds.size(); ++S)
			{
				const uint16 R = RegionIx[Seeds[S]];
				if (R != 0 && SeedOf[R] == 0xffffffffu)
				{
					SeedOf[R] = Seeds[S];
				}
			}
			for (uint32 I = 0; I < N; ++I)
			{
				const uint16 R = RegionIx[I];
				if (R == 0)
				{
					continue;
				}
				const TileCoord C = Grid.CoordOf(I);
				++Info[R].Tiles;
				SumX[R] += static_cast<uint32>(C.X);
				SumY[R] += static_cast<uint32>(C.Y);
				SumZ[R] += Elevation[I] >> 16; // keep the sum in range
				Info[R].CoastTiles += (Terrain[I] & TerrainFlag::Coast) != 0 ? 1u : 0u;
				Info[R].RiverTiles += RiverIx[I] != 0 ? 1u : 0u;
				Info[R].LakeTiles += LakeIx[I] != 0 ? 1u : 0u;
				const uint8 B = BiomeLayer[I] < static_cast<uint8>(Biome::Count) ? BiomeLayer[I] : 0;
				++BiomeCounts[R][B];
			}
			for (uint16 R = 1; R <= RegionCount; ++R)
			{
				RegionInfo& RI = Info[R];
				RI.Index = R;
				RI.SeedTile = SeedOf[R] == 0xffffffffu ? 0 : SeedOf[R];
				RI.MeanElevation = RI.Tiles == 0 ? 0 : (SumZ[R] / static_cast<int64>(RI.Tiles)) << 16;
				uint32 Dominant = 1;
				for (uint32 B = 1; B < static_cast<uint32>(Biome::Count); ++B)
				{
					if (BiomeCounts[R][B] > BiomeCounts[R][Dominant])
					{
						Dominant = B;
					}
				}
				RI.DominantBiome = Dominant;
				// Centroid: the region tile nearest to the mean position (scan order on ties).
				const int64 MX = RI.Tiles == 0 ? 0 : static_cast<int64>(SumX[R] / RI.Tiles);
				const int64 MY = RI.Tiles == 0 ? 0 : static_cast<int64>(SumY[R] / RI.Tiles);
				int64 BestD = -1;
				for (uint32 I = 0; I < N; ++I)
				{
					if (RegionIx[I] != R)
					{
						continue;
					}
					const TileCoord C = Grid.CoordOf(I);
					const int64 D = (C.X - MX) * (C.X - MX) + (C.Y - MY) * (C.Y - MY);
					if (BestD < 0 || D < BestD)
					{
						BestD = D;
						RI.CentroidTile = I;
					}
				}
				const EntityHandle H = W.CreateEntity(IdKind::Region);
				W.Components().GetPool(Types.Region).Add(H, RI);
			}
		}
		return true;
	}

	RegionGraph BuildRegionGraph(const WorldMap& Map, const RegionLayers& Regions)
	{
		RegionGraph G;
		if (!Map.IsReady())
		{
			return G;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint16>& RegionIx = Map.GetLayer(Regions.RegionIndex);
		uint16 Count = 0;
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			Count = RegionIx[I] > Count ? RegionIx[I] : Count;
		}
		G.Neighbours.assign(Count + 1u, {});
		G.SharedBorder.assign(Count + 1u, {});
		std::vector<std::vector<uint32>> Border(Count + 1u);
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			const uint16 R = RegionIx[I];
			if (R == 0)
			{
				continue;
			}
			// Only E and S edges: each 4-edge is counted once per direction pair.
			Grid.ForEachNeighbour(
				Grid.CoordOf(I), 4,
				[&](TileCoord NC, uint32 Slot)
				{
					if (Slot != 2 && Slot != 4)
					{
						return;
					}
					const uint16 O = RegionIx[Grid.IndexOf(NC)];
					if (O == 0 || O == R)
					{
						return;
					}
					for (const auto& Pair : {std::pair<uint16, uint16>{R, O}, std::pair<uint16, uint16>{O, R}})
					{
						std::vector<uint16>& List = G.Neighbours[Pair.first];
						const auto Pos = std::lower_bound(List.begin(), List.end(), Pair.second);
						const usize At = static_cast<usize>(Pos - List.begin());
						if (Pos == List.end() || *Pos != Pair.second)
						{
							List.insert(Pos, Pair.second);
							Border[Pair.first].insert(Border[Pair.first].begin() + static_cast<std::ptrdiff_t>(At), 1u);
						}
						else
						{
							++Border[Pair.first][At];
						}
					}
				});
		}
		G.SharedBorder = Border;
		return G;
	}

	RegionStats MeasureRegions(const World& W, const WorldLayers& Layers, const RegionLayers& Regions,
							   const RegionTypes& Types)
	{
		RegionStats S;
		const WorldMap& Map = W.Map();
		if (!Map.IsReady())
		{
			return S;
		}
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<uint16>& RegionIx = Map.GetLayer(Regions.RegionIndex);
		for (uint32 I = 0; I < Map.Grid().TileCount(); ++I)
		{
			const bool Land = (Terrain[I] & TerrainFlag::Land) != 0;
			S.UnassignedLand += (Land && RegionIx[I] == 0) ? 1u : 0u;
			S.AssignedSea += (!Land && RegionIx[I] != 0) ? 1u : 0u;
		}
		W.Components()
			.GetPool(Types.Region)
			.ForEach(
				[&](EntityHandle, const RegionInfo& R)
				{
					++S.Regions;
					S.SmallestTiles = S.SmallestTiles == 0 || R.Tiles < S.SmallestTiles ? R.Tiles : S.SmallestTiles;
					S.LargestTiles = R.Tiles > S.LargestTiles ? R.Tiles : S.LargestTiles;
				});
		const RegionGraph G = BuildRegionGraph(Map, Regions);
		for (const std::vector<uint16>& List : G.Neighbours)
		{
			S.MaxNeighbours =
				static_cast<uint32>(List.size()) > S.MaxNeighbours ? static_cast<uint32>(List.size()) : S.MaxNeighbours;
		}
		return S;
	}

	void ExportRegionAscii(const WorldMap& Map, const RegionLayers& Regions, uint32 Columns, std::string& Out)
	{
		Out.clear();
		if (!Map.IsReady() || Columns == 0)
		{
			return;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint16>& RegionIx = Map.GetLayer(Regions.RegionIndex);
		const uint32 Cols = Columns > Grid.Width ? Grid.Width : Columns;
		const uint32 CellW = Grid.Width / Cols;
		const uint32 CellH = CellW * 2 > Grid.Height ? Grid.Height : CellW * 2;
		const uint32 Rows = Grid.Height / CellH;
		static constexpr char Glyphs[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		for (uint32 R = 0; R < Rows; ++R)
		{
			for (uint32 C = 0; C < Cols; ++C)
			{
				// Majority region of the cell.
				uint16 Best = 0;
				uint32 BestCount = 0;
				std::vector<std::pair<uint16, uint32>> Counts;
				for (uint32 Y = R * CellH; Y < (R + 1) * CellH; ++Y)
				{
					for (uint32 X = C * CellW; X < (C + 1) * CellW; ++X)
					{
						const uint16 V = RegionIx[Y * Grid.Width + X];
						bool Found = false;
						for (auto& E : Counts)
						{
							if (E.first == V)
							{
								++E.second;
								Found = true;
								break;
							}
						}
						if (!Found)
						{
							Counts.push_back({V, 1});
						}
					}
				}
				for (const auto& E : Counts)
				{
					if (E.second > BestCount || (E.second == BestCount && E.first < Best))
					{
						Best = E.first;
						BestCount = E.second;
					}
				}
				Out.push_back(Best == 0 ? '~' : Glyphs[(Best - 1) % 62]);
			}
			Out.push_back('\n');
		}
	}
} // namespace Vaelen::WorldGen
