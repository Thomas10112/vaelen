// VAELEN - VaelenPopulation
// Phase 04.02: ageing, mortality, fertility and the reconciliation of the grains.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population

#include "Vaelen/Population/Lives.h"

#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Population
{
	using History::RegionFaith;
	using History::RegionPopulation;

	namespace
	{
		EntityHandle RegionHandle(const World& W, const History::PreHistoryTypes& Types, uint32 Region)
		{
			EntityHandle Found;
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (R.Index == Region && Found.IsNull())
						{
							Found = H;
						}
					});
			return Found;
		}

		struct Living
		{
			EntityHandle Handle;
			PersonInfo Info;
		};
	} // namespace

	uint32 AgeYears(const PersonInfo& P, uint64 Tick) noexcept
	{
		return Tick > P.Born ? static_cast<uint32>((Tick - P.Born) / History::TicksPerYear) : 0u;
	}

	uint32 BandOf(uint32 Age, const LifeRules& Rules) noexcept
	{
		for (uint32 B = 0; B < LifeRules::Bands; ++B)
		{
			if (Age < Rules.BandEnd[B])
			{
				return B;
			}
		}
		return LifeRules::Bands - 1;
	}

	bool ReconcileRegion(World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons, uint32 Region)
	{
		const EntityHandle H = RegionHandle(W, Types, Region);
		if (H.IsNull() || W.Components().GetPool(Persons.Detail).TryGet(H) == nullptr)
		{
			return false;
		}
		const RegionCensus C = CountPersons(W, Persons, Region);
		if (RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(H))
		{
			for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
			{
				if (Counts->Culture[S] != 0)
				{
					Counts->Remove(Counts->Culture[S], Counts->Count[S]);
				}
			}
			for (uint32 K = 0; K < RegionPopulation::MaxCultures; ++K)
			{
				if (C.CultureOf[K] != 0 && C.ByCulture[K] > 0)
				{
					Counts->Add(C.CultureOf[K], C.ByCulture[K]);
				}
			}
			Counts->Recount();
		}
		RegionFaith* F = W.Components().GetPool(Types.Religion.Faith).TryGet(H);
		if (F == nullptr && C.Alive > C.Faithless)
		{
			F = &W.Components().GetPool(Types.Religion.Faith).Add(H, RegionFaith{});
		}
		if (F != nullptr)
		{
			for (uint32 S = 0; S < RegionFaith::MaxFaiths; ++S)
			{
				if (F->Religion[S] != 0)
				{
					F->Remove(F->Religion[S], F->Adherents[S]);
				}
			}
			for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
			{
				if (C.FaithOf[K] != 0 && C.ByFaith[K] > 0)
				{
					F->Add(C.FaithOf[K], C.ByFaith[K]);
				}
			}
			F->Recount();
		}
		return true;
	}

	void LifeSystem::Tick(TickContext& Context)
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
		uint32 NextIndex = 0;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach([&](EntityHandle, const PersonInfo& P) { NextIndex = P.Index > NextIndex ? P.Index : NextIndex; });

		for (const uint32 Region : Regions)
		{
			const EntityHandle RH = RegionHandle(W, Types, Region);
			if (RH.IsNull())
			{
				continue;
			}
			// The living of the region, in index order (the draw order).
			std::vector<Living> People;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive))
						{
							People.push_back(Living{H, P});
						}
					});
			std::sort(People.begin(), People.end(),
					  [](const Living& A, const Living& B) { return A.Info.Index < B.Info.Index; });

			// 1. Deaths by age band.
			std::vector<uint8> Died(People.size(), 0);
			for (usize i = 0; i < People.size(); ++i)
			{
				const uint32 Age = AgeYears(People[i].Info, Context.Tick);
				if (Random.Below(1000) < Rules.DeathsPerMille[BandOf(Age, Rules)])
				{
					Died[i] = 1;
					PersonInfo& P = W.Components().GetPool(Persons.Person).Get(People[i].Handle);
					P.State = static_cast<uint8>(LifeState::Dead);
					P.Died = Context.Tick;
					Context.Events->Publish(Context.Tick, PersonDiedEvent, PersonPayload{P.Index, Region, Age, 0},
											W.Entities().GetId(People[i].Handle));
				}
			}

			// 2. Births: every fertile woman may bear a child of a fertile man of
			//    her culture, the chance scaled by the room left in the region.
			const RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			const uint32 Capacity = Counts != nullptr ? Counts->Capacity : 0u;
			uint32 Alive = 0;
			for (usize i = 0; i < People.size(); ++i)
			{
				Alive += Died[i] == 0 ? 1u : 0u;
			}
			uint32 Chance = Rules.MinimumBirthsPerMille;
			if (Capacity > Alive)
			{
				const uint64 Scaled = uint64{Rules.BirthsPerMille} * (Capacity - Alive) / Capacity;
				Chance =
					Scaled > Rules.MinimumBirthsPerMille ? static_cast<uint32>(Scaled) : Rules.MinimumBirthsPerMille;
			}
			std::vector<PersonInfo> Newborn;
			for (usize i = 0; i < People.size(); ++i)
			{
				const PersonInfo& Mother = People[i].Info;
				if (Died[i] != 0 || Mother.Sex != static_cast<uint8>(Sex::Female))
				{
					continue;
				}
				const uint32 Age = AgeYears(Mother, Context.Tick);
				if (Age < Rules.FertileFrom || Age >= Rules.FertileTo ||
					(Rules.SpouseRequired != 0 && Mother.Spouse == 0) || Random.Below(1000) >= Chance)
				{
					continue;
				}
				// Her husband when she has one; otherwise a father of her culture, the
				// n-th eligible man in index order.
				if (Mother.Spouse != 0)
				{
					PersonInfo Child;
					Child.Index = ++NextIndex;
					Child.Region = Region;
					Child.Culture = Mother.Culture;
					Child.Religion = Mother.Religion;
					Child.Language = Mother.Language;
					Child.Family = Mother.Family;
					Child.Mother = Mother.Index;
					Child.Father = Mother.Spouse;
					Child.Born = Context.Tick;
					Child.Sex = static_cast<uint8>(Random.Below(1000) < Rules.FemalePerMille ? Sex::Female : Sex::Male);
					Child.State = static_cast<uint8>(LifeState::Alive);
					Child.Identity = Noise::LatticeHash(W.Config().Seed ^ 0x424f524eull,
														static_cast<int32>(Child.Index), static_cast<int32>(Region));
					Newborn.push_back(Child);
					continue;
				}
				uint32 Eligible = 0;
				for (usize j = 0; j < People.size(); ++j)
				{
					const PersonInfo& M = People[j].Info;
					const uint32 MA = AgeYears(M, Context.Tick);
					Eligible += Died[j] == 0 && M.Sex == static_cast<uint8>(Sex::Male) && M.Culture == Mother.Culture &&
										MA >= Rules.FatherFrom && MA < Rules.FatherTo
									? 1u
									: 0u;
				}
				if (Eligible == 0)
				{
					continue;
				}
				uint32 Pick = static_cast<uint32>(Random.Below(Eligible));
				uint32 Father = 0;
				for (usize j = 0; j < People.size() && Father == 0; ++j)
				{
					const PersonInfo& M = People[j].Info;
					const uint32 MA = AgeYears(M, Context.Tick);
					if (Died[j] == 0 && M.Sex == static_cast<uint8>(Sex::Male) && M.Culture == Mother.Culture &&
						MA >= Rules.FatherFrom && MA < Rules.FatherTo)
					{
						if (Pick == 0)
						{
							Father = M.Index;
						}
						else
						{
							--Pick;
						}
					}
				}
				PersonInfo Child;
				Child.Index = ++NextIndex;
				Child.Region = Region;
				Child.Culture = Mother.Culture;
				Child.Religion = Mother.Religion;
				Child.Language = Mother.Language;
				Child.Family = Mother.Family;
				Child.Mother = Mother.Index;
				Child.Father = Father;
				Child.Born = Context.Tick;
				Child.Sex = static_cast<uint8>(Random.Below(1000) < Rules.FemalePerMille ? Sex::Female : Sex::Male);
				Child.State = static_cast<uint8>(LifeState::Alive);
				Child.Identity = Noise::LatticeHash(W.Config().Seed ^ 0x424f524eull, static_cast<int32>(Child.Index),
													static_cast<int32>(Region));
				Newborn.push_back(Child);
			}
			for (const PersonInfo& Child : Newborn)
			{
				const EntityHandle E = W.CreateEntity(IdKind::Person);
				W.Components().GetPool(Persons.Person).Add(E, Child);
				Context.Events->Publish(Context.Tick, PersonBornEvent,
										PersonPayload{Child.Index, Region, 0, Child.Mother}, W.Entities().GetId(E));
			}

			// 3. The counts follow the persons.
			ReconcileRegion(W, Types, Persons, Region);
		}
	}

	LifeStats MeasureLives(const World& W, const PersonTypes& Persons, uint32 Region, uint64 Tick)
	{
		LifeStats S;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					if (Region != 0 && P.Region != Region)
					{
						return;
					}
					if (P.State != static_cast<uint8>(LifeState::Alive))
					{
						++S.Dead;
						return;
					}
					++S.Alive;
					const uint32 Age = AgeYears(P, Tick);
					S.AgeSum += Age;
					S.Oldest = Age > S.Oldest ? Age : S.Oldest;
					S.Children += Age < 15 ? 1u : 0u;
					S.Elders += Age >= 60 ? 1u : 0u;
					S.BornHere += P.Mother != 0 ? 1u : 0u;
				});
		return S;
	}
} // namespace Vaelen::Population
