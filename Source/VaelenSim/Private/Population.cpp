// VAELEN - VaelenSim
// Cultures and coarse population.
//
// STATUS: VALIDATED (Phase 03) - covered by Tests/Sim/Test_Population.cpp
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::History
{
	using WorldGen::Biome;
	using WorldGen::RegionInfo;

	// ── RegionPopulation ─────────────────────────────────────────────────────
	uint32 RegionPopulation::SlotOf(uint32 CultureIndex) const noexcept
	{
		for (uint32 S = 0; S < MaxCultures; ++S)
		{
			if (Culture[S] == CultureIndex && CultureIndex != 0)
			{
				return S;
			}
		}
		return MaxCultures;
	}

	bool RegionPopulation::Add(uint32 CultureIndex, uint32 People) noexcept
	{
		if (CultureIndex == 0 || People == 0)
		{
			return CultureIndex != 0;
		}
		uint32 S = SlotOf(CultureIndex);
		if (S == MaxCultures)
		{
			for (uint32 K = 0; K < MaxCultures; ++K)
			{
				if (Culture[K] == 0)
				{
					S = K;
					Culture[K] = CultureIndex;
					break;
				}
			}
			if (S == MaxCultures)
			{
				return false;
			}
		}
		const uint64 Sum = uint64{Count[S]} + People;
		Count[S] = Sum > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(Sum);
		Recount();
		return true;
	}

	uint32 RegionPopulation::Remove(uint32 CultureIndex, uint32 People) noexcept
	{
		const uint32 S = SlotOf(CultureIndex);
		if (S == MaxCultures)
		{
			return 0;
		}
		const uint32 Removed = People > Count[S] ? Count[S] : People;
		Count[S] -= Removed;
		if (Count[S] == 0)
		{
			Culture[S] = 0;
		}
		Recount();
		return Removed;
	}

	void RegionPopulation::Recount() noexcept
	{
		uint64 Sum = 0;
		uint32 Best = 0;
		uint32 BestCount = 0;
		for (uint32 S = 0; S < MaxCultures; ++S)
		{
			if (Culture[S] == 0)
			{
				Count[S] = 0;
				continue;
			}
			Sum += Count[S];
			if (Count[S] > BestCount || (Count[S] == BestCount && Culture[S] < Best))
			{
				Best = Culture[S];
				BestCount = Count[S];
			}
		}
		Total = Sum > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(Sum);
		Majority = BestCount == 0 ? 0 : Best;
	}

	PopulationTypes PopulationTypes::Declare(World& W)
	{
		PopulationTypes T;
		T.Culture = W.Types().Register<CultureInfo>("CultureInfo");
		T.Population = W.Types().Register<RegionPopulation>("RegionPopulation");
		W.Components().CreatePool(T.Culture);
		W.Components().CreatePool(T.Population);
		return T;
	}

	namespace
	{
		/// Region entity handles by index (index 0 unused), in a fresh vector.
		std::vector<EntityHandle> RegionHandles(World& W, const WorldGen::WorldSetup& Setup)
		{
			std::vector<EntityHandle> Handles;
			W.Components()
				.GetPool(Setup.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (Handles.size() <= R.Index)
						{
							Handles.resize(R.Index + 1u);
						}
						Handles[R.Index] = H;
					});
			return Handles;
		}

		uint32 ComputeCapacity(const World& W, const WorldGen::WorldSetup& Setup, const PopulationRules& Rules,
							   const RegionInfo& R)
		{
			const WorldMap& Map = W.Map();
			const TileLayer<uint16>& RegionIx = Map.GetLayer(Setup.Regions.RegionIndex);
			const TileLayer<uint8>& BiomeLayer = Map.GetLayer(Setup.Layers.Biome);
			const TileLayer<uint16>& RiverIx = Map.GetLayer(Setup.Hydro.RiverIndex);
			uint64 Capacity = 0;
			for (uint32 I = 0; I < Map.Grid().TileCount(); ++I)
			{
				if (RegionIx[I] != R.Index)
				{
					continue;
				}
				const uint8 B = BiomeLayer[I] < static_cast<uint8>(Biome::Count) ? BiomeLayer[I] : 0;
				Capacity += Rules.CapacityPerTile[B];
				Capacity += RiverIx[I] != 0 ? Rules.CapacityPerRiverTile : 0u;
			}
			W.Components()
				.GetPool(Setup.DepositTypes_.Deposit)
				.ForEach(
					[&](EntityHandle, const WorldGen::DepositInfo& D)
					{
						if (D.Region == R.Index && D.Kind == static_cast<uint32>(WorldGen::ResourceKind::FertileSoil))
						{
							Capacity += Rules.CapacityPerFertileDeposit * D.Richness / 1000u;
						}
					});
			return Capacity > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(Capacity);
		}

		uint32 FoundCulture(World& W, const PopulationTypes& Types, uint32 HomeRegion, uint32 Parent, SimTick Tick,
							uint32& Counter)
		{
			CultureInfo C;
			C.Index = ++Counter;
			C.HomeRegion = HomeRegion;
			C.Parent = Parent;
			C.Founded = Tick;
			C.Identity = Noise::LatticeHash(W.Config().Seed ^ 0x43554c54ull, static_cast<int32>(C.Index),
											static_cast<int32>(HomeRegion));
			if (Parent != 0)
			{
				W.Components()
					.GetPool(Types.Culture)
					.ForEach(
						[&](EntityHandle, const CultureInfo& P)
						{
							if (P.Index == Parent)
							{
								C.Generation = P.Generation + 1;
							}
						});
			}
			const EntityHandle H = W.CreateEntity(IdKind::Culture);
			W.Components().GetPool(Types.Culture).Add(H, C);
			return C.Index;
		}
	} // namespace

	uint32 SeedCultures(World& W, const WorldGen::WorldSetup& Setup, const PopulationTypes& Types,
						const PopulationRules& Rules, SimTick Tick)
	{
		if (W.Components().GetPool(Types.Population).Size() != 0)
		{
			VAELEN_CHECKF(false, "SeedCultures called twice");
			return 0;
		}
		// Capacity per region.
		std::vector<std::pair<uint32, uint32>> ByCapacity; // (capacity, region)
		const std::vector<EntityHandle> Handles = RegionHandles(W, Setup);
		W.Components()
			.GetPool(Setup.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const RegionInfo& R)
				{
					RegionPopulation P;
					P.Capacity = ComputeCapacity(W, Setup, Rules, R);
					W.Components().GetPool(Types.Population).Add(H, P);
					ByCapacity.push_back({P.Capacity, R.Index});
				});
		std::sort(ByCapacity.begin(), ByCapacity.end(), [](const auto& A, const auto& B)
				  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
		const WorldGen::RegionGraph Graph = WorldGen::BuildRegionGraph(W.Map(), Setup.Regions);
		std::vector<uint32> Chosen;
		uint32 Counter = 0;
		for (const auto& [Capacity, Region] : ByCapacity)
		{
			if (Chosen.size() >= Rules.SeedCultures || Capacity < Rules.SeedPeople)
			{
				break;
			}
			bool NearChosen = false;
			for (uint32 C : Chosen)
			{
				NearChosen = NearChosen || Graph.AreAdjacent(static_cast<uint16>(C), static_cast<uint16>(Region));
			}
			if (NearChosen)
			{
				continue;
			}
			Chosen.push_back(Region);
			const uint32 Culture = FoundCulture(W, Types, Region, 0, Tick, Counter);
			RegionPopulation& P = W.Components().GetPool(Types.Population).Get(Handles[Region]);
			P.Add(Culture, Rules.SeedPeople);
			P.SettledSince = 1;
			const PersistentId Subject = W.Entities().GetId(Handles[Region]);
			const PersistentId Founded =
				W.Events().Publish(Tick, CultureFoundedEvent, CulturePayload{Culture, Region, 0, 0}, Subject);
			W.Events().Publish(Tick, RegionSettledEvent, RegionPeople{Region, Culture, Rules.SeedPeople, 0}, Subject,
							   Founded);
		}
		return Counter;
	}

	void PopulationSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		auto IsDetailed = [&](EntityHandle E)
		{
			const RegionLod* L = HasLod ? W.Components().GetPool(Lod).TryGet(E) : nullptr;
			return L != nullptr && L->Level <= RegionLod::DetailedLevel;
		};
		// The region layer is a pure function of the world seed and the generation
		// config, so the config keys the derived graph cache.
		const Hash64 Digest = HashBytes(reinterpret_cast<const char*>(&W.Map().Config()), sizeof(WorldGenConfig));
		if (Digest != GraphDigest)
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Setup.Regions);
			GraphDigest = Digest;
		}
		const std::vector<EntityHandle> Handles = RegionHandles(W, Setup);
		uint32 CultureCounter = 0;
		std::vector<CultureInfo> Cultures;
		W.Components()
			.GetPool(Types.Culture)
			.ForEach(
				[&](EntityHandle, const CultureInfo& C)
				{
					Cultures.push_back(C);
					CultureCounter = C.Index > CultureCounter ? C.Index : CultureCounter;
				});
		auto HomeOf = [&](uint32 Culture) -> uint32
		{
			for (const CultureInfo& C : Cultures)
			{
				if (C.Index == Culture)
				{
					return C.HomeRegion;
				}
			}
			return 0;
		};
		// Graph distance from one region (BFS), small graphs.
		auto Distance = [&](uint32 From, uint32 To) -> uint32
		{
			if (From == To || From == 0 || To == 0 || From >= Graph.Neighbours.size() || To >= Graph.Neighbours.size())
			{
				return From == To ? 0u : 0xffffffffu;
			}
			std::vector<uint32> Dist(Graph.Neighbours.size(), 0xffffffffu);
			std::vector<uint32> Queue;
			Dist[From] = 0;
			Queue.push_back(From);
			for (usize Head = 0; Head < Queue.size(); ++Head)
			{
				const uint32 R = Queue[Head];
				for (uint16 N : Graph.Neighbours[R])
				{
					if (Dist[N] == 0xffffffffu)
					{
						Dist[N] = Dist[R] + 1;
						if (N == To)
						{
							return Dist[N];
						}
						Queue.push_back(N);
					}
				}
			}
			return 0xffffffffu;
		};

		struct Split
		{
			EntityHandle Region;
			uint32 RegionIndex;
			uint32 OldCulture;
			uint32 People;
		};
		std::vector<Split> Splits;
		std::vector<std::pair<EntityHandle, RegionPeople>> Abandoned;

		W.Components()
			.GetPool(Types.Population)
			.ForEach(
				[&](EntityHandle H, RegionPopulation& P)
				{
					const RegionInfo* R = W.Components().GetPool(Setup.RegionTypes_.Region).TryGet(H);
					if (R == nullptr || IsDetailed(H))
					{
						return;
					}
					if (P.Total == 0)
					{
						P.SettledSince = 0;
						return;
					}
					// Growth or decline, applied per culture slot proportionally.
					if (P.Total < P.Capacity)
					{
						const uint64 Room = P.Capacity - P.Total;
						const uint64 Growth = uint64{P.Total} * Rules.GrowthPerMille * Room / P.Capacity / 1000u;
						for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
						{
							if (P.Culture[S] != 0 && P.Count[S] > 0)
							{
								const uint64 Share = Growth * P.Count[S] / P.Total;
								P.Add(P.Culture[S], static_cast<uint32>(Share));
							}
						}
					}
					else if (P.Total > P.Capacity)
					{
						const uint64 Excess = P.Total - P.Capacity;
						const uint64 Deaths = Excess * Rules.DeclinePerMille / 1000u + 1u;
						const uint32 TotalBefore = P.Total;
						for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
						{
							if (P.Culture[S] != 0 && P.Count[S] > 0)
							{
								const uint64 Share = Deaths * P.Count[S] / TotalBefore;
								P.Remove(P.Culture[S], static_cast<uint32>(Share));
							}
						}
					}
					// Assimilation: minorities below the share join the majority.
					if (P.Majority != 0 && P.Total > 0)
					{
						const uint32 Threshold =
							static_cast<uint32>(uint64{P.Total} * Rules.AssimilationSharePerMille / 1000u);
						for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
						{
							if (P.Culture[S] != 0 && P.Culture[S] != P.Majority && P.Count[S] < Threshold)
							{
								const uint32 Moved = P.Count[S];
								P.Remove(P.Culture[S], Moved);
								P.Add(P.Majority, Moved);
							}
						}
					}
					if (P.Total == 0)
					{
						Abandoned.push_back({H, RegionPeople{R->Index, P.Majority, 0, 0}});
						P.SettledSince = 0;
						return;
					}
					P.SettledSince = P.SettledSince == 0xffffffffu ? P.SettledSince : P.SettledSince + 1;
					// Split: a settled majority far from its home founds a culture of its own.
					if (P.SettledSince >= Rules.SplitYears && P.Majority != 0)
					{
						const uint32 Home = HomeOf(P.Majority);
						if (Home != R->Index && Distance(Home, R->Index) >= Rules.SplitDistance &&
							Distance(Home, R->Index) != 0xffffffffu)
						{
							Splits.push_back({H, R->Index, P.Majority, P.Count[P.SlotOf(P.Majority)]});
						}
					}
				});
		for (const auto& [H, Payload] : Abandoned)
		{
			Context.Events->Publish(Context.Tick, RegionAbandonedEvent, Payload, W.Entities().GetId(H));
		}
		// One new culture per connected far component: a due region drags every
		// adjacent region of the same culture that is also far from home with it,
		// whether or not that region is due yet. The block's home is its lowest
		// region index.
		std::sort(Splits.begin(), Splits.end(),
				  [](const Split& A, const Split& B) { return A.RegionIndex < B.RegionIndex; });
		std::vector<uint8> Taken(Handles.size(), 0);
		for (const Split& Seed : Splits)
		{
			if (Seed.RegionIndex >= Taken.size() || Taken[Seed.RegionIndex] != 0)
			{
				continue;
			}
			const uint32 OldCulture = Seed.OldCulture;
			const uint32 OldHome = HomeOf(OldCulture);
			std::vector<uint32> Block;
			Block.push_back(Seed.RegionIndex);
			Taken[Seed.RegionIndex] = 1;
			for (usize Head = 0; Head < Block.size(); ++Head)
			{
				const uint32 A = Block[Head];
				if (A >= Graph.Neighbours.size())
				{
					continue;
				}
				for (uint16 N : Graph.Neighbours[A])
				{
					if (N >= Taken.size() || Taken[N] != 0)
					{
						continue;
					}
					const RegionPopulation* Q = W.Components().GetPool(Types.Population).TryGet(Handles[N]);
					if (Q == nullptr || Q->Majority != OldCulture || IsDetailed(Handles[N]))
					{
						continue;
					}
					const uint32 D = Distance(OldHome, N);
					if (D >= Rules.SplitDistance && D != 0xffffffffu)
					{
						Taken[N] = 1;
						Block.push_back(N);
					}
				}
			}
			uint32 HomeIndex = Block[0];
			for (uint32 R : Block)
			{
				HomeIndex = R < HomeIndex ? R : HomeIndex;
			}
			// Culture homes of one lineage stay at least SplitDistance apart: the
			// block joins the nearest sibling (same parent) whose home is closer
			// than that, ties to the lowest index; otherwise it founds a culture.
			uint32 NewCulture = 0;
			uint32 NearestSibling = 0xffffffffu;
			for (const CultureInfo& C : Cultures)
			{
				if (C.Parent != OldCulture)
				{
					continue;
				}
				const uint32 D = Distance(C.HomeRegion, HomeIndex);
				if (D < Rules.SplitDistance && (D < NearestSibling || (D == NearestSibling && C.Index < NewCulture)))
				{
					NearestSibling = D;
					NewCulture = C.Index;
				}
			}
			const bool Founded = NewCulture == 0;
			if (Founded)
			{
				NewCulture = FoundCulture(W, Types, HomeIndex, OldCulture, Context.Tick, CultureCounter);
				Cultures.push_back(CultureInfo{NewCulture, HomeIndex, OldCulture, 0, Context.Tick, 0});
			}
			for (uint32 R : Block)
			{
				RegionPopulation& P = W.Components().GetPool(Types.Population).Get(Handles[R]);
				const uint32 People = P.Count[P.SlotOf(OldCulture)];
				P.Remove(OldCulture, People);
				P.Add(NewCulture, People);
				P.SettledSince = 1;
			}
			if (Founded)
			{
				Context.Events->Publish(
					Context.Tick, CultureSplitEvent,
					CulturePayload{NewCulture, HomeIndex, OldCulture, static_cast<uint32>(Block.size())},
					W.Entities().GetId(Handles[HomeIndex]));
			}
		}
	}

	void MigrationSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		auto IsDetailed = [&](EntityHandle E)
		{
			const RegionLod* L = HasLod ? W.Components().GetPool(Lod).TryGet(E) : nullptr;
			return L != nullptr && L->Level <= RegionLod::DetailedLevel;
		};
		const ITileLayer* RegionLayer = W.Map().GetLayerBase(Setup.Regions.RegionIndex.Index);
		if (RegionLayer == nullptr)
		{
			return; // no region layer: nothing to migrate along
		}
		// The region layer is a pure function of the world seed and the generation
		// config, so the config is the cache key: hashing the layer itself costs a
		// pass over every tile per tick (2 MB at 1024).
		const Hash64 Digest = HashBytes(reinterpret_cast<const char*>(&W.Map().Config()), sizeof(WorldGenConfig));
		if (Digest != GraphDigest)
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Setup.Regions);
			GraphDigest = Digest;
		}
		const std::vector<EntityHandle> Handles = RegionHandles(W, Setup);
		struct Move
		{
			uint32 From;
			uint32 To;
			uint32 Culture;
			uint32 People;
		};
		std::vector<Move> Moves;
		// Decide every move on the state at the start of the tick, in region order.
		for (uint32 R = 1; R < Handles.size(); ++R)
		{
			const RegionPopulation* P = W.Components().GetPool(Types.Population).TryGet(Handles[R]);
			if (P == nullptr || P->Capacity == 0 || P->Total == 0 || IsDetailed(Handles[R]))
			{
				continue;
			}
			if (uint64{P->Total} * 1000u < uint64{P->Capacity} * Rules.MigrationThresholdPerMille)
			{
				continue;
			}
			// Least crowded neighbour (crowding = Total * 1000 / Capacity), ties by lower index.
			uint32 Best = 0;
			uint64 BestCrowding = 0;
			if (R < Graph.Neighbours.size())
			{
				for (uint16 N : Graph.Neighbours[R])
				{
					const RegionPopulation* Q = W.Components().GetPool(Types.Population).TryGet(Handles[N]);
					if (Q == nullptr || Q->Capacity == 0 || IsDetailed(Handles[N]))
					{
						continue;
					}
					const uint64 Crowding = uint64{Q->Total} * 1000u / Q->Capacity;
					if (Best == 0 || Crowding < BestCrowding)
					{
						Best = N;
						BestCrowding = Crowding;
					}
				}
			}
			if (Best == 0 || BestCrowding >= uint64{P->Total} * 1000u / P->Capacity)
			{
				continue; // nowhere better to go
			}
			const uint32 Wave = static_cast<uint32>(uint64{P->Total} * Rules.MigrationSharePerMille / 1000u);
			if (Wave < Rules.MinimumWave)
			{
				continue;
			}
			// The majority culture migrates.
			Moves.push_back({R, Best, P->Majority, Wave});
		}
		for (const Move& M : Moves)
		{
			RegionPopulation& From = W.Components().GetPool(Types.Population).Get(Handles[M.From]);
			RegionPopulation& To = W.Components().GetPool(Types.Population).Get(Handles[M.To]);
			const bool WasEmpty = To.Total == 0;
			const uint32 Moved = From.Remove(M.Culture, M.People);
			if (Moved == 0)
			{
				continue;
			}
			if (!To.Add(M.Culture, Moved))
			{
				From.Add(M.Culture, Moved); // no slot: the wave turns back (conservation)
				continue;
			}
			const PersistentId Subject = W.Entities().GetId(Handles[M.To]);
			const PersistentId Wave = Context.Events->Publish(Context.Tick, MigrationWaveEvent,
															  RegionPeople{M.To, M.Culture, Moved, M.From}, Subject);
			if (WasEmpty)
			{
				To.SettledSince = 1;
				Context.Events->Publish(Context.Tick, RegionSettledEvent, RegionPeople{M.To, M.Culture, Moved, M.From},
										Subject, Wave);
			}
		}
	}

	PopulationStats MeasurePopulation(const World& W, const PopulationTypes& Types)
	{
		PopulationStats S;
		std::vector<uint64> PerCulture;
		W.Components()
			.GetPool(Types.Population)
			.ForEach(
				[&](EntityHandle, const RegionPopulation& P)
				{
					++S.Regions;
					S.People += P.Total;
					S.Capacity += P.Capacity;
					S.SettledRegions += P.Total > 0 ? 1u : 0u;
					uint32 Here = 0;
					for (uint32 K = 0; K < RegionPopulation::MaxCultures; ++K)
					{
						if (P.Culture[K] != 0)
						{
							++Here;
							if (PerCulture.size() <= P.Culture[K])
							{
								PerCulture.resize(P.Culture[K] + 1u, 0);
							}
							PerCulture[P.Culture[K]] += P.Count[K];
						}
					}
					S.MaxCulturesInARegion = Here > S.MaxCulturesInARegion ? Here : S.MaxCulturesInARegion;
				});
		W.Components().GetPool(Types.Culture).ForEach([&](EntityHandle, const CultureInfo&) { ++S.Cultures; });
		for (usize C = 1; C < PerCulture.size(); ++C)
		{
			if (PerCulture[C] > S.LargestCulturePeople)
			{
				S.LargestCulturePeople = PerCulture[C];
				S.LargestCulture = static_cast<uint32>(C);
			}
		}
		return S;
	}

	void ExportCultureAscii(const World& W, const WorldGen::WorldSetup& Setup, const PopulationTypes& Types,
							uint32 Columns, std::string& Out)
	{
		WorldGen::ExportRegionAscii(W.Map(), Setup.Regions, Columns, Out);
		if (Out.empty())
		{
			return;
		}
		// Majority culture per region index.
		std::vector<uint32> Majority;
		W.Components()
			.GetPool(Types.Population)
			.ForEach(
				[&](EntityHandle H, const RegionPopulation& P)
				{
					const RegionInfo* R = W.Components().GetPool(Setup.RegionTypes_.Region).TryGet(H);
					if (R == nullptr)
					{
						return;
					}
					if (Majority.size() <= R->Index)
					{
						Majority.resize(R->Index + 1u, 0);
					}
					Majority[R->Index] = P.Majority;
				});
		// Re-render: the region picture used letters per region; map each cell's
		// region back through a second pass over the same downsampling.
		const WorldGrid& Grid = W.Map().Grid();
		const TileLayer<uint16>& RegionIx = W.Map().GetLayer(Setup.Regions.RegionIndex);
		const uint32 Cols = Columns > Grid.Width ? Grid.Width : Columns;
		const uint32 CellW = Grid.Width / Cols;
		const uint32 CellH = CellW * 2 > Grid.Height ? Grid.Height : CellW * 2;
		const uint32 Rows = Grid.Height / CellH;
		static constexpr char Glyphs[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
		for (uint32 Row = 0; Row < Rows; ++Row)
		{
			for (uint32 C = 0; C < Cols; ++C)
			{
				uint16 Best = 0;
				uint32 BestCount = 0;
				std::vector<std::pair<uint16, uint32>> Counts;
				for (uint32 Y = Row * CellH; Y < (Row + 1) * CellH; ++Y)
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
				char Glyph = '~';
				if (Best != 0)
				{
					const uint32 M = Best < Majority.size() ? Majority[Best] : 0;
					Glyph = M == 0 ? '.' : Glyphs[(M - 1) % 62];
				}
				Out[static_cast<usize>(Row) * (Cols + 1) + C] = Glyph;
			}
		}
	}
} // namespace Vaelen::History
