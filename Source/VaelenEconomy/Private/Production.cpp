// VAELEN - VaelenEconomy
// Phase 06.02: production and consumption.
//
// STATUS: VALIDATED (Phase 06) - unit/integration/deterministic tests in Tests/Economy

#include "Vaelen/Economy/Production.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Economy
{
	namespace
	{
		constexpr uint32 G_GRAIN = static_cast<uint32>(Good::Grain);
		constexpr uint32 G_CLOTH = static_cast<uint32>(Good::Cloth);
		constexpr uint32 G_TOOLS = static_cast<uint32>(Good::Tools);
		constexpr uint32 G_ORE = static_cast<uint32>(Good::Ore);
		constexpr uint32 G_TIMBER = static_cast<uint32>(Good::Timber);
		constexpr uint32 G_SALT = static_cast<uint32>(Good::Salt);

		uint32 Saturate(uint64 V) noexcept
		{
			return V > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(V);
		}

		// `Take(Amount, Wanted)` used to live here, and its disappearance is the
		// shape of ADR-0111. It moved grain out of a stock and told nobody. Every
		// one of its call sites now goes through MoveStock, which does the same
		// arithmetic and publishes StockTaken, so the compiler removing this as
		// unused was the proof that none was missed.

		struct House
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Members = 0; ///< living, in the region
			uint64 Yield = 0;	///< sum of the workers' yields, per mille of a worker's harvest
		};

		struct Blow
		{
			uint32 CutPerMille = 0;
			PersistentId Event;
		};
	} // namespace

	ProductionTypes ProductionTypes::Declare(World& W)
	{
		ProductionTypes T;
		T.Ration = W.Types().Register<Population::RegionRation>("RegionRation");
		W.Components().CreatePool(T.Ration);
		return T;
	}

	ComponentType<RegionMined> DeclareMined(World& W)
	{
		const ComponentType<RegionMined> T = W.Types().Register<RegionMined>("RegionMined");
		W.Components().CreatePool(T);
		return T;
	}

	void ProductionSystem::Tick(TickContext& Context)
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
		// This year's droughts, the worst per region.
		std::vector<Blow> Blows(N);
		const std::vector<Event>& Events = W.Log().All();
		for (usize i = Events.size(); i > 0; --i)
		{
			const Event& E = Events[i - 1];
			if (E.Tick + History::TicksPerYear <= Context.Tick)
			{
				break;
			}
			if (!E.Is(History::DisasterStruckEvent))
			{
				continue;
			}
			const History::DisasterPayload P = E.Get<History::DisasterPayload>();
			if (P.Kind != static_cast<uint32>(History::DisasterKind::Drought) || P.Severity == 0 || P.Region >= N)
			{
				continue;
			}
			const uint32 Cut = Rules.DroughtCutPerMille[P.Severity > 3 ? 2u : P.Severity - 1u];
			if (Cut > Blows[P.Region].CutPerMille)
			{
				Blows[P.Region] = Blow{Cut, E.Id};
			}
		}
		// The deposits, by region.
		std::vector<uint32> Timber(N, 0u), Ore(N, 0u), Salt(N, 0u);
		W.Components()
			.GetPool(Types.World.DepositTypes_.Deposit)
			.ForEach(
				[&](EntityHandle, const WorldGen::DepositInfo& D)
				{
					if (D.Region == 0 || D.Region >= N)
					{
						return;
					}
					switch (static_cast<WorldGen::ResourceKind>(D.Kind))
					{
					case WorldGen::ResourceKind::Timber:
						Timber[D.Region] = Saturate(uint64{Timber[D.Region]} + D.Richness);
						break;
					case WorldGen::ResourceKind::IronOre:
					case WorldGen::ResourceKind::CopperOre:
						Ore[D.Region] = Saturate(uint64{Ore[D.Region]} + D.Richness);
						break;
					case WorldGen::ResourceKind::Salt:
						Salt[D.Region] = Saturate(uint64{Salt[D.Region]} + D.Richness);
						break;
					default:
						break;
					}
				});
		// The detailed regions, their houses and their people.
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
		std::vector<House> Houses;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					if (F.Extinct == 0 && F.Region < N && Detailed[F.Region] != 0 &&
						W.Components().GetPool(Economy.House).TryGet(H) != nullptr)
					{
						Houses.push_back(House{H, F.Index, 0, 0});
					}
				});
		std::sort(Houses.begin(), Houses.end(), [](const House& A, const House& B) { return A.Index < B.Index; });
		std::vector<uint32> HouseAt; // family index -> position + 1
		for (usize i = 0; i < Houses.size(); ++i)
		{
			if (Houses[i].Index >= HouseAt.size())
			{
				HouseAt.resize(usize{Houses[i].Index} + 1u, 0u);
			}
			HouseAt[Houses[i].Index] = static_cast<uint32>(i + 1);
		}
		// 11.06: which ground is worked for what is under it, by region, so the
		// person loop can tell a miner from a farmer.
		std::vector<uint8> MinedHere(N, 0u);
		if (HasMined)
		{
			for (uint32 R = 1; R < N; ++R)
			{
				const EntityHandle RH = RegionHandles[R];
				MinedHere[R] = !RH.IsNull() && W.Components().GetPool(Mined).TryGet(RH) != nullptr ? 1u : 0u;
			}
		}
		std::vector<uint32> Alive(N, 0u), Workers(N, 0u), Unhoused(N, 0u), Crafters(N, 0u);
		std::vector<uint64> UnhousedYield(N, 0u), CraftSum(N, 0u);
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (P.Region >= N || Detailed[P.Region] == 0 ||
						P.State != static_cast<uint8>(Population::LifeState::Alive))
					{
						return;
					}
					++Alive[P.Region];
					const Population::PersonTraits* T = HasTraits ? W.Components().GetPool(Traits).TryGet(H) : nullptr;
					const uint32 Farming =
						T != nullptr ? T->Skills[static_cast<uint32>(Population::Skill::Farming)] : 0u;
					const uint32 Craft = T != nullptr ? T->Skills[static_cast<uint32>(Population::Skill::Craft)] : 0u;
					// On mined ground the bound are on the rock: they are not farm
					// workers and the fields yield nothing from them. Everybody
					// else on that ground still farms, which is what 11.06 found
					// the first version of this rule got wrong by reaping nothing
					// at all - a colony has free people, and there is no spare
					// food anywhere in AELVOR to send it instead.
					const bool OnRock =
						MinedHere[P.Region] != 0 && HasBonds && W.Components().GetPool(Bonds).TryGet(H) != nullptr;
					const bool Works = !OnRock && Population::AgeYears(P, Context.Tick) >= Rules.WorkerFromAge;
					const uint64 Yield =
						Works ? Rules.FarmingFloorPerMille + uint64{Rules.FarmingSpanPerMille} * Farming / 255u : 0u;
					Workers[P.Region] += Works ? 1u : 0u;
					if (Craft >= Rules.CraftFrom && Works)
					{
						++Crafters[P.Region];
						CraftSum[P.Region] += Craft;
					}
					const uint32 At = P.Family < HouseAt.size() ? HouseAt[P.Family] : 0u;
					if (At == 0)
					{
						++Unhoused[P.Region];
						UnhousedYield[P.Region] += Yield;
						return;
					}
					++Houses[At - 1].Members;
					Houses[At - 1].Yield += Yield;
				});

		for (uint32 Region = 1; Region < N; ++Region)
		{
			const EntityHandle RH = RegionHandles[Region];
			RegionStock* Common = RH.IsNull() ? nullptr : W.Components().GetPool(Economy.Region).TryGet(RH);
			if (Common == nullptr)
			{
				continue;
			}
			// 11.03: this ground's hands are on the rock, not in the fields. One
			// fact, three consequences below: nothing reaped here, no ore extracted
			// here, and the people still eat out of the common stock - which is
			// what makes a colony a place that eats what it does not grow.
			const History::RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			const uint64 Capacity = Counts != nullptr ? Counts->Capacity : 0u;
			const bool Fine = Detailed[Region] != 0;
			// Only where the colony is actually simulated person by person. A coarse
			// region has no bonds to read, so there is no telling who is on the rock
			// and who is in the fields - and the colony's own daily mining cannot run
			// there either, because it counts hands and a coarse region has none. So a
			// mined region that goes coarse behaves exactly as it did before Phase 11:
			// it reaps, and the yearly pass extracts its ore. Leaving the harvest
			// zeroed here would have starved it for the very reason 11.06 fixed in the
			// fine branch, and no gate would have shown it - a colony is held detailed.
			const bool OnTheRock = Fine && HasMined && W.Components().GetPool(Mined).TryGet(RH) != nullptr;
			const uint64 People = Fine ? Alive[Region] : (Counts != nullptr ? Counts->Total : 0u);
			if (People == 0)
			{
				continue;
			}
			const uint64 Kept = 1000u - Blows[Region].CutPerMille;
			// 1. The harvest.
			// What the region has built lifts what the same fields and the same
			// hands give (09.02). Nothing built means a factor of one, to the unit.
			const RegionWorkshops* Built = HasShops ? W.Components().GetPool(Shops).TryGet(RH) : nullptr;
			const uint64 FieldsPerMille = 1000u + (Built != nullptr ? Built->FieldsPerMille : 0u);
			uint64 Harvest = 0;
			if (!Fine)
			{
				// OnTheRock is false here by construction (it is gated on Fine), so a
				// coarse region reaps as it always did, mined or not.
				const uint64 OnLand = std::min<uint64>(People, Capacity);
				Harvest = OnLand * Rules.WorkerSharePerMille / 1000u * Rules.HarvestPerWorker * Kept / 1000u *
						  FieldsPerMille / 1000u;
				Common->Amount[G_GRAIN] = Saturate(Common->Amount[G_GRAIN] + Harvest);
			}
			else
			{
				// The land takes so many workers: beyond the capacity's share the rest find none.
				const uint64 Land = Capacity * Rules.WorkerSharePerMille / 1000u;
				// Not zeroed for mined ground any more: Workers already excludes the
				// bound, so a colony reaps in proportion to the people it has left
				// over rather than not at all. 11.06 measured why that matters -
				// AELVOR is a subsistence world with no spare food to send it.
				const uint64 ScalePerMille =
					Workers[Region] == 0 || Land >= Workers[Region] ? 1000u : Land * 1000u / Workers[Region];
				const Population::RegionStores* Granary =
					HasStores ? W.Components().GetPool(Stores).TryGet(RH) : nullptr;
				const uint64 ToCommon = Granary != nullptr ? std::min<uint32>(1000u, Granary->GrainPerMille) : 0u;
				auto Reap = [&](uint64 YieldPerMille) -> uint64
				{
					return YieldPerMille * Rules.HarvestPerWorker / 1000u * ScalePerMille / 1000u * Kept / 1000u *
						   FieldsPerMille / 1000u;
				};
				for (House& Hs : Houses)
				{
					HouseStock* Stock = W.Components().GetPool(Economy.House).TryGet(Hs.Handle);
					const Population::FamilyInfo* F = W.Components().GetPool(Families.Family).TryGet(Hs.Handle);
					if (Stock == nullptr || F == nullptr || F->Region != Region || Hs.Yield == 0)
					{
						continue;
					}
					const uint64 Reaped = Reap(Hs.Yield);
					const uint64 Shared = Reaped * ToCommon / 1000u;
					Stock->Amount[G_GRAIN] = Saturate(Stock->Amount[G_GRAIN] + (Reaped - Shared));
					Common->Amount[G_GRAIN] = Saturate(Common->Amount[G_GRAIN] + Shared);
					Harvest += Reaped;
				}
				const uint64 Loose = Reap(UnhousedYield[Region]);
				Common->Amount[G_GRAIN] = Saturate(Common->Amount[G_GRAIN] + Loose);
				Harvest += Loose;
			}
			Context.Events->Publish(Context.Tick, HarvestEvent, StockPayload{Region, 0, G_GRAIN, Saturate(Harvest)},
									W.Entities().GetId(RH), Blows[Region].Event);
			// What is owed away is assessed on the harvest of the year, in the
			// year it is reaped: a bad year owes less. The grain does not move
			// here - a collector of a higher layer comes for it (07.02).
			if (HasDues)
			{
				RegionDues* Owed = W.Components().GetPool(Dues).TryGet(RH);
				if (Owed != nullptr && Owed->PerMille != 0)
				{
					Owed->Owed = Saturate(uint64{Owed->Owed} + Harvest * Owed->PerMille / 1000u);
				}
			}
			// ADR-0111. Everything from here down used to move by direct
			// assignment and publish nothing, and that is why a region could
			// harvest 7329 units of grain, end the year six units richer, and
			// leave the log unable to account for the other 7323. The chronicle
			// could say what a region GREW and never what it ATE.
			//
			// The harvest above keeps its direct writes on purpose: HarvestEvent
			// already names it, and naming it twice would make the ledger count
			// it twice. What follows is the movement nothing named.
			//
			// MoveStock rather than AddStock because AddStock's lookup is a pass
			// over the region or family pool per call, and this loop already
			// holds every pointer it needs - see Stocks.h.
			const PersistentId Why = Blows[Region].Event;
			auto Gain = [&](uint32* Amount, EntityHandle Sub, uint32 Of, uint32 G, uint64 Units) -> uint32
			{
				return MoveStock(W, Amount, Sub, Region, Of, static_cast<Good>(G),
								 static_cast<int32>(std::min<uint64>(Units, 0x7fffffffull)), Context.Tick, Why);
			};
			auto Lose = [&](uint32* Amount, EntityHandle Sub, uint32 Of, uint32 G, uint64 Units) -> uint32
			{
				return MoveStock(W, Amount, Sub, Region, Of, static_cast<Good>(G),
								 -static_cast<int32>(std::min<uint64>(Units, 0x7fffffffull)), Context.Tick, Why);
			};

			// 2. Spoilage, then the meals: a house from its own stock, then the common one.
			Lose(&Common->Amount[G_GRAIN], RH, 0, G_GRAIN,
				 uint64{Common->Amount[G_GRAIN]} * Rules.GrainSpoilPerMille / 1000u);
			uint64 Need = People * Rules.GrainPerPerson;
			uint64 Short = 0;
			if (!Fine)
			{
				Short = Need - Lose(&Common->Amount[G_GRAIN], RH, 0, G_GRAIN, Need);
			}
			else
			{
				for (const House& Hs : Houses)
				{
					HouseStock* Stock = W.Components().GetPool(Economy.House).TryGet(Hs.Handle);
					const Population::FamilyInfo* F = W.Components().GetPool(Families.Family).TryGet(Hs.Handle);
					if (Stock == nullptr || F == nullptr || F->Region != Region)
					{
						continue;
					}
					Lose(&Stock->Amount[G_GRAIN], Hs.Handle, F->Index, G_GRAIN,
						 uint64{Stock->Amount[G_GRAIN]} * Rules.GrainSpoilPerMille / 1000u);
					const uint64 Wanted = uint64{Hs.Members} * Rules.GrainPerPerson;
					uint64 Left = Wanted - Lose(&Stock->Amount[G_GRAIN], Hs.Handle, F->Index, G_GRAIN, Wanted);
					Left -= Lose(&Common->Amount[G_GRAIN], RH, 0, G_GRAIN, Left);
					Short += Left;
				}
				const uint64 Wanted = uint64{Unhoused[Region]} * Rules.GrainPerPerson;
				Short += Wanted - Lose(&Common->Amount[G_GRAIN], RH, 0, G_GRAIN, Wanted);
			}
			Population::RegionRation Ration;
			Ration.PerMille = Need == 0 ? 1000u : static_cast<uint32>((Need - Short) * 1000u / Need);
			Population::RegionRation* Written = W.Components().GetPool(Production.Ration).TryGet(RH);
			if (Written != nullptr)
			{
				*Written = Ration;
			}
			else
			{
				W.Components().GetPool(Production.Ration).Add(RH, Ration);
			}
			if (Short > 0)
			{
				Context.Events->Publish(Context.Tick, ShortfallEvent, StockPayload{Region, 0, G_GRAIN, Saturate(Short)},
										W.Entities().GetId(RH), Blows[Region].Event);
			}
			// 3. The other goods, all in the common stock.
			if (!OnTheRock && People >= Rules.ExtractFromPeople)
			{
				Gain(&Common->Amount[G_TIMBER], RH, 0, G_TIMBER,
					 uint64{Timber[Region]} * Rules.ExtractPerMille / 1000u);
				Gain(&Common->Amount[G_ORE], RH, 0, G_ORE, uint64{Ore[Region]} * Rules.ExtractPerMille / 1000u);
				Gain(&Common->Amount[G_SALT], RH, 0, G_SALT, uint64{Salt[Region]} * Rules.ExtractPerMille / 1000u);
			}
			Lose(&Common->Amount[G_TIMBER], RH, 0, G_TIMBER,
				 Rules.TimberPerPersons > 0 ? People / Rules.TimberPerPersons : 0u);
			Lose(&Common->Amount[G_SALT], RH, 0, G_SALT, Rules.SaltPerPersons > 0 ? People / Rules.SaltPerPersons : 0u);
			uint64 CraftPerMille = 1000;
			if (Fine)
			{
				const uint64 Mean = Crafters[Region] > 0 ? CraftSum[Region] / Crafters[Region] : 0u;
				CraftPerMille = Rules.CraftFloorPerMille + uint64{Rules.CraftSpanPerMille} * Mean / 255u;
			}
			CraftPerMille = CraftPerMille * (1000u + (Built != nullptr ? Built->CraftPerMille : 0u)) / 1000u;
			const uint64 Cloth =
				(Rules.ClothPerPersons > 0 ? People / Rules.ClothPerPersons : 0u) * CraftPerMille / 1000u;
			Gain(&Common->Amount[G_CLOTH], RH, 0, G_CLOTH, Cloth);
			const uint64 ToolsWanted =
				(Rules.ToolsPerPersons > 0 ? People / Rules.ToolsPerPersons : 0u) * CraftPerMille / 1000u;
			const uint32 Made = Lose(&Common->Amount[G_ORE], RH, 0, G_ORE, ToolsWanted); // one ore each
			Gain(&Common->Amount[G_TOOLS], RH, 0, G_TOOLS, Made);
			Lose(&Common->Amount[G_CLOTH], RH, 0, G_CLOTH,
				 Rules.ClothWearPerPersons > 0 ? People / Rules.ClothWearPerPersons : 0u);
			Lose(&Common->Amount[G_TOOLS], RH, 0, G_TOOLS,
				 Rules.ToolsWearPerPersons > 0 ? People / Rules.ToolsWearPerPersons : 0u);
		}
	}

	const Population::RegionRation* RationOf(const World& W, const History::PreHistoryTypes& Types,
											 const ProductionTypes& Production, uint32 Region)
	{
		const Population::RegionRation* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Production.Ration).TryGet(H);
					}
				});
		return Found;
	}

	ProductionStats MeasureProduction(const World& W, const History::PreHistoryTypes& Types,
									  const ProductionTypes& Production, uint32 Region)
	{
		ProductionStats S;
		for (const Event& E : W.Log().All())
		{
			const bool Harvest = E.Is(HarvestEvent);
			if (!Harvest && !E.Is(ShortfallEvent))
			{
				continue;
			}
			const StockPayload P = E.Get<StockPayload>();
			if (Region != 0 && P.Region != Region)
			{
				continue;
			}
			if (Harvest)
			{
				++S.Harvests;
				S.Grain += P.Amount;
				S.Cut += E.Cause.IsValid() ? 1u : 0u;
			}
			else
			{
				++S.Shortfalls;
				S.Short += P.Amount;
			}
		}
		std::vector<std::pair<uint32, Population::RegionRation>> Rations;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const Population::RegionRation* Rt = Region == 0 || R.Index == Region
															 ? W.Components().GetPool(Production.Ration).TryGet(H)
															 : nullptr;
					if (Rt != nullptr)
					{
						Rations.push_back({R.Index, *Rt});
					}
				});
		std::sort(Rations.begin(), Rations.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		Hash64 D = HashString("Production");
		for (const auto& [Index, Rt] : Rations)
		{
			S.Rationed += Rt.PerMille < 1000 ? 1u : 0u;
			S.RationMin = std::min(S.RationMin, Rt.PerMille);
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashUInt64(Rt.PerMille));
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Economy
