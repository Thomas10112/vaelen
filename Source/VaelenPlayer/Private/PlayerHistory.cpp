// VAELEN - VaelenPlayer
// Phase 10.07: the player in the chronicle.
//
// STATUS: PROTOTYPE (Phase 10) - integration/text/deterministic tests in Tests/Player

#include "Vaelen/Player/PlayerHistory.h"

#include "Vaelen/Population/Lod.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>

namespace Vaelen::Player
{
	namespace
	{
		void AppendNumber(std::string& Out, uint64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
			Out += Buffer;
		}

		void AppendSigned(std::string& Out, int64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%+lld", static_cast<long long>(Value));
			Out += Buffer;
		}

		void AppendPerson(const World& W, const History::PreHistoryTypes& Types, const LifeContext& Context,
						  uint32 Person, std::string& Out, const Population::PersonIndex* Index)
		{
			std::string Name;
			Population::NamePerson(W, Types, Context.Persons, Person, Name, Index);
			Out += Name;
		}

		bool IsLifeEvent(const Event& E)
		{
			return E.Is(PlayerActedEvent) || E.Is(PlayerRefusedEvent) || E.Is(Population::PersonMovedEvent);
		}

		/// "gave", "took", "spoke with": the verb as it is remembered rather
		/// than as it was meant.
		const char* Told(Intent Kind) noexcept
		{
			switch (Kind)
			{
			case Intent::Wait:
				return "let the day go by";
			case Intent::Work:
				return "worked";
			case Intent::Rest:
				return "rested";
			case Intent::Eat:
				return "ate";
			case Intent::Move:
				return "walked";
			case Intent::Speak:
				return "spoke with";
			case Intent::Give:
				return "gave to";
			case Intent::Take:
				return "took from";
			case Intent::None:
			case Intent::Count:
			default:
				break;
			}
			return "did something";
		}

		bool AimedAtSomebody(Intent Kind) noexcept
		{
			return Kind == Intent::Speak || Kind == Intent::Give || Kind == Intent::Take;
		}
	} // namespace

	LifeChronicleTypes LifeChronicleTypes::Declare(World& W)
	{
		LifeChronicleTypes T;
		T.State = W.Types().Register<LifeChronicleState>("LifeChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void LifeChronicle::Attach()
	{
		EventBus& Bus = Owner->Events();
		Bus.Subscribe(PlayerActedEvent.TypeHash, this);
		Bus.Subscribe(PlayerRefusedEvent.TypeHash, this);
		Bus.Subscribe(Population::PersonMovedEvent.TypeHash, this);
	}

	bool LifeChronicle::Matters(const Event& E, uint32& Person) const
	{
		const World& W = *Owner;
		const uint32 Played = PlayerPerson(W, Context.Player);
		if (E.Is(Population::PersonMovedEvent))
		{
			const Population::PersonPayload P = E.Get<Population::PersonPayload>();
			Person = P.Person;
			// Everybody's walk is a fact; only the played person's is this
			// chronicle's business.
			return Rules.RecordDoings != 0 && Played != 0 && P.Person == Played;
		}
		const ActPayload A = E.Get<ActPayload>();
		Person = A.Person;
		if (E.Is(PlayerRefusedEvent))
		{
			return Rules.RecordRefusals != 0;
		}
		const Intent Kind = static_cast<Intent>(A.Kind);
		// A day of work is not history. What touched somebody else is.
		return AimedAtSomebody(Kind) ? Rules.RecordDoings != 0 : Rules.RecordSmallDoings != 0;
	}

	void LifeChronicle::OnEvent(const Event& E)
	{
		World& W = *Owner;
		LifeChronicleState* S = nullptr;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, LifeChronicleState& St)
				{
					if (S == nullptr)
					{
						S = &St;
					}
				});
		if (S == nullptr)
		{
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(State.State).Add(H, LifeChronicleState{});
			W.Components()
				.GetPool(State.State)
				.ForEach(
					[&](EntityHandle, LifeChronicleState& St)
					{
						if (S == nullptr)
						{
							S = &St;
						}
					});
			if (S == nullptr)
			{
				return;
			}
		}
		uint32 Person = 0;
		if (!Matters(E, Person))
		{
			return;
		}
		const uint32 Year = static_cast<uint32>(E.Tick / History::TicksPerYear);
		if (S->Year != Year || S->Person != Person)
		{
			S->Year = Year;
			S->Person = Person;
			S->InYear = 0;
		}
		if (S->InYear >= Rules.MaxRecordsPerYear)
		{
			++S->Dropped;
			return; // a life is not a ledger
		}
		++S->InYear;
		++S->Records;
		History::RecordInfo R;
		R.Event = E.Id.Value;
		R.Tick = E.Tick;
		R.Type = E.TypeHash;
		R.Subject = E.Subject.Value;
		R.Era = History::EraAt(W, Types.History, E.Tick);
		R.Region = 0;
		const EntityHandle H = W.CreateEntity(IdKind::Document);
		W.Components().GetPool(Types.History.Record).Add(H, R);
		History::HistoryState* HS = nullptr;
		W.Components()
			.GetPool(Types.History.State)
			.ForEach(
				[&](EntityHandle, History::HistoryState& St)
				{
					if (HS == nullptr)
					{
						HS = &St;
					}
				});
		if (HS != nullptr)
		{
			++HS->RecordCount;
		}
	}

	void NamePlayed(const World& W, const History::PreHistoryTypes& Types, const LifeContext& Context, std::string& Out,
					const Population::PersonIndex* Index)
	{
		const uint32 Played = PlayerPerson(W, Context.Player);
		if (Played == 0)
		{
			Out += "nobody";
			return;
		}
		AppendPerson(W, Types, Context, Played, Out, Index);
	}

	void DescribeLifeEvent(const World& W, const History::PreHistoryTypes& Types, const LifeContext& Context,
						   const Event& E, std::string& Out, const Population::PersonIndex* Index)
	{
		if (!IsLifeEvent(E))
		{
			// Down through every layer that was built before this one.
			if (Context.Works != nullptr)
			{
				Infrastructure::DescribeWorksEvent(W, Types, *Context.Works, E, Out, Index);
			}
			else if (Context.Goods != nullptr)
			{
				Economy::DescribeEconomyEvent(W, Types, *Context.Goods, E, Out, Index);
			}
			else
			{
				Population::DescribePersonEvent(W, Types, Context.Persons, Context.Families, E, Out, Index);
			}
			return;
		}
		// The year and the age, in the same hand as every layer below.
		std::string Prefix;
		History::DescribeEvent(W, Types, E, Prefix);
		const usize Colon = Prefix.find(": ");
		Out.clear();
		Out += Colon != std::string::npos ? Prefix.substr(0, Colon + 2) : std::string();

		if (E.Is(Population::PersonMovedEvent))
		{
			const Population::PersonPayload P = E.Get<Population::PersonPayload>();
			AppendPerson(W, Types, Context, P.Person, Out, Index);
			Out += " walked from ";
			std::string Name;
			History::NameRegion(W, Types, P.Other, Name);
			Out += Name;
			Out += " to ";
			Name.clear();
			History::NameRegion(W, Types, P.Region, Name);
			Out += Name;
			Out += '.';
			return;
		}
		const ActPayload A = E.Get<ActPayload>();
		const Intent Kind = static_cast<Intent>(A.Kind);
		AppendPerson(W, Types, Context, A.Person, Out, Index);
		if (E.Is(PlayerRefusedEvent))
		{
			Out += " could not ";
			Out += Told(Kind);
			if (AimedAtSomebody(Kind) && A.Target != 0)
			{
				Out += ' ';
				AppendPerson(W, Types, Context, A.Target, Out, Index);
			}
			Out += ": ";
			Out += RefusalName(static_cast<Refusal>(A.Amount));
			Out += '.';
			return;
		}
		Out += ' ';
		Out += Told(Kind);
		if (AimedAtSomebody(Kind) && A.Target != 0)
		{
			Out += ' ';
			AppendPerson(W, Types, Context, A.Target, Out, Index);
		}
		else if (Kind == Intent::Move && A.Target != 0)
		{
			Out += " to ";
			std::string Name;
			History::NameRegion(W, Types, A.Target, Name);
			Out += Name;
		}
		if (A.Amount > 0 && Kind != Intent::Move)
		{
			Out += ", ";
			AppendNumber(Out, A.Amount);
			Out += A.Amount == 1 ? " hour of it" : " hours of it";
		}
		Out += '.';
	}

	uint32 ExportChronicleWithLife(const World& W, const History::PreHistoryTypes& Types, const LifeContext& Context,
								   std::string& Out, uint32 MaxLines)
	{
		std::vector<History::RecordInfo> Records;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach([&](EntityHandle, const History::RecordInfo& R) { Records.push_back(R); });
		std::sort(Records.begin(), Records.end(), [](const History::RecordInfo& A, const History::RecordInfo& B)
				  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		uint32 Lines = 0;
		std::string Line;
		for (const History::RecordInfo& R : Records)
		{
			if (MaxLines != 0 && Lines >= MaxLines)
			{
				break;
			}
			const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
			Line.clear();
			if (E != nullptr)
			{
				DescribeLifeEvent(W, Types, Context, *E, Line, &Index);
			}
			else
			{
				History::DescribeRecord(W, Types, R, Line);
			}
			Out += Line;
			Out += '\n';
			++Lines;
		}
		return Lines;
	}

	uint32 ExportWhyWithLife(const World& W, const History::PreHistoryTypes& Types, const LifeContext& Context,
							 PersistentId Id, std::string& Out)
	{
		std::vector<History::WhyStep> Steps;
		History::Why(W, Types, Id, Steps);
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		uint32 Lines = 0;
		std::string Line;
		for (const History::WhyStep& Step : Steps)
		{
			if (Step.Cause == nullptr)
			{
				continue;
			}
			Line.clear();
			DescribeLifeEvent(W, Types, Context, *Step.Cause, Line, &Index);
			if (Lines > 0)
			{
				const usize Colon = Line.find(": ");
				Line = "  because " + (Colon != std::string::npos ? Line.substr(Colon + 2) : Line);
			}
			Out += Line;
			Out += '\n';
			++Lines;
		}
		return Lines;
	}

	void LifeTimeline(const World& W, const LifeContext& Context, std::vector<const Event*>& Out)
	{
		Out.clear();
		const uint32 Played = PlayerPerson(W, Context.Player);
		if (Played == 0)
		{
			return;
		}
		for (const Event& E : W.Log().All())
		{
			if (E.Is(PlayerActedEvent) || E.Is(PlayerRefusedEvent))
			{
				const ActPayload A = E.Get<ActPayload>();
				if (A.Person == Played)
				{
					Out.push_back(&E);
				}
				continue;
			}
			if (E.Is(Population::PersonMovedEvent) || E.Is(Population::PersonBornEvent) ||
				E.Is(Population::PersonDiedEvent))
			{
				const Population::PersonPayload P = E.Get<Population::PersonPayload>();
				if (P.Person == Played)
				{
					Out.push_back(&E); // what the world did to them, not only what they did
				}
			}
		}
	}

	uint32 ExportLife(const World& W, const History::PreHistoryTypes& Types, const LifeContext& Context,
					  std::string& Out, uint32 MaxActs)
	{
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		uint32 Lines = 0;
		std::string Line;

		// Who they are: a person of this world, named the way the world names
		// everybody, and nothing about them that the world does not hold.
		const uint32 Played = PlayerPerson(W, Context.Player);
		Line.clear();
		NamePlayed(W, Types, Context, Line, &Index);
		Out += Line;
		if (Played != 0)
		{
			const Population::PersonInfo* P = Population::FindPerson(W, Context.Persons, Played);
			if (P != nullptr)
			{
				Out += " of ";
				std::string Name;
				History::NameRegion(W, Types, P->Region, Name);
				Out += Name;
				if (P->Family != 0)
				{
					Out += ", ";
					Name.clear();
					Population::NameFamily(W, Types, Context.Persons, Context.Families, P->Family, Name, &Index);
					Out += Name;
				}
			}
		}
		Out += '\n';
		++Lines;

		// What they did, in the order they did it.
		std::vector<const Event*> Timeline;
		LifeTimeline(W, Context, Timeline);
		uint32 Acts = 0;
		for (const Event* E : Timeline)
		{
			if (MaxActs != 0 && Acts >= MaxActs)
			{
				break;
			}
			Line.clear();
			DescribeLifeEvent(W, Types, Context, *E, Line, &Index);
			Out += Line;
			Out += '\n';
			++Lines;
			++Acts;
		}

		// Who knows them, and what those people make of it (10.06).
		const PlayerRegard* Known = RegardOf(W, Context.Regard);
		if (Known != nullptr && Known->Known > 0)
		{
			for (usize i = 0; i < Known->Known && i < MostKnown; ++i)
			{
				const Opinion& O = Known->Who[i];
				Out += "  ";
				Line.clear();
				AppendPerson(W, Types, Context, O.Person, Line, &Index);
				Out += Line;
				Out += O.Regard > 0 ? " thinks well of them ("
									: (O.Regard < 0 ? " thinks ill of them (" : " has no view of them (");
				AppendSigned(Out, O.Regard);
				Out += ").\n";
				++Lines;
			}
			Out += "  the place at large: ";
			AppendSigned(Out, Known->Repute);
			Out += ".\n";
			++Lines;
		}

		// And the why of the last thing that happened because of them, walked
		// back through every layer under the player.
		PersistentId Last;
		for (const Event& E : W.Log().All())
		{
			if (E.Cause.IsValid())
			{
				for (const Event* Act : Timeline)
				{
					if (Act->Id == E.Cause)
					{
						Last = E.Id;
						break;
					}
				}
			}
		}
		if (Last.IsValid())
		{
			Out += "why:\n";
			++Lines;
			Lines += ExportWhyWithLife(W, Types, Context, Last, Out);
		}
		return Lines;
	}

	LifeChronicleStats CheckLifeChronicle(const World& W, const History::PreHistoryTypes& Types,
										  const LifeContext& Context, const LifeChronicleTypes& State)
	{
		LifeChronicleStats S;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, const LifeChronicleState& St)
				{
					S.Records += St.Records;
					S.Dropped += St.Dropped;
				});
		const uint32 Played = PlayerPerson(W, Context.Player);
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		std::string Line;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const History::RecordInfo& R)
				{
					const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
					if (E == nullptr || !IsLifeEvent(*E))
					{
						return;
					}
					Line.clear();
					DescribeLifeEvent(W, Types, Context, *E, Line, &Index);
					S.Described += Line.empty() ? 0u : 1u;
					S.EraConsistent += R.Era == History::EraAt(W, Types.History, R.Tick) ? 1u : 0u;
					if (E->Is(Population::PersonMovedEvent))
					{
						++S.ByType[2];
						const Population::PersonPayload P = E->Get<Population::PersonPayload>();
						S.OfThePlayer += Played != 0 && P.Person == Played ? 1u : 0u;
						return;
					}
					const ActPayload A = E->Get<ActPayload>();
					S.OfThePlayer += Played != 0 && A.Person == Played ? 1u : 0u;
					++S.ByType[E->Is(PlayerRefusedEvent) ? 1 : 0];
				});
		return S;
	}
} // namespace Vaelen::Player
