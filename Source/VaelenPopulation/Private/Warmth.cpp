// VAELEN - VaelenPopulation
// Phase 18.05: the warmth type and its measure. The two verbs that move a
// person's chill live in Needs.cpp beside FeedPerson, on the same lookup.
//
// STATUS: PROTOTYPE (Phase 18) - unit/deterministic tests in Tests/Population

#include "Vaelen/Population/Warmth.h"

#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Sim/World.h"

namespace Vaelen::Population
{
	WarmthTypes WarmthTypes::Declare(World& W)
	{
		WarmthTypes T;
		T.Warmth = W.Types().Register<PersonWarmth>("PersonWarmth");
		W.Components().CreatePool(T.Warmth);
		return T;
	}

	WarmthStats MeasureWarmth(const World& W, const PersonTypes& Persons, const WarmthTypes& Warmth,
							  const WarmthRules& Rules, uint32 Region)
	{
		WarmthStats S;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if ((Region != 0 && P.Region != Region) || P.State != static_cast<uint8>(LifeState::Alive))
					{
						return;
					}
					const PersonWarmth* C = W.Components().GetPool(Warmth.Warmth).TryGet(H);
					if (C == nullptr)
					{
						return;
					}
					++S.WithWarmth;
					S.Cold += C->Chill > Rules.ChillLine ? 1u : 0u;
					S.ChillSum += C->Chill;
				});
		for (const Event& E : W.Log().All())
		{
			if (!E.Is(PersonDiedEvent))
			{
				continue;
			}
			const PersonPayload P = E.Get<PersonPayload>();
			if ((Region == 0 || P.Region == Region) && P.Other == static_cast<uint32>(DeathCause::Cold))
			{
				++S.ColdDeaths;
			}
		}
		return S;
	}
} // namespace Vaelen::Population
