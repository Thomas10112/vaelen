// VAELEN - VaelenPopulation
// Phase 04.04: needs and body - food, health, famine and disease.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population

#include "Vaelen/Population/Needs.h"

#include "Vaelen/Core/Random.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Population
{
	namespace
	{
		uint8 Clamp255(int32 V) noexcept
		{
			return static_cast<uint8>(V < 0 ? 0 : (V > 255 ? 255 : V));
		}

		struct Ref
		{
			EntityHandle Handle;
			PersonInfo Info;
		};

		struct Blow
		{
			uint32 Kind = 0;
			uint32 Severity = 0;
			SimTick Tick = 0;
			PersistentId Event;
		};
	} // namespace

	NeedTypes NeedTypes::Declare(World& W)
	{
		NeedTypes T;
		T.Needs = W.Types().Register<PersonNeeds>("PersonNeeds");
		W.Components().CreatePool(T.Needs);
		return T;
	}

	void NeedSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr || Context.Random == nullptr)
		{
			return;
		}
		World& W = *Owner;
		RandomStream& Random = *Context.Random;
		std::vector<uint32> Regions;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const RegionDetail& D) { Regions.push_back(D.Region); });
		std::sort(Regions.begin(), Regions.end());
		if (Regions.empty())
		{
			return;
		}
		// Disasters that struck detailed regions lately (the coarse system killed
		// nobody there: their deaths are ours). This year's cut the ration and
		// spread disease; the memory years only name the famine.
		const uint64 Memory =
			uint64{History::TicksPerYear} * (Rules.FamineMemoryYears > 0 ? Rules.FamineMemoryYears : 1u);
		std::vector<std::pair<uint32, Blow>> Blows;
		const std::vector<Event>& Events = W.Log().All();
		for (usize i = Events.size(); i > 0; --i)
		{
			const Event& E = Events[i - 1];
			if (E.Tick + Memory <= Context.Tick)
			{
				break;
			}
			if (E.Is(History::DisasterStruckEvent))
			{
				const History::DisasterPayload P = E.Get<History::DisasterPayload>();
				if (std::find(Regions.begin(), Regions.end(), P.Region) != Regions.end())
				{
					Blows.push_back({P.Region, Blow{P.Kind, P.Severity, E.Tick, E.Id}});
				}
			}
		}
		std::reverse(Blows.begin(), Blows.end()); // oldest first

		for (const uint32 Region : Regions)
		{
			EntityHandle RH;
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (R.Index == Region && RH.IsNull())
						{
							RH = H;
						}
					});
			if (RH.IsNull())
			{
				continue;
			}
			std::vector<Ref> People;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive))
						{
							People.push_back(Ref{H, P});
						}
					});
			std::sort(People.begin(), People.end(),
					  [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
			if (People.empty())
			{
				continue;
			}
			// Everyone alive carries needs; newcomers start whole.
			for (const Ref& R : People)
			{
				if (W.Components().GetPool(Needs.Needs).TryGet(R.Handle) == nullptr)
				{
					W.Components().GetPool(Needs.Needs).Add(R.Handle, PersonNeeds{});
				}
			}
			// The ration: capacity over the living, cut by this year's droughts.
			const History::RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			const uint64 Capacity = Counts != nullptr ? Counts->Capacity : 0u;
			uint64 RationPerMille = Capacity >= People.size() ? 1000u : Capacity * 1000u / People.size();
			PersistentId Drought;
			PersistentId Plague;
			uint32 PlagueSeverity = 0;
			for (const auto& [R, B] : Blows)
			{
				if (R != Region || B.Severity == 0)
				{
					continue;
				}
				const uint32 S = B.Severity > 3 ? 2u : B.Severity - 1u;
				const bool ThisYear = B.Tick + History::TicksPerYear > Context.Tick;
				if (B.Kind == static_cast<uint32>(History::DisasterKind::Drought))
				{
					if (ThisYear)
					{
						// The stores soften the cut (a council's grain, Phase 05).
						uint32 Cut = Rules.DroughtCutPerMille[S];
						const RegionStores* Grain = HasStores ? W.Components().GetPool(Stores).TryGet(RH) : nullptr;
						if (Grain != nullptr && Grain->GrainPerMille > 0)
						{
							Cut = Cut * (1000u - std::min(1000u, Grain->GrainPerMille)) / 1000u;
						}
						RationPerMille = RationPerMille * (1000u - Cut) / 1000u;
					}
					Drought = B.Event; // the latest drought names the famine
				}
				if (ThisYear && B.Kind == static_cast<uint32>(History::DisasterKind::Plague) &&
					B.Severity > PlagueSeverity)
				{
					PlagueSeverity = B.Severity;
					Plague = B.Event;
				}
			}
			const uint32 Refill = static_cast<uint32>(uint64{Rules.FoodRefillMax} * RationPerMille / 1000u);

			// Each person: eat, hunger, recover, sicken, die.
			uint32 Deaths = 0;
			for (const Ref& R : People)
			{
				PersonNeeds& N = W.Components().GetPool(Needs.Needs).Get(R.Handle);
				const uint32 Age = AgeYears(R.Info, Context.Tick);
				const bool Frail = Age < 5 || Age >= Rules.ElderFrom;
				auto Extra = [&](uint32 Damage)
				{ return Frail ? Damage + Damage * Rules.FrailExtraPerMille / 1000u : Damage; };
				N.Food = Clamp255(static_cast<int32>(N.Food) + static_cast<int32>(Refill) -
								  static_cast<int32>(Rules.FoodBurn));
				uint32 Cause = static_cast<uint32>(DeathCause::Natural);
				PersistentId CauseEvent;
				if (N.Food < Rules.HungerLine)
				{
					N.Hungry = static_cast<uint8>(N.Hungry < 255 ? N.Hungry + 1 : 255);
					const uint32 Deficit = Rules.HungerLine - N.Food;
					const uint32 Damage =
						Rules.HungerDamage + static_cast<uint32>(Random.Below(Deficit * Rules.HungerDeficitFactor + 1));
					N.Health = Clamp255(static_cast<int32>(N.Health) - static_cast<int32>(Extra(Damage)));
					Cause = static_cast<uint32>(Drought.IsValid() ? DeathCause::Famine : DeathCause::Starvation);
					CauseEvent = Drought;
				}
				else
				{
					N.Hungry = 0;
					N.Health = Clamp255(static_cast<int32>(N.Health) + static_cast<int32>(Rules.HealthRecovery));
				}
				if (PlagueSeverity > 0)
				{
					const uint32 S = PlagueSeverity > 3 ? 2u : PlagueSeverity - 1u;
					if (Random.Below(1000) < Rules.PlagueSharePerMille[S])
					{
						const uint32 Damage = static_cast<uint32>(Random.Below(Rules.PlagueDamage[S] + 1));
						N.Health = Clamp255(static_cast<int32>(N.Health) - static_cast<int32>(Extra(Damage)));
						if (N.Health == 0)
						{
							Cause = static_cast<uint32>(DeathCause::Plague);
							CauseEvent = Plague;
						}
					}
				}
				if (N.Health == 0)
				{
					PersonInfo& P = W.Components().GetPool(Persons.Person).Get(R.Handle);
					P.State = static_cast<uint8>(LifeState::Dead);
					P.Died = Context.Tick;
					Context.Events->Publish(Context.Tick, PersonDiedEvent, PersonPayload{P.Index, Region, Age, Cause},
											W.Entities().GetId(R.Handle), CauseEvent);
					++Deaths;
				}
			}
			if (Deaths > 0)
			{
				ReconcileRegion(W, Types, Persons, Region);
			}
		}
	}

	NeedStats MeasureNeeds(const World& W, const PersonTypes& Persons, const NeedTypes& Needs, uint32 Region)
	{
		NeedStats S;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if ((Region != 0 && P.Region != Region) || P.State != static_cast<uint8>(LifeState::Alive))
					{
						return;
					}
					const PersonNeeds* N = W.Components().GetPool(Needs.Needs).TryGet(H);
					if (N == nullptr)
					{
						return;
					}
					++S.WithNeeds;
					S.Hungry += N->Food < NeedRules{}.HungerLine ? 1u : 0u;
					S.Weak += N->Health < 128 ? 1u : 0u;
					S.FoodSum += N->Food;
					S.HealthSum += N->Health;
				});
		for (const Event& E : W.Log().All())
		{
			if (!E.Is(PersonDiedEvent))
			{
				continue;
			}
			const PersonPayload P = E.Get<PersonPayload>();
			if (Region != 0 && P.Region != Region)
			{
				continue;
			}
			switch (static_cast<DeathCause>(P.Other))
			{
			case DeathCause::Famine:
				++S.FamineDeaths;
				break;
			case DeathCause::Starvation:
				++S.StarvationDeaths;
				break;
			case DeathCause::Plague:
				++S.PlagueDeaths;
				break;
			case DeathCause::Natural:
			default:
				++S.NaturalDeaths;
				break;
			}
			S.CausedDeaths += E.Cause.IsValid() ? 1u : 0u;
		}
		return S;
	}
} // namespace Vaelen::Population
