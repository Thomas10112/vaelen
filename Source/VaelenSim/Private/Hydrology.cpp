// VAELEN - VaelenSim
// Hydrology stage implementation.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_Hydrology.cpp
#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <queue>
#include <vector>

namespace Vaelen::WorldGen
{
	WorldTypes WorldTypes::Declare(World& W)
	{
		WorldTypes T;
		T.River = W.Types().Register<RiverInfo>("RiverInfo");
		T.Lake = W.Types().Register<LakeInfo>("LakeInfo");
		W.Components().CreatePool(T.River);
		W.Components().CreatePool(T.Lake);
		return T;
	}

	HydroLayers HydroLayers::Declare(WorldMap& Map)
	{
		HydroLayers H;
		H.Filled = Map.AddLayer<int64>("filled");
		H.FlowDirection = Map.AddLayer<uint8>("flowdir");
		H.Accumulation = Map.AddLayer<uint32>("flowacc");
		H.RiverIndex = Map.AddLayer<uint16>("river");
		H.LakeIndex = Map.AddLayer<uint16>("lake");
		return H;
	}

	HydrologyParams HydrologyParams::Resolve(const WorldGenConfig& Config) noexcept
	{
		HydrologyParams P;
		if (Config.Params[ParamIndex::RiverThreshold] != 0)
		{
			P.RiverThreshold = Fix64::FromRaw(Config.Params[ParamIndex::RiverThreshold]);
		}
		if (Config.Params[ParamIndex::MinRiverLength] > 0 && Config.Params[ParamIndex::MinRiverLength] < 100000)
		{
			P.MinRiverLength = static_cast<uint32>(Config.Params[ParamIndex::MinRiverLength]);
		}
		if (Config.Params[ParamIndex::LakeMinDepth] != 0)
		{
			P.LakeMinDepth = Fix64::FromRaw(Config.Params[ParamIndex::LakeMinDepth]);
		}
		if (Config.Params[ParamIndex::LakeMinTiles] > 0 && Config.Params[ParamIndex::LakeMinTiles] < 100000)
		{
			P.LakeMinTiles = static_cast<uint32>(Config.Params[ParamIndex::LakeMinTiles]);
		}
		return P;
	}

	namespace
	{
		struct FloodItem
		{
			int64 Level;
			uint32 Index;
		};
		/// Lowest level first; equal levels by lowest index (deterministic).
		struct FloodOrder
		{
			bool operator()(const FloodItem& A, const FloodItem& B) const noexcept
			{
				return A.Level != B.Level ? A.Level > B.Level : A.Index > B.Index;
			}
		};

		template <typename T>
		void DestroyAll(World& W, ComponentType<T> Type)
		{
			std::vector<EntityHandle> Doomed;
			W.Components().GetPool(Type).ForEach([&](EntityHandle H, T&) { Doomed.push_back(H); });
			for (EntityHandle H : Doomed)
			{
				W.DestroyEntity(H);
			}
		}
	} // namespace

	bool GenerateHydrology(World& W, const WorldLayers& Layers, const HydroLayers& Hydro, const WorldTypes& Types)
	{
		WorldMap& Map = W.Map();
		if (!Map.IsReady())
		{
			VAELEN_CHECKF(false, "GenerateHydrology: the map has no grid (call WorldMap::Reset first)");
			return false;
		}
		const WorldGrid& Grid = Map.Grid();
		const uint32 N = Grid.TileCount();
		const HydrologyParams P = HydrologyParams::Resolve(Map.Config());
		TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		TileLayer<int64>& Filled = Map.GetLayer(Hydro.Filled);
		TileLayer<uint8>& Flow = Map.GetLayer(Hydro.FlowDirection);
		TileLayer<uint32>& Acc = Map.GetLayer(Hydro.Accumulation);
		TileLayer<uint16>& RiverIx = Map.GetLayer(Hydro.RiverIndex);
		TileLayer<uint16>& LakeIx = Map.GetLayer(Hydro.LakeIndex);

		DestroyAll(W, Types.River);
		DestroyAll(W, Types.Lake);

		auto IsLand = [&](uint32 I) { return (Terrain[I] & TerrainFlag::Land) != 0; };

		// 1. Priority flood + epsilon from every sea tile: every land tile ends
		//    with a strictly lower neighbour towards the sea.
		{
			std::vector<uint8> Visited(N, 0);
			std::priority_queue<FloodItem, std::vector<FloodItem>, FloodOrder> Queue;
			for (uint32 I = 0; I < N; ++I)
			{
				Filled[I] = Elevation[I];
				if (!IsLand(I))
				{
					Visited[I] = 1;
					Queue.push({Elevation[I], I});
				}
			}
			if (Queue.empty())
			{
				// A world without sea: seed from the border so the flood still terminates.
				for (uint32 I = 0; I < N; ++I)
				{
					if ((Terrain[I] & TerrainFlag::Border) != 0)
					{
						Visited[I] = 1;
						Queue.push({Elevation[I], I});
					}
				}
			}
			while (!Queue.empty())
			{
				const FloodItem C = Queue.top();
				Queue.pop();
				Grid.ForEachNeighbour(Grid.CoordOf(C.Index), 8,
									  [&](TileCoord NC, uint32)
									  {
										  const uint32 J = Grid.IndexOf(NC);
										  if (Visited[J] != 0)
										  {
											  return;
										  }
										  Visited[J] = 1;
										  const int64 Raised = C.Level + 1; // epsilon: one raw unit (2^-32)
										  Filled[J] = Elevation[J] > Raised ? Elevation[J] : Raised;
										  Queue.push({Filled[J], J});
									  });
			}
		}

		// 2. D8 flow direction on the filled surface: steepest descent with the
		//    diagonal drop scaled by 181/256 (about 1/sqrt 2); ties keep the first slot.
		for (uint32 I = 0; I < N; ++I)
		{
			Flow[I] = FlowNone;
			if (!IsLand(I))
			{
				continue;
			}
			int64 BestScore = 0;
			uint8 Best = FlowNone;
			Grid.ForEachNeighbour(Grid.CoordOf(I), 8,
								  [&](TileCoord NC, uint32 Slot)
								  {
									  const uint32 J = Grid.IndexOf(NC);
									  const int64 Drop = Fix64::WrapSub(Filled[I], Filled[J]);
									  if (Drop <= 0)
									  {
										  return;
									  }
									  // Scores stay in range: drops are bounded by the elevation span.
									  const int64 Score = (Slot & 1u) != 0 ? (Drop >> 8) * 181 : (Drop >> 8) * 256;
									  if (Best == FlowNone || Score > BestScore)
									  {
										  BestScore = Score;
										  Best = static_cast<uint8>(Slot);
									  }
								  });
			Flow[I] = Best;
		}

		// 3. Accumulation in decreasing filled order (ties by index).
		{
			std::vector<uint32> Order;
			Order.reserve(N);
			for (uint32 I = 0; I < N; ++I)
			{
				Acc[I] = IsLand(I) ? 1u : 0u;
				if (IsLand(I))
				{
					Order.push_back(I);
				}
			}
			std::sort(Order.begin(), Order.end(),
					  [&](uint32 A, uint32 B) { return Filled[A] != Filled[B] ? Filled[A] > Filled[B] : A < B; });
			for (uint32 I : Order)
			{
				if (Flow[I] == FlowNone)
				{
					continue;
				}
				const TileCoord C = Grid.CoordOf(I);
				const uint32 J = Grid.IndexOf({C.X + NeighbourOffsets[Flow[I]].X, C.Y + NeighbourOffsets[Flow[I]].Y});
				if (IsLand(J))
				{
					Acc[J] += Acc[I];
				}
			}
		}

		// 4. Basins: 4-connected components of raised land tiles, in scan order.
		//    A basin whose deepest fill is below LakeMinDepth, or with fewer than
		//    LakeMinTiles tiles, is filled with sediment: its elevation rises to
		//    the water surface and it becomes a plain the rivers cross. Deeper
		//    basins are lakes (the elevation stays as the lake bed).
		uint32 LakeCount = 0;
		{
			const int64 MinDepth = P.LakeMinDepth.Raw;
			auto IsRaised = [&](uint32 I) { return IsLand(I) && Filled[I] > Elevation[I]; };
			std::vector<uint8> Seen(N, 0);
			for (uint32 I = 0; I < N; ++I)
			{
				LakeIx[I] = 0;
			}
			std::vector<uint32> Component;
			std::vector<uint32> Stack;
			for (uint32 Start = 0; Start < N; ++Start)
			{
				if (Seen[Start] != 0 || !IsRaised(Start))
				{
					continue;
				}
				Component.clear();
				Stack.clear();
				Stack.push_back(Start);
				Seen[Start] = 1;
				int64 MaxDepth = 0;
				while (!Stack.empty())
				{
					const uint32 I = Stack.back();
					Stack.pop_back();
					Component.push_back(I);
					const int64 Depth = Fix64::WrapSub(Filled[I], Elevation[I]);
					MaxDepth = Depth > MaxDepth ? Depth : MaxDepth;
					Grid.ForEachNeighbour(Grid.CoordOf(I), 4,
										  [&](TileCoord NC, uint32)
										  {
											  const uint32 J = Grid.IndexOf(NC);
											  if (Seen[J] == 0 && IsRaised(J))
											  {
												  Seen[J] = 1;
												  Stack.push_back(J);
											  }
										  });
				}
				if (MaxDepth < MinDepth || Component.size() < P.LakeMinTiles || LakeCount == 0xffff)
				{
					for (uint32 I : Component)
					{
						Elevation[I] = Filled[I]; // sediment fill
					}
					continue;
				}
				const uint16 Index = static_cast<uint16>(++LakeCount);
				LakeInfo Info;
				Info.Index = Index;
				Info.Surface = Filled[Start];
				Info.Tiles = static_cast<uint32>(Component.size());
				for (uint32 I : Component)
				{
					LakeIx[I] = Index;
					Info.Surface = Filled[I] < Info.Surface ? Filled[I] : Info.Surface;
				}
				// Outlet: the first tile (scan order) that a lake tile flows into outside the lake.
				for (uint32 I : Component)
				{
					if (Flow[I] == FlowNone)
					{
						continue;
					}
					const TileCoord C = Grid.CoordOf(I);
					const uint32 J =
						Grid.IndexOf({C.X + NeighbourOffsets[Flow[I]].X, C.Y + NeighbourOffsets[Flow[I]].Y});
					if (LakeIx[J] != Index)
					{
						Info.OutletTile = J;
						break;
					}
				}
				const EntityHandle H = W.CreateEntity(IdKind::Lake);
				W.Components().GetPool(Types.Lake).Add(H, Info);
			}
		}
		// The sediment fill changed the elevation: terrain flags and slope follow.
		ClassifyTerrain(Map, Layers);

		// 5. Rivers: tiles above the threshold outside lakes, traced from their
		//    sources in scan order; a trace ends at sea, at a lake, or where it
		//    meets an already assigned river.
		{
			const Fix64 ThresholdF = Fix64::FromInt(static_cast<int32>(N)) * P.RiverThreshold;
			const int32 ThresholdI = ThresholdF.FloorToInt();
			const uint32 Threshold = ThresholdI < 8 ? 8u : static_cast<uint32>(ThresholdI);
			std::vector<uint8> IsRiver(N, 0);
			for (uint32 I = 0; I < N; ++I)
			{
				RiverIx[I] = 0;
				IsRiver[I] = (IsLand(I) && LakeIx[I] == 0 && Acc[I] >= Threshold) ? 1 : 0;
			}
			auto Downstream = [&](uint32 I) -> uint32
			{
				const TileCoord C = Grid.CoordOf(I);
				return Grid.IndexOf({C.X + NeighbourOffsets[Flow[I]].X, C.Y + NeighbourOffsets[Flow[I]].Y});
			};
			// Upstream river count per tile.
			std::vector<uint8> HasUpstream(N, 0);
			for (uint32 I = 0; I < N; ++I)
			{
				if (IsRiver[I] != 0 && Flow[I] != FlowNone)
				{
					const uint32 J = Downstream(I);
					if (IsRiver[J] != 0)
					{
						HasUpstream[J] = 1;
					}
				}
			}
			uint32 RiverCount = 0;
			std::vector<uint32> Trace;
			for (uint32 Source = 0; Source < N; ++Source)
			{
				if (IsRiver[Source] == 0 || HasUpstream[Source] != 0 || RiverIx[Source] != 0)
				{
					continue;
				}
				Trace.clear();
				uint32 I = Source;
				uint32 Mouth = Source;
				while (true)
				{
					Trace.push_back(I);
					if (Flow[I] == FlowNone)
					{
						Mouth = I;
						break;
					}
					const uint32 J = Downstream(I);
					if (IsRiver[J] == 0 || RiverIx[J] != 0)
					{
						Mouth = J;
						break;
					}
					I = J;
				}
				if (Trace.size() < P.MinRiverLength || RiverCount == 0xffff)
				{
					continue;
				}
				const uint16 Index = static_cast<uint16>(++RiverCount);
				for (uint32 T : Trace)
				{
					RiverIx[T] = Index;
				}
				RiverInfo Info;
				Info.SourceTile = Source;
				Info.MouthTile = Mouth;
				Info.Length = static_cast<uint32>(Trace.size());
				Info.MouthFlow = Acc[Trace.back()];
				Info.Index = Index;
				const EntityHandle H = W.CreateEntity(IdKind::River);
				W.Components().GetPool(Types.River).Add(H, Info);
			}
		}
		return true;
	}

	HydrologyStats MeasureHydrology(const World& W, const WorldLayers& Layers, const HydroLayers& Hydro,
									const WorldTypes& Types)
	{
		HydrologyStats S;
		const WorldMap& Map = W.Map();
		if (!Map.IsReady())
		{
			return S;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<int64>& Elevation = Map.GetLayer(Layers.Elevation);
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<int64>& Filled = Map.GetLayer(Hydro.Filled);
		const TileLayer<uint32>& Acc = Map.GetLayer(Hydro.Accumulation);
		const TileLayer<uint16>& RiverIx = Map.GetLayer(Hydro.RiverIndex);
		const TileLayer<uint16>& LakeIx = Map.GetLayer(Hydro.LakeIndex);
		for (uint32 I = 0; I < Grid.TileCount(); ++I)
		{
			S.RiverTiles += RiverIx[I] != 0 ? 1u : 0u;
			S.LakeTiles += LakeIx[I] != 0 ? 1u : 0u;
			S.MaxAccumulation = Acc[I] > S.MaxAccumulation ? Acc[I] : S.MaxAccumulation;
			S.RaisedTiles += ((Terrain[I] & TerrainFlag::Land) != 0 && Filled[I] > Elevation[I]) ? 1u : 0u;
		}
		W.Components()
			.GetPool(Types.River)
			.ForEach(
				[&](EntityHandle, const RiverInfo& R)
				{
					++S.Rivers;
					S.LongestRiver = R.Length > S.LongestRiver ? R.Length : S.LongestRiver;
				});
		W.Components()
			.GetPool(Types.Lake)
			.ForEach(
				[&](EntityHandle, const LakeInfo& L)
				{
					++S.Lakes;
					S.LargestLake = L.Tiles > S.LargestLake ? L.Tiles : S.LargestLake;
				});
		return S;
	}

	uint32 StepsToSea(const WorldMap& Map, const WorldLayers& Layers, const HydroLayers& Hydro, uint32 Tile)
	{
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint8>& Terrain = Map.GetLayer(Layers.Terrain);
		const TileLayer<uint8>& Flow = Map.GetLayer(Hydro.FlowDirection);
		uint32 Steps = 0;
		uint32 I = Tile;
		while (Steps <= Grid.TileCount())
		{
			if ((Terrain[I] & TerrainFlag::Land) == 0)
			{
				return Steps;
			}
			if (Flow[I] == FlowNone)
			{
				return 0xffffffffu;
			}
			const TileCoord C = Grid.CoordOf(I);
			I = Grid.IndexOf({C.X + NeighbourOffsets[Flow[I]].X, C.Y + NeighbourOffsets[Flow[I]].Y});
			++Steps;
		}
		return 0xffffffffu;
	}

	void ExportHydroAscii(const WorldMap& Map, const WorldLayers& Layers, const HydroLayers& Hydro, uint32 Columns,
						  std::string& Out)
	{
		ExportAscii(Map, Layers, Columns, Out);
		if (Out.empty())
		{
			return;
		}
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint16>& RiverIx = Map.GetLayer(Hydro.RiverIndex);
		const TileLayer<uint16>& LakeIx = Map.GetLayer(Hydro.LakeIndex);
		const uint32 Cols = Columns > Grid.Width ? Grid.Width : Columns;
		const uint32 CellW = Grid.Width / Cols;
		const uint32 CellH = CellW * 2 > Grid.Height ? Grid.Height : CellW * 2;
		const uint32 Rows = Grid.Height / CellH;
		for (uint32 R = 0; R < Rows; ++R)
		{
			for (uint32 C = 0; C < Cols; ++C)
			{
				bool River = false;
				bool Lake = false;
				for (uint32 Y = R * CellH; Y < (R + 1) * CellH; ++Y)
				{
					for (uint32 X = C * CellW; X < (C + 1) * CellW; ++X)
					{
						River = River || RiverIx[Y * Grid.Width + X] != 0;
						Lake = Lake || LakeIx[Y * Grid.Width + X] != 0;
					}
				}
				if (Lake)
				{
					Out[static_cast<usize>(R) * (Cols + 1) + C] = 'o';
				}
				else if (River)
				{
					Out[static_cast<usize>(R) * (Cols + 1) + C] = '=';
				}
			}
		}
	}
} // namespace Vaelen::WorldGen
