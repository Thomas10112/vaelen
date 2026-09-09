// VAELEN - VaelenInfrastructure
// Phase 09.01: buildings.
//
// STATUS: VALIDATED (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure

#include "Vaelen/Infrastructure/Buildings.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Infrastructure
{
	namespace
	{
		constexpr uint64 WorksSalt = 0x574f524b53ull; // "WORKS"
		constexpr uint32 G_GRAIN = static_cast<uint32>(Economy::Good::Grain);
		constexpr uint32 G_TOOLS = static_cast<uint32>(Economy::Good::Tools);
		constexpr uint32 G_TIMBER = static_cast<uint32>(Economy::Good::Timber);

		/// The sound part of a size: a half-ruined granary of four holds two.
		uint32 Sound(uint32 Size, uint32 Repair) noexcept
		{
			const uint64 Kept = (static_cast<uint64>(Size) * static_cast<uint64>(Repair)) / 1000u;
			return static_cast<uint32>(Kept);
		}
	} // namespace

	const char* WorkName(Work W) noexcept
	{
		switch (W)
		{
		case Work::Granary:
			return "granary";
		case Work::Mill:
			return "mill";
		case Work::Smithy:
			return "smithy";
		case Work::Wall:
			return "wall";
		case Work::Count:
			break;
		}
		return "work";
	}

	InfrastructureTypes InfrastructureTypes::Declare(World& W)
	{
		InfrastructureTypes T;
		T.Building = W.Types().Register<BuildingInfo>("BuildingInfo");
		T.Works = W.Types().Register<RegionWorks>("RegionWorks");
		W.Components().CreatePool(T.Building);
		W.Components().CreatePool(T.Works);
		return T;
	}

	void BuildingSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;

		// 1. The regions, by index.
		std::vector<EntityHandle> RegionHandles;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= RegionHandles.size())
					{
						RegionHandles.resize(usize{R.Index} + 1u);
					}
					RegionHandles[R.Index] = H;
				});
		const usize N = RegionHandles.size();
		if (N <= 1)
		{
			return;
		}

		// 2. Where a polity sits, and who rules what.
		std::vector<uint8> IsSeat(N, 0u);
		std::vector<uint32> RuledBy(N, 0u);
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle, const Politics::PolityInfo& P)
				{
					if (P.Dissolved == 0 && P.Seat != 0 && P.Seat < N)
					{
						IsSeat[P.Seat] = 1u;
					}
				});
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const Politics::RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
		}

		// 3. What already stands, by region and by kind. A region keeps at most
		//    one work of a kind, so this is a plain table.
		std::vector<EntityHandle> Standing(N * WorkCount);
		std::vector<uint8> Ruined(N * WorkCount, 0u); // ground that already holds a ruin of the kind
		uint32 Highest = 0;
		{
			std::vector<std::pair<uint32, EntityHandle>> All;
			W.Components()
				.GetPool(Works.Building)
				.ForEach([&](EntityHandle H, const BuildingInfo& B) { All.push_back({B.Index, H}); });
			std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
			for (const auto& Entry : All)
			{
				Highest = std::max(Highest, Entry.first);
				const BuildingInfo* B = W.Components().GetPool(Works.Building).TryGet(Entry.second);
				if (B == nullptr || B->Region == 0 || B->Region >= N || B->Kind >= WorkCount)
				{
					continue;
				}
				if (B->Fell != 0)
				{
					Ruined[usize{B->Region} * WorkCount + B->Kind] = 1u;
					continue;
				}
				EntityHandle& Slot = Standing[usize{B->Region} * WorkCount + B->Kind];
				if (Slot.IsNull())
				{
					Slot = Entry.second;
				}
			}
		}

		// 4. What every region raises this year: at most one thing, the first
		//    kind it wants, can pay for and has hands for.
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const History::RegionPopulation* Counts =
				W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[R]);
			const uint64 People = Counts != nullptr ? Counts->Total : 0u;
			if (People < Rules.PeopleToBuild)
			{
				continue;
			}
			Economy::RegionStock* Stock = W.Components().GetPool(Economy.Region).TryGet(RegionHandles[R]);
			if (Stock == nullptr)
			{
				continue;
			}
			const uint64 Workers = (People * Rules.WorkerSharePerMille) / 1000u;
			uint64 Spare = (Workers * Rules.SparePerMille) / 1000u;
			const uint32 Want =
				std::min(static_cast<uint32>(People / std::max<uint32>(Rules.PeoplePerSize, 1u)), Rules.MostOfAKind);
			if (Want == 0)
			{
				continue;
			}
			uint32 Done = 0;
			for (uint32 K = 0; K < WorkCount && Done < Rules.RaisedPerRegionPerYear; ++K)
			{
				if (K == static_cast<uint32>(Work::Wall) && Rules.WallsAtSeatsOnly != 0 && IsSeat[R] == 0)
				{
					continue;
				}
				if (Spare < Rules.HandsPerSize)
				{
					break; // nobody can leave the fields this year
				}
				EntityHandle Have = Standing[usize{R} * WorkCount + K];
				const BuildingInfo* Held =
					Have.IsNull() ? nullptr : W.Components().GetPool(Works.Building).TryGet(Have);
				const uint32 Size = Held != nullptr ? Held->Size : 0u;
				if (Size >= Want)
				{
					continue;
				}
				// Ground that already holds a ruin of the kind is cheaper to build
				// on: the stone is there (09.05). Only for raising a work anew -
				// enlarging a standing one has no ruin to stand on.
				const bool OnARuin =
					Held == nullptr && Rules.RebuildPerMille > 0 && Ruined[usize{R} * WorkCount + K] != 0;
				const uint32 Spared = OnARuin ? std::min<uint32>(1000u, Rules.RebuildPerMille) : 0u;
				auto Cost = [&](uint32 Full) { return Full - Full * Spared / 1000u; };
				const uint32 NeedTimber = Cost(Rules.TimberPerSize);
				const uint32 NeedTools = Cost(Rules.ToolsPerSize);
				const uint32 NeedGrain = Cost(Rules.GrainPerSize);
				if (Stock->Amount[G_TIMBER] < NeedTimber || Stock->Amount[G_TOOLS] < NeedTools ||
					Stock->Amount[G_GRAIN] < NeedGrain)
				{
					continue; // it cannot pay for this one; it may afford a cheaper year
				}

				// The thing first, then what it cost, so that every unit taken
				// names the raising that took it.
				PersistentId Cause;
				if (Held == nullptr)
				{
					BuildingInfo Fresh;
					Fresh.Index = ++Highest;
					Fresh.Kind = K;
					Fresh.Region = R;
					Fresh.Polity = RuledBy[R];
					Fresh.Size = 1;
					Fresh.Repair = 1000;
					Fresh.Timber = NeedTimber;
					Fresh.Tools = NeedTools;
					Fresh.Grain = NeedGrain;
					Fresh.Hands = Rules.HandsPerSize;
					Fresh.Raised = Context.Tick;
					Fresh.Identity = Noise::LatticeHash(W.Config().Seed ^ WorksSalt, static_cast<int32>(Fresh.Index),
														static_cast<int32>(R));
					const EntityHandle BH = W.CreateEntity(IdKind::Building);
					W.Components().GetPool(Works.Building).Add(BH, Fresh);
					Standing[usize{R} * WorkCount + K] = BH;
					Cause =
						Context.Events->Publish(Context.Tick, BuildingRaisedEvent,
												WorksPayload{R, Fresh.Index, K, Fresh.Size}, W.Entities().GetId(BH));
				}
				else
				{
					BuildingInfo* Grown = W.Components().GetPool(Works.Building).TryGet(Have);
					if (Grown == nullptr)
					{
						continue; // it was standing a moment ago; there is nothing to enlarge
					}
					Grown->Size += 1;
					Grown->Timber += NeedTimber;
					Grown->Tools += NeedTools;
					Grown->Grain += NeedGrain;
					Grown->Hands += Rules.HandsPerSize;
					Cause = Context.Events->Publish(Context.Tick, BuildingEnlargedEvent,
													WorksPayload{R, Grown->Index, K, Grown->Size},
													W.Entities().GetId(Have));
				}

				const int32 Timber = -static_cast<int32>(NeedTimber);
				const int32 Tools = -static_cast<int32>(NeedTools);
				const int32 Grain = -static_cast<int32>(NeedGrain);
				VAELEN_ENSURE(Economy::AddStock(W, Types, Families, Economy, R, 0, Economy::Good::Timber, Timber,
												Context.Tick, Cause) == NeedTimber);
				VAELEN_ENSURE(Economy::AddStock(W, Types, Families, Economy, R, 0, Economy::Good::Tools, Tools,
												Context.Tick, Cause) == NeedTools);
				VAELEN_ENSURE(Economy::AddStock(W, Types, Families, Economy, R, 0, Economy::Good::Grain, Grain,
												Context.Tick, Cause) == NeedGrain);
				// AddStock may have moved the pool; ask for the stock again.
				Stock = W.Components().GetPool(Economy.Region).TryGet(RegionHandles[R]);
				VAELEN_ENSURE(Stock != nullptr);
				Spare -= Rules.HandsPerSize;
				++Done;
			}
		}

		// 5. What every region now holds, written where the layers below can read it.
		std::vector<RegionWorks> Summary(N);
		std::vector<uint8> Touched(N, 0u);
		{
			std::vector<uint32> Order;
			std::vector<EntityHandle> Handles;
			W.Components()
				.GetPool(Works.Building)
				.ForEach(
					[&](EntityHandle H, const BuildingInfo& B)
					{
						Order.push_back(B.Index);
						Handles.push_back(H);
					});
			std::vector<uint32> Sorted(Order.size());
			for (uint32 I = 0; I < Order.size(); ++I)
			{
				Sorted[I] = I;
			}
			std::sort(Sorted.begin(), Sorted.end(), [&](uint32 A, uint32 B) { return Order[A] < Order[B]; });
			for (const uint32 I : Sorted)
			{
				const BuildingInfo* B = W.Components().GetPool(Works.Building).TryGet(Handles[I]);
				if (B == nullptr || B->Region == 0 || B->Region >= N || B->Kind >= WorkCount)
				{
					continue;
				}
				Touched[B->Region] = 1u;
				RegionWorks& Sum = Summary[B->Region];
				if (B->Fell != 0)
				{
					++Sum.Ruins;
					continue;
				}
				++Sum.Standing;
				Sum.Kept[B->Kind] += Sound(B->Size, B->Repair);
			}
		}
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			RegionWorks* Held = W.Components().GetPool(Works.Works).TryGet(RegionHandles[R]);
			if (Held == nullptr)
			{
				if (Touched[R] == 0)
				{
					continue; // a region that never built anything carries nothing
				}
				W.Components().GetPool(Works.Works).Add(RegionHandles[R], Summary[R]);
				continue;
			}
			*Held = Summary[R];
		}
	}

	const BuildingInfo* BuildingOf(const World& W, const InfrastructureTypes& Works, uint32 Building)
	{
		const BuildingInfo* Found = nullptr;
		if (Building == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Works.Building)
			.ForEach(
				[&](EntityHandle, const BuildingInfo& B)
				{
					if (B.Index == Building && Found == nullptr)
					{
						Found = &B;
					}
				});
		return Found;
	}

	void BuildingsIn(const World& W, const InfrastructureTypes& Works, uint32 Region, std::vector<uint32>& Out)
	{
		Out.clear();
		if (Region == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Works.Building)
			.ForEach(
				[&](EntityHandle, const BuildingInfo& B)
				{
					if (B.Region == Region)
					{
						Out.push_back(B.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	const RegionWorks* WorksOf(const World& W, const History::PreHistoryTypes& Types, const InfrastructureTypes& Works,
							   uint32 Region)
	{
		const RegionWorks* Found = nullptr;
		if (Region == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Works.Works).TryGet(H);
					}
				});
		return Found;
	}

	uint32 KeptSize(const World& W, const History::PreHistoryTypes& Types, const InfrastructureTypes& Works,
					uint32 Region, Work Kind)
	{
		const uint32 K = static_cast<uint32>(Kind);
		if (K >= WorkCount)
		{
			return 0;
		}
		const RegionWorks* Held = WorksOf(W, Types, Works, Region);
		return Held != nullptr ? Held->Kept[K] : 0u;
	}

	BuildingStats MeasureBuildings(const World& W, const History::PreHistoryTypes& Types,
								   const InfrastructureTypes& Works, const BuildingRules& Rules)
	{
		BuildingStats S;
		(void)Rules;
		std::vector<uint32> KnownRegions;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach([&](EntityHandle, const WorldGen::RegionInfo& R) { KnownRegions.push_back(R.Index); });
		std::sort(KnownRegions.begin(), KnownRegions.end());
		auto Known = [&](uint32 R)
		{ return R != 0 && std::binary_search(KnownRegions.begin(), KnownRegions.end(), R); };
		const uint32 Widest = KnownRegions.empty() ? 0u : KnownRegions.back() + 1u;

		std::vector<BuildingInfo> All;
		W.Components().GetPool(Works.Building).ForEach([&](EntityHandle, const BuildingInfo& B) { All.push_back(B); });
		std::sort(All.begin(), All.end(),
				  [](const BuildingInfo& A, const BuildingInfo& B) { return A.Index < B.Index; });

		// What the buildings say every region holds, to check the summaries against.
		std::vector<RegionWorks> Truth(Widest);
		std::vector<uint8> HasKind(usize{Widest} * WorkCount, 0u);

		Hash64 D = HashString("Buildings");
		uint32 Previous = 0;
		for (const BuildingInfo& B : All)
		{
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&B), sizeof(B)));
			if (B.Index == 0 || B.Index == Previous)
			{
				++S.Bad; // an index used twice, or none at all
			}
			Previous = B.Index;
			if (!Known(B.Region) || B.Kind >= WorkCount || B.Repair > 1000)
			{
				++S.Bad;
				continue;
			}
			S.Timber += B.Timber;
			S.Tools += B.Tools;
			S.Grain += B.Grain;
			S.Hands += B.Hands;
			if (B.Fell != 0)
			{
				++S.Ruined;
				++Truth[B.Region].Ruins;
				continue;
			}
			if (B.Size == 0)
			{
				++S.Bad; // a building standing that is not there
				continue;
			}
			uint8& Seen = HasKind[usize{B.Region} * WorkCount + B.Kind];
			if (Seen != 0)
			{
				++S.Bad; // two works of one kind on one region
			}
			Seen = 1u;
			++S.Standing;
			++S.Of[B.Kind];
			S.Size[B.Kind] += B.Size;
			++Truth[B.Region].Standing;
			Truth[B.Region].Kept[B.Kind] += Sound(B.Size, B.Repair);
		}
		for (uint32 R = 1; R < Widest; ++R)
		{
			if (Truth[R].Standing != 0)
			{
				++S.Regions;
			}
		}

		// Every summary must say what the buildings on that region say.
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionWorks* Held = W.Components().GetPool(Works.Works).TryGet(H);
					if (Held == nullptr)
					{
						return;
					}
					const RegionWorks& Says = R.Index < Widest ? Truth[R.Index] : RegionWorks{};
					if (Held->Standing != Says.Standing || Held->Ruins != Says.Ruins)
					{
						++S.Bad;
						return;
					}
					for (uint32 K = 0; K < WorkCount; ++K)
					{
						if (Held->Kept[K] != Says.Kept[K])
						{
							++S.Bad;
							return;
						}
					}
				});

		for (const Event& E : W.Log().All())
		{
			S.Raised += E.Is(BuildingRaisedEvent) ? 1u : 0u;
			S.Enlarged += E.Is(BuildingEnlargedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Infrastructure
