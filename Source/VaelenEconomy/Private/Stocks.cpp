// VAELEN - VaelenEconomy
// Phase 06.01: goods and stocks.
//
// STATUS: VALIDATED (Phase 06) - unit/deterministic/edge tests in Tests/Economy

#include "Vaelen/Economy/Stocks.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Economy
{
	namespace
	{
		struct House
		{
			EntityHandle Handle;
			Population::FamilyInfo Info;
		};

		uint32 SumOf(const uint32 Amount[8]) noexcept
		{
			uint32 Sum = 0;
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				Sum += Amount[g];
			}
			return Sum;
		}

		uint32 AddSaturating(uint32 A, uint32 B) noexcept
		{
			const uint64 S = uint64{A} + uint64{B};
			return S > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(S);
		}

		void RegionHandlesOf(const World& W, const History::PreHistoryTypes& Types, std::vector<EntityHandle>& Out)
		{
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (R.Index >= Out.size())
						{
							Out.resize(usize{R.Index} + 1u);
						}
						Out[R.Index] = H;
					});
		}

		void HousesOf(const World& W, const Population::FamilyTypes& Families, std::vector<House>& Out)
		{
			W.Components()
				.GetPool(Families.Family)
				.ForEach([&](EntityHandle H, const Population::FamilyInfo& F) { Out.push_back(House{H, F}); });
			std::sort(Out.begin(), Out.end(),
					  [](const House& A, const House& B) { return A.Info.Index < B.Info.Index; });
		}

		bool DetailedAt(const World& W, const Population::PersonTypes& Persons, EntityHandle RegionHandle)
		{
			return !RegionHandle.IsNull() && W.Components().GetPool(Persons.Detail).TryGet(RegionHandle) != nullptr;
		}
	} // namespace

	const char* GoodName(Good G) noexcept
	{
		switch (G)
		{
		case Good::Grain:
			return "grain";
		case Good::Cloth:
			return "cloth";
		case Good::Tools:
			return "tools";
		case Good::Ore:
			return "ore";
		case Good::Timber:
			return "timber";
		case Good::Salt:
			return "salt";
		case Good::Luxuries:
			return "luxuries";
		case Good::Count:
		default:
			return "goods";
		}
	}

	EconomyTypes EconomyTypes::Declare(World& W)
	{
		EconomyTypes T;
		T.Region = W.Types().Register<RegionStock>("RegionStock");
		T.House = W.Types().Register<HouseStock>("HouseStock");
		W.Components().CreatePool(T.Region);
		W.Components().CreatePool(T.House);
		return T;
	}

	void StockSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		std::vector<EntityHandle> RegionHandles;
		RegionHandlesOf(W, Types, RegionHandles);

		// 1. The land endows every region once: grain from its capacity, the
		//    rest from its deposits.
		std::vector<uint32> Timber(RegionHandles.size(), 0u);
		std::vector<uint32> Ore(RegionHandles.size(), 0u);
		std::vector<uint32> Salt(RegionHandles.size(), 0u);
		std::vector<uint32> Gold(RegionHandles.size(), 0u);
		W.Components()
			.GetPool(Types.World.DepositTypes_.Deposit)
			.ForEach(
				[&](EntityHandle, const WorldGen::DepositInfo& D)
				{
					if (D.Region == 0 || D.Region >= RegionHandles.size())
					{
						return;
					}
					switch (static_cast<WorldGen::ResourceKind>(D.Kind))
					{
					case WorldGen::ResourceKind::Timber:
						Timber[D.Region] = AddSaturating(Timber[D.Region], D.Richness);
						break;
					case WorldGen::ResourceKind::IronOre:
					case WorldGen::ResourceKind::CopperOre:
						Ore[D.Region] = AddSaturating(Ore[D.Region], D.Richness);
						break;
					case WorldGen::ResourceKind::Salt:
						Salt[D.Region] = AddSaturating(Salt[D.Region], D.Richness);
						break;
					case WorldGen::ResourceKind::Gold:
						Gold[D.Region] = AddSaturating(Gold[D.Region], D.Richness);
						break;
					default:
						break;
					}
				});
		for (uint32 Region = 1; Region < RegionHandles.size(); ++Region)
		{
			const EntityHandle RH = RegionHandles[Region];
			if (RH.IsNull() || W.Components().GetPool(Economy.Region).TryGet(RH) != nullptr)
			{
				continue;
			}
			const History::RegionPopulation* P = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			const uint64 Capacity = P != nullptr ? P->Capacity : 0u;
			RegionStock S;
			S.Amount[static_cast<uint32>(Good::Grain)] =
				static_cast<uint32>(std::min<uint64>(Capacity * Rules.EndowGrainPerCapacity / 1000u, 0xffffffffull));
			S.Amount[static_cast<uint32>(Good::Timber)] = static_cast<uint32>(
				std::min<uint64>(uint64{Timber[Region]} * Rules.EndowTimberPerRichness, 0xffffffffull));
			S.Amount[static_cast<uint32>(Good::Ore)] =
				static_cast<uint32>(std::min<uint64>(uint64{Ore[Region]} * Rules.EndowOrePerRichness, 0xffffffffull));
			S.Amount[static_cast<uint32>(Good::Salt)] =
				static_cast<uint32>(std::min<uint64>(uint64{Salt[Region]} * Rules.EndowSaltPerRichness, 0xffffffffull));
			S.Amount[static_cast<uint32>(Good::Luxuries)] = static_cast<uint32>(
				std::min<uint64>(uint64{Gold[Region]} * Rules.EndowLuxuryPerRichnessPerMille / 1000u, 0xffffffffull));
			W.Components().GetPool(Economy.Region).Add(RH, S);
			Context.Events->Publish(Context.Tick, StockEndowedEvent,
									StockPayload{Region, 0, GoodCount, SumOf(S.Amount)}, W.Entities().GetId(RH));
		}

		// 2. Houses whose region is coarse again fold into the common stock;
		//    extinct houses return theirs.
		std::vector<House> Houses;
		HousesOf(W, Families, Houses);
		std::vector<uint32> FoldedHouses(RegionHandles.size(), 0u);
		std::vector<uint32> FoldedUnits(RegionHandles.size(), 0u);
		for (const House& Hs : Houses)
		{
			const HouseStock* Stock = W.Components().GetPool(Economy.House).TryGet(Hs.Handle);
			if (Stock == nullptr)
			{
				continue;
			}
			const uint32 Region = Hs.Info.Region;
			const EntityHandle RH = Region < RegionHandles.size() ? RegionHandles[Region] : EntityHandle{};
			const bool Coarse = !DetailedAt(W, Persons, RH);
			const bool Extinct = Hs.Info.Extinct != 0;
			if (!Coarse && !Extinct)
			{
				continue;
			}
			const uint32 Units = SumOf(Stock->Amount);
			// An extinct house with an heir of the same region passes its goods on;
			// everything else joins the common stock.
			const HouseHeir* Heir = !Coarse && HasHeirs ? W.Components().GetPool(Heirs).TryGet(Hs.Handle) : nullptr;
			HouseStock* Inheriting = nullptr;
			uint32 HeirIndex = 0;
			if (Heir != nullptr && Heir->Family != 0)
			{
				for (const House& Other : Houses)
				{
					if (Other.Info.Index == Heir->Family && Other.Info.Extinct == 0 && Other.Info.Region == Region)
					{
						Inheriting = W.Components().GetPool(Economy.House).TryGet(Other.Handle);
						HeirIndex = Other.Info.Index;
						break;
					}
				}
			}
			if (Inheriting != nullptr)
			{
				for (uint32 g = 0; g < GoodCount; ++g)
				{
					Inheriting->Amount[g] = AddSaturating(Inheriting->Amount[g], Stock->Amount[g]);
				}
			}
			else
			{
				RegionStock* Common = RH.IsNull() ? nullptr : W.Components().GetPool(Economy.Region).TryGet(RH);
				if (Common != nullptr)
				{
					for (uint32 g = 0; g < GoodCount; ++g)
					{
						Common->Amount[g] = AddSaturating(Common->Amount[g], Stock->Amount[g]);
					}
				}
			}
			W.Components().GetPool(Economy.House).Remove(Hs.Handle);
			if (Coarse)
			{
				++FoldedHouses[Region];
				FoldedUnits[Region] = AddSaturating(FoldedUnits[Region], Units);
			}
			else
			{
				Context.Events->Publish(
					Context.Tick, Inheriting != nullptr ? StockInheritedEvent : StockReturnedEvent,
					StockPayload{Region, Inheriting != nullptr ? HeirIndex : Hs.Info.Index, GoodCount, Units},
					W.Entities().GetId(Hs.Handle));
			}
		}
		for (uint32 Region = 1; Region < RegionHandles.size(); ++Region)
		{
			if (FoldedHouses[Region] > 0)
			{
				Context.Events->Publish(Context.Tick, StockFoldedEvent,
										StockPayload{Region, FoldedHouses[Region], GoodCount, FoldedUnits[Region]},
										W.Entities().GetId(RegionHandles[Region]));
			}
		}

		// 3. Detailed regions: every living house holds a stock; a region whose
		//    houses hold none yet (just promoted) splits a share of its common
		//    stock among them by their living members, the rest stays in common.
		std::vector<uint32> Regions;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const Population::RegionDetail& D) { Regions.push_back(D.Region); });
		std::sort(Regions.begin(), Regions.end());
		for (const uint32 Region : Regions)
		{
			const EntityHandle RH = Region < RegionHandles.size() ? RegionHandles[Region] : EntityHandle{};
			RegionStock* Common = RH.IsNull() ? nullptr : W.Components().GetPool(Economy.Region).TryGet(RH);
			if (Common == nullptr)
			{
				continue;
			}
			std::vector<const House*> Living;
			bool AnyStock = false;
			for (const House& Hs : Houses)
			{
				if (Hs.Info.Region != Region || Hs.Info.Extinct != 0)
				{
					continue;
				}
				Living.push_back(&Hs);
				AnyStock = AnyStock || W.Components().GetPool(Economy.House).TryGet(Hs.Handle) != nullptr;
			}
			if (Living.empty())
			{
				continue;
			}
			if (AnyStock)
			{
				for (const House* Hs : Living)
				{
					if (W.Components().GetPool(Economy.House).TryGet(Hs->Handle) == nullptr)
					{
						W.Components()
							.GetPool(Economy.House)
							.Add(Hs->Handle, HouseStock{}); // founded since: nothing yet
					}
				}
				continue;
			}
			// Weights: living members of each house, in the region.
			std::vector<uint32> Members(Living.size(), 0u);
			uint64 Weight = 0;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const Population::PersonInfo& P)
					{
						if (P.Region != Region || P.State != static_cast<uint8>(Population::LifeState::Alive) ||
							P.Family == 0)
						{
							return;
						}
						for (usize i = 0; i < Living.size(); ++i)
						{
							if (Living[i]->Info.Index == P.Family)
							{
								++Members[i];
								++Weight;
								return;
							}
						}
					});
			uint32 Given = 0;
			std::vector<HouseStock> Shares(Living.size());
			if (Weight > 0)
			{
				for (uint32 g = 0; g < GoodCount; ++g)
				{
					const uint64 Pool = uint64{Common->Amount[g]} * Rules.HouseSharePerMille / 1000u;
					uint64 Spent = 0;
					for (usize i = 0; i < Living.size(); ++i)
					{
						const uint64 Share = Pool * Members[i] / Weight;
						Shares[i].Amount[g] = static_cast<uint32>(Share);
						Spent += Share;
					}
					Common->Amount[g] -= static_cast<uint32>(Spent);
					Given = AddSaturating(Given, static_cast<uint32>(Spent));
				}
			}
			for (usize i = 0; i < Living.size(); ++i)
			{
				W.Components().GetPool(Economy.House).Add(Living[i]->Handle, Shares[i]);
			}
			Context.Events->Publish(Context.Tick, StockSplitEvent,
									StockPayload{Region, static_cast<uint32>(Living.size()), GoodCount, Given},
									W.Entities().GetId(RH));
		}
	}

	const RegionStock* StockOf(const World& W, const History::PreHistoryTypes& Types, const EconomyTypes& Economy,
							   uint32 Region)
	{
		const RegionStock* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Economy.Region).TryGet(H);
					}
				});
		return Found;
	}

	const HouseStock* HouseStockOf(const World& W, const Population::FamilyTypes& Families, const EconomyTypes& Economy,
								   uint32 Family)
	{
		const HouseStock* Found = nullptr;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					if (F.Index == Family && Found == nullptr)
					{
						Found = W.Components().GetPool(Economy.House).TryGet(H);
					}
				});
		return Found;
	}

	void TotalStock(const World& W, const History::PreHistoryTypes& Types, const Population::FamilyTypes& Families,
					const EconomyTypes& Economy, uint32 Region, uint32 Out[GoodCount])
	{
		for (uint32 g = 0; g < GoodCount; ++g)
		{
			Out[g] = 0;
		}
		const RegionStock* Common = StockOf(W, Types, Economy, Region);
		if (Common != nullptr)
		{
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				Out[g] = Common->Amount[g];
			}
		}
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					const HouseStock* S =
						F.Region == Region ? W.Components().GetPool(Economy.House).TryGet(H) : nullptr;
					if (S == nullptr)
					{
						return;
					}
					for (uint32 g = 0; g < GoodCount; ++g)
					{
						Out[g] = AddSaturating(Out[g], S->Amount[g]);
					}
				});
	}

	uint32 AddStock(World& W, const History::PreHistoryTypes& Types, const Population::FamilyTypes& Families,
					const EconomyTypes& Economy, uint32 Region, uint32 House, Good G, int32 Delta, SimTick Tick,
					PersistentId Cause)
	{
		if (Delta == 0 || static_cast<uint32>(G) >= GoodCount)
		{
			return 0;
		}
		const uint32 g = static_cast<uint32>(G);
		uint32* Amount = nullptr;
		EntityHandle Subject;
		if (House == 0)
		{
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (R.Index == Region && Amount == nullptr)
						{
							RegionStock* S = W.Components().GetPool(Economy.Region).TryGet(H);
							Amount = S != nullptr ? &S->Amount[g] : nullptr;
							Subject = H;
						}
					});
		}
		else
		{
			W.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle H, const Population::FamilyInfo& F)
					{
						if (F.Index == House && F.Region == Region && Amount == nullptr)
						{
							HouseStock* S = W.Components().GetPool(Economy.House).TryGet(H);
							Amount = S != nullptr ? &S->Amount[g] : nullptr;
							Subject = H;
						}
					});
		}
		if (Amount == nullptr)
		{
			return 0;
		}
		uint32 Moved = 0;
		if (Delta > 0)
		{
			const uint32 Before = *Amount;
			*Amount = AddSaturating(*Amount, static_cast<uint32>(Delta));
			Moved = *Amount - Before;
		}
		else
		{
			const uint32 Wanted = static_cast<uint32>(-static_cast<int64>(Delta));
			Moved = std::min(Wanted, *Amount);
			*Amount -= Moved;
		}
		if (Moved > 0)
		{
			W.Events().Publish(Tick, Delta > 0 ? StockAddedEvent : StockTakenEvent,
							   StockPayload{Region, House, g, Moved}, W.Entities().GetId(Subject), Cause);
		}
		return Moved;
	}

	StockStats MeasureStocks(const World& W, const History::PreHistoryTypes& Types,
							 const Population::PersonTypes& Persons, const Population::FamilyTypes& Families,
							 const EconomyTypes& Economy, uint32 Region)
	{
		StockStats S;
		std::vector<EntityHandle> RegionHandles;
		RegionHandlesOf(W, Types, RegionHandles);
		std::vector<std::pair<uint32, RegionStock>> Commons;
		for (uint32 R = 1; R < RegionHandles.size(); ++R)
		{
			if (RegionHandles[R].IsNull() || (Region != 0 && R != Region))
			{
				continue;
			}
			const RegionStock* St = W.Components().GetPool(Economy.Region).TryGet(RegionHandles[R]);
			if (St == nullptr)
			{
				continue;
			}
			++S.RegionsWithStock;
			Commons.push_back({R, *St});
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				S.Common[g] = AddSaturating(S.Common[g], St->Amount[g]);
				S.Total[g] = AddSaturating(S.Total[g], St->Amount[g]);
			}
		}
		std::vector<House> Houses;
		HousesOf(W, Families, Houses);
		std::vector<std::pair<uint32, HouseStock>> Held;
		for (const House& Hs : Houses)
		{
			if (Region != 0 && Hs.Info.Region != Region)
			{
				continue;
			}
			const HouseStock* St = W.Components().GetPool(Economy.House).TryGet(Hs.Handle);
			if (St == nullptr)
			{
				continue;
			}
			++S.HousesWithStock;
			const EntityHandle RH =
				Hs.Info.Region < RegionHandles.size() ? RegionHandles[Hs.Info.Region] : EntityHandle{};
			S.Stale += Hs.Info.Extinct != 0 || !DetailedAt(W, Persons, RH) ? 1u : 0u;
			Held.push_back({Hs.Info.Index, *St});
			for (uint32 g = 0; g < GoodCount; ++g)
			{
				S.Total[g] = AddSaturating(S.Total[g], St->Amount[g]);
			}
		}
		for (const Event& E : W.Log().All())
		{
			if (!(E.Is(StockEndowedEvent) || E.Is(StockSplitEvent) || E.Is(StockFoldedEvent) ||
				  E.Is(StockReturnedEvent) || E.Is(StockInheritedEvent) || E.Is(StockAddedEvent) ||
				  E.Is(StockTakenEvent)))
			{
				continue;
			}
			const StockPayload P = E.Get<StockPayload>();
			if (Region != 0 && P.Region != Region)
			{
				continue;
			}
			S.Endowed += E.Is(StockEndowedEvent) ? 1u : 0u;
			S.Splits += E.Is(StockSplitEvent) ? 1u : 0u;
			S.Folds += E.Is(StockFoldedEvent) ? 1u : 0u;
			S.Returns += E.Is(StockReturnedEvent) ? 1u : 0u;
			S.Inheritances += E.Is(StockInheritedEvent) ? 1u : 0u;
			S.Added += E.Is(StockAddedEvent) ? 1u : 0u;
			S.Taken += E.Is(StockTakenEvent) ? 1u : 0u;
		}
		Hash64 D = HashString("Stocks");
		for (const auto& [Index, St] : Commons)
		{
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&St), sizeof(St)));
		}
		for (const auto& [Index, St] : Held)
		{
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&St), sizeof(St)));
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Economy
