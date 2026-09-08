// VAELEN - VaelenInfrastructure
// Phase 09.06: logistics.
//
// STATUS: PROTOTYPE (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure

#include "Vaelen/Infrastructure/Logistics.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Infrastructure
{
	namespace
	{
		/// What the best road touching a region is worth to whatever walks it.
		void BestByRegion(const World& W, const Economy::TradeTypes& Trade, const RoadTypes& Roads,
						  const RoadRules& RoadRules_, const LogisticsRules& Rules, usize N, std::vector<uint32>& Out)
		{
			Out.assign(N, 0u);
			W.Components()
				.GetPool(Trade.Route)
				.ForEach(
					[&](EntityHandle H, const Economy::RouteInfo&)
					{
						const RoadInfo* Road = W.Components().GetPool(Roads.Road).TryGet(H);
						if (Road == nullptr || Road->Grade == 0)
						{
							return;
						}
						const uint64 Raw = uint64{RoadWorth(*Road, RoadRules_)} * Rules.SharePerMille / 1000u;
						const uint32 Worth = static_cast<uint32>(std::min<uint64>(Rules.MostEase, Raw));
						for (const uint32 End : {Road->From, Road->To})
						{
							if (End != 0 && End < N)
							{
								Out[End] = std::max(Out[End], Worth);
							}
						}
					});
		}
	} // namespace

	LogisticsTypes LogisticsTypes::Declare(World& W)
	{
		LogisticsTypes T;
		T.Ways = W.Types().Register<Politics::RegionWays>("RegionWays");
		W.Components().CreatePool(T.Ways);
		return T;
	}

	void LogisticsSystem::Tick(TickContext& Context)
	{
		(void)Context;
		World& W = *Owner;
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
		std::vector<uint32> Best;
		BestByRegion(W, Trade, Roads, RoadRules_, Rules, N, Best);

		// Written every year from what stands, never added to, so that a road
		// falling back to a track (09.04) takes its worth with it the same year.
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			Politics::RegionWays* Held = W.Components().GetPool(Ways.Ways).TryGet(RegionHandles[R]);
			if (Held != nullptr)
			{
				Held->EasePerMille = Best[R];
			}
			else if (Best[R] != 0)
			{
				W.Components().GetPool(Ways.Ways).Add(RegionHandles[R], Politics::RegionWays{Best[R], 0u});
			}
		}
	}

	uint32 EaseOf(const World& W, const History::PreHistoryTypes& Types, const LogisticsTypes& Ways, uint32 Region)
	{
		uint32 Out = 0;
		if (Region == 0)
		{
			return 0;
		}
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index != Region)
					{
						return;
					}
					const Politics::RegionWays* Held = W.Components().GetPool(Ways.Ways).TryGet(H);
					Out = Held != nullptr ? Held->EasePerMille : 0u;
				});
		return Out;
	}

	LogisticsStats MeasureLogistics(const World& W, const History::PreHistoryTypes& Types,
									const Economy::TradeTypes& Trade, const RoadTypes& Roads,
									const LogisticsTypes& Ways, const RoadRules& RoadRules_,
									const LogisticsRules& Rules)
	{
		LogisticsStats S;
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
		std::vector<uint32> Best;
		BestByRegion(W, Trade, Roads, RoadRules_, Rules, N, Best);

		Hash64 D = HashString("Logistics");
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const Politics::RegionWays* Held = W.Components().GetPool(Ways.Ways).TryGet(RegionHandles[R]);
			const uint32 Says = Held != nullptr ? Held->EasePerMille : 0u;
			if (Held != nullptr)
			{
				D = HashCombine(D, HashCombine(HashUInt64(R), HashUInt64(Says)));
			}
			if (Says != Best[R])
			{
				++S.Bad; // what the ground says is not what the roads on it are worth
			}
			if (Says > Rules.MostEase)
			{
				++S.Bad;
			}
			if (Says != 0)
			{
				++S.Served;
				S.Total += Says;
				S.Best = std::max(S.Best, Says);
			}
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Infrastructure
