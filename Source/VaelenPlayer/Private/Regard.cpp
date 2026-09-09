// VAELEN - VaelenPlayer
// Phase 10.06: what the people around the player make of them.
//
// STATUS: PROTOTYPE (Phase 10) - unit/integration/deterministic/edge tests in Tests/Player

#include "Vaelen/Player/Regard.h"

#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Player
{
	namespace
	{
		EntityHandle MarkedHandle(const World& W, const PlayerTypes& Player)
		{
			EntityHandle Out;
			W.Components()
				.GetPool(Player.Mark)
				.ForEach(
					[&](EntityHandle H, const PlayerMark&)
					{
						if (Out.IsNull())
						{
							Out = H;
						}
					});
			return Out;
		}

		int32 Clamped(int64 Value, const RegardRules& Rules) noexcept
		{
			const int64 Low = Rules.Least;
			const int64 High = Rules.Most;
			return static_cast<int32>(std::max(Low, std::min(High, Value)));
		}

		/// The slot holding this person's opinion, making one when there is
		/// room and taking the faintest and oldest when there is not: somebody
		/// dealt with once, long ago, is who a person stops thinking about.
		Opinion* SlotFor(PlayerRegard& R, uint32 Person, SimTick Now)
		{
			for (usize i = 0; i < R.Known && i < MostKnown; ++i)
			{
				if (R.Who[i].Person == Person)
				{
					return &R.Who[i];
				}
			}
			if (R.Known < MostKnown)
			{
				Opinion& Fresh = R.Who[R.Known++];
				Fresh = Opinion{};
				Fresh.Person = Person;
				Fresh.Last = Now;
				return &Fresh;
			}
			usize Faintest = 0;
			for (usize i = 1; i < MostKnown; ++i)
			{
				const int64 A = R.Who[i].Regard < 0 ? -int64{R.Who[i].Regard} : int64{R.Who[i].Regard};
				const int64 B =
					R.Who[Faintest].Regard < 0 ? -int64{R.Who[Faintest].Regard} : int64{R.Who[Faintest].Regard};
				const bool Fainter = A < B || (A == B && R.Who[i].Last < R.Who[Faintest].Last);
				Faintest = Fainter ? i : Faintest;
			}
			R.Who[Faintest] = Opinion{};
			R.Who[Faintest].Person = Person;
			R.Who[Faintest].Last = Now;
			return &R.Who[Faintest];
		}
	} // namespace

	RegardTypes RegardTypes::Declare(World& W)
	{
		RegardTypes T;
		T.Regard = W.Types().Register<PlayerRegard>("PlayerRegard");
		W.Components().CreatePool(T.Regard);
		return T;
	}

	void RegardSystem::Tick(TickContext& Context)
	{
		World& W = *Owner;
		const EntityHandle H = MarkedHandle(W, Player);
		if (H.IsNull())
		{
			return; // nobody is played, so nobody is being made anything of
		}
		PlayerRegard* Held = W.Components().GetPool(Regard.Regard).TryGet(H);

		// 1. What was done today. The acts of this tick are at the end of the
		// log, so this walks back over them and stops - it never reads the life.
		const std::vector<Event>& All = W.Log().All();
		usize First = All.size();
		while (First > 0 && All[First - 1].Tick == Context.Tick)
		{
			--First;
		}
		for (usize i = First; i < All.size(); ++i)
		{
			if (!All[i].Is(PlayerActedEvent))
			{
				continue;
			}
			const ActPayload A = All[i].Get<ActPayload>();
			const Intent Kind = static_cast<Intent>(A.Kind);
			if (A.Target == 0 || (Kind != Intent::Speak && Kind != Intent::Give && Kind != Intent::Take))
			{
				continue; // working and eating are nobody else's business
			}
			if (Held == nullptr)
			{
				PlayerRegard Fresh;
				Fresh.Since = Context.Tick;
				W.Components().GetPool(Regard.Regard).Add(H, Fresh);
				Held = W.Components().GetPool(Regard.Regard).TryGet(H);
				if (Held == nullptr)
				{
					return;
				}
			}
			Opinion* Who = SlotFor(*Held, A.Target, Context.Tick);
			int64 Moved = 0;
			switch (Kind)
			{
			case Intent::Speak:
				Moved = Rules.ForSpeaking;
				break;
			case Intent::Give:
				Moved = int64{Rules.ForGiving} + int64{Rules.PerUnitGiven};
				++Held->Kindnesses;
				break;
			case Intent::Take:
				Moved = int64{Rules.ForTaking} + int64{Rules.PerUnitTaken};
				++Held->Wrongs;
				break;
			default:
				break;
			}
			Who->Regard = Clamped(int64{Who->Regard} + Moved, Rules);
			Who->Last = Context.Tick;
			++Who->Met;
		}
		if (Held == nullptr)
		{
			return; // nobody has ever had cause to think anything
		}

		// 2. The world forgets. A whole year of days is worth ForgetPerYear
		// points, and the carry keeps that exact under whole numbers.
		Held->Drift += Rules.ForgetPerYear;
		const uint32 DaysPerYear = 360;
		const uint32 Steps = Held->Drift / DaysPerYear;
		Held->Drift -= Steps * DaysPerYear;
		for (usize i = 0; i < Held->Known && i < MostKnown; ++i)
		{
			Opinion& O = Held->Who[i];
			if (O.Last == Context.Tick || Steps == 0)
			{
				continue; // what happened today is not yet being forgotten
			}
			const int64 Toward = O.Regard > 0 ? -int64{Steps} : int64{Steps};
			const int64 Now = int64{O.Regard} + Toward;
			const bool Overshot = (O.Regard > 0 && Now < 0) || (O.Regard < 0 && Now > 0);
			O.Regard = O.Regard == 0 ? 0 : static_cast<int32>(Overshot ? 0 : Now);
		}

		// 3. What the place at large makes of them: every opinion, worth what
		// its holder is worth. A rank of 05.02 and not a write to it.
		int64 Sum = 0;
		int64 Weight = 0;
		for (usize i = 0; i < Held->Known && i < MostKnown; ++i)
		{
			const Opinion& O = Held->Who[i];
			const Society::PersonStanding* S = Society::StandingOf(W, Persons, Standing, O.Person);
			// An unranked holder still counts for something: the bound and the
			// young of 05.02 have no rank and are not nobody.
			const int64 Says = 1 + (S != nullptr ? int64{S->Rank} : 0);
			Sum += int64{O.Regard} * Says;
			Weight += Says;
		}
		const int64 Mean = Weight > 0 ? Sum / Weight : 0;
		Held->Repute = Clamped(Mean * int64{Rules.ReputeSharePerMille} / 1000, Rules);
	}

	const PlayerRegard* RegardOf(const World& W, const RegardTypes& Regard)
	{
		const PlayerRegard* Found = nullptr;
		W.Components()
			.GetPool(Regard.Regard)
			.ForEach(
				[&](EntityHandle, const PlayerRegard& R)
				{
					if (Found == nullptr)
					{
						Found = &R;
					}
				});
		return Found;
	}

	bool EndRegard(World& W, const RegardTypes& Regard)
	{
		std::vector<EntityHandle> Held;
		W.Components().GetPool(Regard.Regard).ForEach([&](EntityHandle H, const PlayerRegard&) { Held.push_back(H); });
		for (const EntityHandle H : Held)
		{
			W.Components().GetPool(Regard.Regard).Remove(H);
		}
		return !Held.empty();
	}

	int32 RegardFrom(const World& W, const RegardTypes& Regard, uint32 Person)
	{
		const PlayerRegard* R = RegardOf(W, Regard);
		if (R == nullptr || Person == 0)
		{
			return 0;
		}
		for (usize i = 0; i < R->Known && i < MostKnown; ++i)
		{
			if (R->Who[i].Person == Person)
			{
				return R->Who[i].Regard;
			}
		}
		return 0;
	}

	int32 ReputeOf(const World& W, const RegardTypes& Regard)
	{
		const PlayerRegard* R = RegardOf(W, Regard);
		return R != nullptr ? R->Repute : 0;
	}

	RegardStats MeasureRegard(const World& W, const Population::PersonTypes& Persons, const PlayerTypes& Player,
							  const RegardTypes& Regard, const RegardRules& Rules)
	{
		RegardStats S;
		const uint32 Played = PlayerPerson(W, Player);
		W.Components()
			.GetPool(Regard.Regard)
			.ForEach(
				[&](EntityHandle H, const PlayerRegard& R)
				{
					++S.Records;
					const Population::PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					if (Played != 0 && (P == nullptr || P->Index != Played))
					{
						++S.Bad; // an opinion of somebody who is not being played
					}
					if (R.Known > MostKnown)
					{
						++S.Bad; // more people than the table holds
					}
					S.Repute = R.Repute;
					S.Kindnesses += R.Kindnesses;
					S.Wrongs += R.Wrongs;
					for (usize i = 0; i < R.Known && i < MostKnown; ++i)
					{
						const Opinion& O = R.Who[i];
						++S.Known;
						S.Friends += O.Regard > 0 ? 1u : 0u;
						S.Enemies += O.Regard < 0 ? 1u : 0u;
						if (O.Person == 0 || O.Person == Played)
						{
							++S.Bad; // held by nobody, or by the player about themselves
						}
						if (O.Regard > Rules.Most || O.Regard < Rules.Least)
						{
							++S.Bad; // outside what the rules allow
						}
						if (O.Met == 0)
						{
							++S.Bad; // an opinion formed by nothing happening
						}
						for (usize j = 0; j < i; ++j)
						{
							S.Bad += R.Who[j].Person == O.Person ? 1u : 0u; // twice in the table
						}
					}
					if (R.Repute > Rules.Most || R.Repute < Rules.Least)
					{
						++S.Bad;
					}
				});
		if (S.Records > 1)
		{
			S.Bad += S.Records - 1u; // one to a world
		}
		return S;
	}
} // namespace Vaelen::Player
