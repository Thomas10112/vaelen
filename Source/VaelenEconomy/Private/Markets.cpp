// VAELEN - VaelenEconomy
// Phase 06.03: markets and prices.
//
// STATUS: VALIDATED (Phase 06) - unit/distribution/deterministic tests in Tests/Economy

#include "Vaelen/Economy/Markets.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Economy
{
	MarketTypes MarketTypes::Declare(World& W)
	{
		MarketTypes T;
		T.Market = W.Types().Register<RegionMarket>("RegionMarket");
		W.Components().CreatePool(T.Market);
		return T;
	}

	uint64 WantedStock(const ProductionRules& C, const MarketRules& Rules, Good G, uint64 People) noexcept
	{
		auto Per = [&](uint32 Persons) { return Persons > 0 ? People / Persons : 0u; };
		switch (G)
		{
		case Good::Grain:
			return People * C.GrainPerPerson * Rules.GrainYearsWanted;
		case Good::Cloth:
			return Per(C.ClothWearPerPersons) * Rules.GoodsYearsWanted + 1u;
		case Good::Tools:
			return Per(C.ToolsWearPerPersons) * Rules.GoodsYearsWanted + 1u;
		case Good::Ore:
			return Per(C.ToolsPerPersons) * Rules.GoodsYearsWanted + 1u;
		case Good::Timber:
			return Per(C.TimberPerPersons) * Rules.GoodsYearsWanted + 1u;
		case Good::Salt:
			return Per(C.SaltPerPersons) * Rules.GoodsYearsWanted + 1u;
		case Good::Luxuries:
			return Per(Rules.LuxuryPerPersons) + 1u;
		case Good::Count:
		default:
			return 0;
		}
	}

	uint32 PriceFor(const MarketRules& Rules, Good G, uint64 Wanted, uint64 Held) noexcept
	{
		const uint32 g = static_cast<uint32>(G);
		if (g >= GoodCount)
		{
			return 0;
		}
		const uint64 Base = Rules.BasePrice[g];
		const uint64 Floor = std::max<uint64>(1u, Base * Rules.FloorPerMille / 1000u);
		const uint64 Ceiling = std::max<uint64>(Floor, Base * Rules.CeilingPerMille / 1000u);
		if (Wanted == 0)
		{
			return static_cast<uint32>(Floor);
		}
		const uint64 Raw = Held == 0 ? Ceiling : Base * Wanted / Held;
		return static_cast<uint32>(std::min(Ceiling, std::max(Floor, Raw)));
	}

	uint64 ValueOf(const uint32 Amount[GoodCount], const RegionMarket& Market) noexcept
	{
		uint64 Sum = 0;
		for (uint32 g = 0; g < GoodCount; ++g)
		{
			Sum += uint64{Amount[g]} * Market.Price[g];
		}
		return Sum;
	}

	void MarketSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
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
		// The living of the detailed regions, and the houses' stocks by region.
		std::vector<uint8> Detailed(N, 0u);
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach(
				[&](EntityHandle, const Population::RegionDetail& D)
				{
					if (D.Region < N)
					{
						Detailed[D.Region] = 1;
					}
				});
		std::vector<uint32> Alive(N, 0u);
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& P)
				{
					if (P.Region < N && P.State == static_cast<uint8>(Population::LifeState::Alive))
					{
						++Alive[P.Region];
					}
				});
		std::vector<uint64> Held(N * GoodCount, 0u);
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					const HouseStock* S = F.Region < N ? W.Components().GetPool(Economy.House).TryGet(H) : nullptr;
					if (S == nullptr)
					{
						return;
					}
					for (uint32 g = 0; g < GoodCount; ++g)
					{
						Held[usize{F.Region} * GoodCount + g] += S->Amount[g];
					}
				});
		// This year's harvests, the cause of a grain price.
		std::vector<PersistentId> Harvests(N);
		const std::vector<Event>& Events = W.Log().All();
		for (usize i = Events.size(); i > 0; --i)
		{
			const Event& E = Events[i - 1];
			if (E.Tick != Context.Tick)
			{
				break;
			}
			if (E.Is(HarvestEvent))
			{
				const StockPayload P = E.Get<StockPayload>();
				if (P.Region < N && !Harvests[P.Region].IsValid())
				{
					Harvests[P.Region] = E.Id;
				}
			}
		}
		for (uint32 Region = 1; Region < N; ++Region)
		{
			const EntityHandle RH = RegionHandles[Region];
			const RegionStock* Common = RH.IsNull() ? nullptr : W.Components().GetPool(Economy.Region).TryGet(RH);
			if (Common == nullptr)
			{
				continue;
			}
			const History::RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			const uint64 People = Detailed[Region] != 0 ? Alive[Region] : (Counts != nullptr ? Counts->Total : 0u);
			RegionMarket Fresh;
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				const uint64 Stock = uint64{Common->Amount[g]} + Held[usize{Region} * GoodCount + g];
				Fresh.Price[g] = PriceFor(Rules, static_cast<Good>(g),
										  WantedStock(Consumption, Rules, static_cast<Good>(g), People), Stock);
			}
			RegionMarket* Market = W.Components().GetPool(Markets.Market).TryGet(RH);
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				const uint32 Old = Market != nullptr ? Market->Price[g] : 0u;
				const uint32 New = Fresh.Price[g];
				const uint32 Gap = New > Old ? New - Old : Old - New;
				if (Market == nullptr || uint64{Gap} * 1000u >= uint64{Old} * Rules.ChangePerMille)
				{
					Context.Events->Publish(Context.Tick, PriceChangedEvent, StockPayload{Region, 0, g, New},
											W.Entities().GetId(RH),
											g == static_cast<uint32>(Good::Grain) ? Harvests[Region] : PersistentId{});
				}
			}
			if (Market != nullptr)
			{
				*Market = Fresh;
			}
			else
			{
				W.Components().GetPool(Markets.Market).Add(RH, Fresh);
			}
		}
	}

	const RegionMarket* MarketOf(const World& W, const History::PreHistoryTypes& Types, const MarketTypes& Markets,
								 uint32 Region)
	{
		const RegionMarket* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Markets.Market).TryGet(H);
					}
				});
		return Found;
	}

	MarketStats MeasureMarkets(const World& W, const History::PreHistoryTypes& Types, const MarketTypes& Markets,
							   uint32 Region)
	{
		MarketStats S;
		for (uint32 g = 0; g < GoodCount; ++g)
		{
			S.Lowest[g] = 0xffffffffu;
		}
		const MarketRules Bounds;
		std::vector<std::pair<uint32, RegionMarket>> All;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionMarket* M =
						Region == 0 || R.Index == Region ? W.Components().GetPool(Markets.Market).TryGet(H) : nullptr;
					if (M != nullptr)
					{
						All.push_back({R.Index, *M});
					}
				});
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		Hash64 D = HashString("Markets");
		for (const auto& [Index, M] : All)
		{
			++S.Markets;
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				S.Lowest[g] = std::min(S.Lowest[g], M.Price[g]);
				S.Highest[g] = std::max(S.Highest[g], M.Price[g]);
				S.AtFloor[g] +=
					M.Price[g] <= std::max<uint32>(1u, Bounds.BasePrice[g] * Bounds.FloorPerMille / 1000u) ? 1u : 0u;
				S.AtCeiling[g] += M.Price[g] >= Bounds.BasePrice[g] * Bounds.CeilingPerMille / 1000u ? 1u : 0u;
			}
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&M), sizeof(M)));
		}
		if (S.Markets == 0)
		{
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				S.Lowest[g] = 0;
			}
		}
		for (const Event& E : W.Log().All())
		{
			if (!E.Is(PriceChangedEvent))
			{
				continue;
			}
			if (Region != 0 && E.Get<StockPayload>().Region != Region)
			{
				continue;
			}
			++S.Changes;
			S.Caused += E.Cause.IsValid() ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Economy
