// VAELEN - VaelenView
// Phase 13 task 13.08b's missing half: the PEOPLE, as a renderer needs them.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/deterministic tests in Tests/View/Test_Folk.cpp
#include "Vaelen/View/Folk.h"

#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::View
{
	// The one place allowed to see both the enum and the view constant, exactly
	// as Land.cpp is for BiomeKinds.
	static_assert(AliveState == static_cast<uint8>(Population::LifeState::Alive),
				  "View::AliveState must match Population::LifeState::Alive");

	bool IsAlive(const PersonView& P)
	{
		return P.State == AliveState;
	}

	void TakePeopleView(const World& W, const ViewSources& From, PeopleView& Out)
	{
		Out.People.clear();
		Out.Tick = static_cast<uint64>(W.Now());
		Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		Out.Living = 0;
		W.Components()
			.GetPool(From.Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& P)
				{
					PersonView V;
					V.Index = P.Index;
					V.Region = P.Region;
					V.Family = P.Family;
					V.Culture = P.Culture;
					V.Religion = P.Religion;
					// Age at THIS frame, not at death: a renderer draws whoever
					// is in front of it now. Born may precede the world's first
					// tick, so the subtraction is guarded rather than assumed.
					const uint64 Until = P.Died != 0 && P.Died < Out.Tick ? P.Died : Out.Tick;
					V.Years = Until > P.Born ? static_cast<uint32>((Until - P.Born) / History::TicksPerYear) : 0u;
					V.Spouse = P.Spouse;
					V.Sex = P.Sex;
					V.State = P.State;
					V.Identity = P.Identity;
					Out.People.push_back(V);
				});
		// Index order, always. The pool hands them out in whatever order it
		// stores them, and a renderer keeping an array in step with this one
		// cannot do that if the order rides on pool order - the same rule
		// RouteView and ColonyView are kept under.
		std::sort(Out.People.begin(), Out.People.end(),
				  [](const PersonView& A, const PersonView& B) { return A.Index < B.Index; });
		for (const PersonView& V : Out.People)
		{
			Out.Living += IsAlive(V) ? 1u : 0u;
		}
	}

	const PersonView* PersonIn(const PeopleView& V, uint32 Index)
	{
		if (Index == 0)
		{
			return nullptr;
		}
		const auto It = std::lower_bound(V.People.begin(), V.People.end(), Index,
										 [](const PersonView& A, uint32 Want) { return A.Index < Want; });
		return It != V.People.end() && It->Index == Index ? &*It : nullptr;
	}

	void PeopleOfRegion(const PeopleView& V, uint32 Region, std::vector<PersonView>& Out)
	{
		for (const PersonView& P : V.People)
		{
			if (P.Region == Region)
			{
				Out.push_back(P);
			}
		}
	}

	PeopleStats MeasurePeopleView(const PeopleView& V)
	{
		PeopleStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		uint32 Most = 0;
		for (const PersonView& P : V.People)
		{
			Most = P.Region > Most ? P.Region : Most;
		}
		std::vector<uint8> Seen(static_cast<usize>(Most) + 1u, uint8{0});
		for (const PersonView& P : V.People)
		{
			++Out.People;
			const bool Alive = IsAlive(P);
			Out.Living += Alive ? 1u : 0u;
			Out.Oldest = Alive && P.Years > Out.Oldest ? P.Years : Out.Oldest;
			if (P.Region != 0 && Seen[P.Region] == 0)
			{
				Seen[P.Region] = 1;
				++Out.Regions;
			}
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&P), sizeof(PersonView)));
		}
		Out.Bytes = static_cast<uint32>(sizeof(PeopleView) + V.People.size() * sizeof(PersonView));
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::View
