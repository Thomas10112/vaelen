// VAELEN - VaelenGameplay
// Phase 12 task 12.06: what the world does about a name.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/deterministic tests in Tests/Gameplay/Test_Judgement.cpp
#include "Vaelen/Gameplay/Judgement.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Gameplay
{
	namespace
	{
		bool IsAlive(const Population::PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(Population::LifeState::Alive);
		}
	} // namespace

	void JudgementSystem::Tick(TickContext& Context)
	{
		World& W = *Owner;

		// What every place says, by region index. Read once: the pool is walked
		// for the whole world rather than per person.
		std::vector<uint32> Where;
		std::vector<const RegionNames*> Said;
		{
			std::vector<std::pair<uint32, const RegionNames*>> Found;
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						const RegionNames* N = W.Components().GetPool(Fame_.Names).TryGet(H);
						if (R.Index != 0 && N != nullptr && N->Count != 0)
						{
							Found.emplace_back(R.Index, N);
						}
					});
			std::sort(Found.begin(), Found.end(),
					  [](const std::pair<uint32, const RegionNames*>& A, const std::pair<uint32, const RegionNames*>& B)
					  { return A.first < B.first; });
			for (const std::pair<uint32, const RegionNames*>& P : Found)
			{
				Where.push_back(P.first);
				Said.push_back(P.second);
			}
		}
		if (Where.empty())
		{
			return; // nowhere has heard of anybody, so nowhere has anything to answer
		}

		// Who is standing where, and whether they are already bound. In person
		// order, so the same world judges the same people in the same order.
		struct Standing
		{
			uint32 Person = 0;
			uint32 Region = 0;
			uint32 Age = 0;
			bool Bound = false;
		};
		std::vector<Standing> People;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (!IsAlive(P) || P.Region == 0)
					{
						return;
					}
					People.push_back(Standing{P.Index, P.Region,
											  static_cast<uint32>(Population::AgeYears(P, Context.Tick)),
											  W.Components().GetPool(Bondage.Bond).TryGet(H) != nullptr});
				});
		std::sort(People.begin(), People.end(),
				  [](const Standing& A, const Standing& B) { return A.Person < B.Person; });

		// A place acts on the names it carries, and on nobody else. Somebody
		// whose name is not among the handful is not judged - not spared by a
		// rule, simply never spoken of.
		for (usize i = 0; i < Where.size(); ++i)
		{
			const uint32 Region = Where[i];
			const RegionNames& N = *Said[i];
			uint32 Done = 0;
			// Worst first, so a place that can only act four times a year acts on
			// the four it likes least rather than on the four it happens to hit.
			std::vector<Fame> Names;
			for (uint32 k = 0; k < N.Count && k < MostNames; ++k)
			{
				Names.push_back(N.Who[k]);
			}
			std::sort(Names.begin(), Names.end(), [](const Fame& A, const Fame& B)
					  { return A.Said != B.Said ? A.Said < B.Said : A.Person < B.Person; });

			for (const Fame& F : Names)
			{
				if (Done >= Rules.MostPerYear)
				{
					break;
				}
				const bool Bad = F.Said <= Rules.BindUnder;
				const bool Good = F.Said >= Rules.FreeOver;
				if (!Bad && !Good)
				{
					continue; // a name the place has no quarrel with and owes nothing to
				}
				// The person has to be HERE. A name travels; a body does not, and
				// a place cannot bind somebody standing three roads away.
				const auto At = std::lower_bound(People.begin(), People.end(), F.Person,
												 [](const Standing& S, uint32 P) { return S.Person < P; });
				if (At == People.end() || At->Person != F.Person || At->Region != Region || At->Age < Rules.FromAge)
				{
					continue;
				}
				if (Bad && !At->Bound)
				{
					// Caused by the telling that brought the name here, so 12.07
					// can walk a bondage back to what the place was TOLD.
					if (Society::BindPerson(W, Persons, Bondage, F.Person, Society::BondKind::Bonded,
											Society::BondEntry::Judgement, 0u, Context.Tick, F.First))
					{
						At->Bound = true;
						++Done;
						if (Context.Events != nullptr)
						{
							Context.Events->Publish(Context.Tick, CondemnedEvent,
													FamePayload{F.Person, Region, F.Said, F.Hops}, PersistentId{},
													F.First);
						}
					}
				}
				else if (Good && At->Bound)
				{
					if (Society::FreePerson(W, Persons, Bondage, F.Person, Society::BondExit::Manumission, Context.Tick,
											F.First))
					{
						At->Bound = false;
						++Done;
						if (Context.Events != nullptr)
						{
							Context.Events->Publish(Context.Tick, PardonedEvent,
													FamePayload{F.Person, Region, F.Said, F.Hops}, PersistentId{},
													F.First);
						}
					}
				}
			}
		}
	}

	JudgementStats MeasureJudgement(const World& W, const Population::PersonTypes& Persons,
									const Society::BondageTypes& Bondage)
	{
		JudgementStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const Event& E : W.Log().All())
		{
			if (!E.Is(CondemnedEvent) && !E.Is(PardonedEvent))
			{
				continue;
			}
			const FamePayload& P = E.Get<FamePayload>();
			(E.Is(CondemnedEvent) ? Out.Condemned : Out.Pardoned) += 1u;
			Out.WorstBound = E.Is(CondemnedEvent) && P.Said < Out.WorstBound ? P.Said : Out.WorstBound;
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&P), sizeof(FamePayload)));
		}
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (P.State != static_cast<uint8>(Population::LifeState::Alive))
					{
						return;
					}
					const Society::BondState* B = W.Components().GetPool(Bondage.Bond).TryGet(H);
					Out.BoundNow +=
						B != nullptr && B->Entry == static_cast<uint8>(Society::BondEntry::Judgement) ? 1u : 0u;
				});
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Gameplay
