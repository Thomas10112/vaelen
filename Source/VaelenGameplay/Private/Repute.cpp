// VAELEN - VaelenGameplay
// Phase 12 task 12.02: an opinion between any two people, and hearsay.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/deterministic/edge tests in Tests/Gameplay
#include "Vaelen/Gameplay/Repute.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Gameplay
{
	namespace
	{
		int32 Clamped(int64 Value, const ReputeRules& Rules) noexcept
		{
			return static_cast<int32>(std::min<int64>(Rules.Most, std::max<int64>(Rules.Least, Value)));
		}

	} // namespace

	/// The slot for what one person thinks of another, made if there is room and
	/// reused - oldest first - when there is not. A person is thought about by a
	/// handful, and the handful is the most recent.
	Player::Opinion* SlotFor(PersonRepute& R, uint32 Holder, SimTick Now)
	{
		for (usize i = 0; i < R.Known && i < MostThoughtOf; ++i)
		{
			if (R.Who[i].Person == Holder)
			{
				return &R.Who[i];
			}
		}
		if (R.Known < MostThoughtOf)
		{
			Player::Opinion& O = R.Who[R.Known];
			O = Player::Opinion{};
			O.Person = Holder;
			O.Last = Now;
			++R.Known;
			return &O;
		}
		usize Oldest = 0;
		for (usize i = 1; i < MostThoughtOf; ++i)
		{
			Oldest = R.Who[i].Last < R.Who[Oldest].Last ? i : Oldest;
		}
		R.Who[Oldest] = Player::Opinion{};
		R.Who[Oldest].Person = Holder;
		R.Who[Oldest].Last = Now;
		return &R.Who[Oldest];
	}

	namespace
	{
		EntityHandle HandleOf(const World& W, const Population::PersonTypes& Persons, uint32 Person)
		{
			EntityHandle Found;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const Population::PersonInfo& P)
					{
						if (P.Index == Person && Found.IsNull())
						{
							Found = H;
						}
					});
			return Found;
		}
	} // namespace

	ReputeTypes ReputeTypes::Declare(World& W)
	{
		ReputeTypes T;
		T.Repute = W.Types().Register<PersonRepute>("PersonRepute");
		W.Components().CreatePool(T.Repute);
		return T;
	}

	void ReputeSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr || Context.Random == nullptr)
		{
			return;
		}
		World& W = *Owner;
		RandomStream& Random = *Context.Random;
		// This tick's acts only. They are at the end of the log, so this walks
		// back over them and stops - it never reads the history, which is the
		// rule 10.06 set and the reason nobody here is omniscient.
		const std::vector<Event>& All = W.Log().All();
		usize First = All.size();
		while (First > 0 && All[First - 1].Tick == Context.Tick)
		{
			--First;
		}
		// Whose handle is whose, once, rather than per act: a day of a lively
		// region is thousands of acts and a person lookup walks every person.
		std::vector<std::pair<uint32, EntityHandle>> Where;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach([&](EntityHandle H, const Population::PersonInfo& P) { Where.push_back({P.Index, H}); });
		std::sort(Where.begin(), Where.end(),
				  [](const std::pair<uint32, EntityHandle>& A, const std::pair<uint32, EntityHandle>& B)
				  { return A.first < B.first; });
		auto Find = [&](uint32 Person) -> EntityHandle
		{
			const auto It =
				std::lower_bound(Where.begin(), Where.end(), Person,
								 [](const std::pair<uint32, EntityHandle>& A, uint32 B) { return A.first < B; });
			return It != Where.end() && It->first == Person ? It->second : EntityHandle{};
		};
		auto ReputeFor = [&](uint32 Person) -> PersonRepute*
		{
			const EntityHandle H = Find(Person);
			if (H.IsNull())
			{
				return nullptr;
			}
			PersonRepute* R = W.Components().GetPool(Repute.Repute).TryGet(H);
			if (R == nullptr)
			{
				PersonRepute Fresh;
				Fresh.Since = Context.Tick;
				W.Components().GetPool(Repute.Repute).Add(H, Fresh);
				R = W.Components().GetPool(Repute.Repute).TryGet(H);
			}
			return R;
		};

		for (usize i = First; i < All.size(); ++i)
		{
			if (!All[i].Is(Player::PlayerActedEvent))
			{
				continue;
			}
			const Player::ActPayload A = All[i].Get<Player::ActPayload>();
			const Player::Intent Kind = static_cast<Player::Intent>(A.Kind);
			if (A.Target == 0 || A.Person == 0 || A.Target == A.Person)
			{
				continue;
			}
			if (Kind != Player::Intent::Speak && Kind != Player::Intent::Give && Kind != Player::Intent::Take)
			{
				continue; // working and eating are nobody else's business
			}
			// 1. First hand. What was done to somebody moves what THEY think of
			//    whoever did it, which is the only opinion anybody has ever had.
			PersonRepute* Actor = ReputeFor(A.Person);
			if (Actor == nullptr)
			{
				continue;
			}
			int64 Moved = 0;
			switch (Kind)
			{
			case Player::Intent::Speak:
				Moved = Rules.ForSpeaking;
				break;
			case Player::Intent::Give:
				Moved = int64{Rules.ForGiving} + int64{Rules.PerUnitGiven} * int64{A.Amount};
				++Actor->Kindnesses;
				break;
			case Player::Intent::Take:
				Moved = int64{Rules.ForTaking} + int64{Rules.PerUnitTaken} * int64{A.Amount};
				++Actor->Wrongs;
				break;
			default:
				break;
			}
			Player::Opinion* Held = SlotFor(*Actor, A.Target, Context.Tick);
			Held->Regard = Clamped(int64{Held->Regard} + Moved, Rules);
			Held->Last = Context.Tick;
			++Held->Met;

			// 2. Hearsay, and it is the one thing no layer of this project has.
			//    A speaker may pass on what they think of somebody else, and it
			//    reaches the listener worth less than having been there.
			if (Kind != Player::Intent::Speak || Random.Below(1000) >= Rules.TellsPerMille)
			{
				continue;
			}
			const PersonRepute* Speaker = W.Components().GetPool(Repute.Repute).TryGet(Find(A.Person));
			if (Speaker == nullptr || Speaker->Known == 0)
			{
				continue; // they have nothing to tell
			}
			const usize Pick = static_cast<usize>(Random.Below(static_cast<uint32>(Speaker->Known)));
			const uint32 About = Speaker->Who[Pick].Person;
			const int32 Worth = Speaker->Who[Pick].Regard;
			if (About == 0 || About == A.Target || About == A.Person || Worth == 0)
			{
				continue; // nobody tells you what you already are
			}
			PersonRepute* Subject = ReputeFor(About);
			if (Subject == nullptr)
			{
				continue;
			}
			const int64 Story = int64{Worth} * int64{Rules.HeardPerMille} / 1000;
			Player::Opinion* Second = SlotFor(*Subject, A.Target, Context.Tick);
			Second->Regard = Clamped(int64{Second->Regard} + Story, Rules);
			Second->Last = Context.Tick;
			++Subject->Heard;
			Context.Events->Publish(Context.Tick, HeardOfEvent,
									Player::ActPayload{About, static_cast<uint32>(Player::Intent::Speak), A.Target,
													   static_cast<uint32>(Story < 0 ? -Story : Story)});
		}
		// The repute of everybody who is thought of at all: the opinions
		// together, which is what a name is worth in a place.
		W.Components()
			.GetPool(Repute.Repute)
			.ForEach(
				[&](EntityHandle, PersonRepute& R)
				{
					int64 Sum = 0;
					for (usize i = 0; i < R.Known && i < MostThoughtOf; ++i)
					{
						Sum += R.Who[i].Regard;
					}
					R.Repute = Clamped(R.Known == 0 ? 0 : Sum / static_cast<int64>(R.Known), Rules);
				});
	}

	int32 ReputeOf(const World& W, const Population::PersonTypes& Persons, const ReputeTypes& Types, uint32 Person)
	{
		const EntityHandle H = HandleOf(W, Persons, Person);
		const PersonRepute* R = H.IsNull() ? nullptr : W.Components().GetPool(Types.Repute).TryGet(H);
		return R != nullptr ? R->Repute : 0;
	}

	const Player::Opinion* OpinionOf(const World& W, const Population::PersonTypes& Persons, const ReputeTypes& Types,
									 uint32 About, uint32 Holder)
	{
		const EntityHandle H = HandleOf(W, Persons, About);
		const PersonRepute* R = H.IsNull() ? nullptr : W.Components().GetPool(Types.Repute).TryGet(H);
		if (R == nullptr)
		{
			return nullptr;
		}
		for (usize i = 0; i < R->Known && i < MostThoughtOf; ++i)
		{
			if (R->Who[i].Person == Holder)
			{
				return &R->Who[i];
			}
		}
		return nullptr;
	}

	ReputeStats MeasureRepute(const World& W, const Population::PersonTypes& Persons, const ReputeTypes& Types)
	{
		(void)Persons;
		ReputeStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		W.Components()
			.GetPool(Types.Repute)
			.ForEach(
				[&](EntityHandle, const PersonRepute& R)
				{
					if (R.Known == 0)
					{
						return;
					}
					++Out.ThoughtOf;
					Out.Opinions += R.Known;
					Out.Heard += R.Heard;
					Out.Best = R.Repute > Out.Best ? R.Repute : Out.Best;
					Out.Worst = R.Repute < Out.Worst ? R.Repute : Out.Worst;
					for (usize i = 0; i < R.Known && i < MostThoughtOf; ++i)
					{
						Digest = HashCombine(
							Digest, HashBytes(reinterpret_cast<const char*>(&R.Who[i]), sizeof(Player::Opinion)));
					}
				});
		for (const Event& E : W.Log().All())
		{
			Out.Tellings += E.Is(HeardOfEvent) ? 1u : 0u;
		}
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Gameplay
