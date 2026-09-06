// VAELEN - VaelenPopulation
// Phase 04.07: persons in history.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic tests in Tests/Population

#include "Vaelen/Population/PersonHistory.h"

#include "Vaelen/Population/Needs.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>

namespace Vaelen::Population
{
	namespace
	{
		void Append(std::string& Out, const char* Text)
		{
			Out += Text;
		}

		void AppendNumber(std::string& Out, uint64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
			Out += Buffer;
		}

		EntityHandle HandleOfPerson(const World& W, const PersonTypes& Persons, uint32 Person, const PersonIndex* Index)
		{
			EntityHandle Found;
			if (Person == 0)
			{
				return Found;
			}
			if (Index != nullptr)
			{
				return Person < Index->Handles.size() ? Index->Handles[Person] : Found;
			}
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Index == Person && Found.IsNull())
						{
							Found = H;
						}
					});
			return Found;
		}

		const FamilyInfo* FamilyByIndex(const World& W, const FamilyTypes& Families, uint32 Family)
		{
			const FamilyInfo* Found = nullptr;
			if (Family == 0)
			{
				return Found;
			}
			W.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle, const FamilyInfo& F)
					{
						if (F.Index == Family && Found == nullptr)
						{
							Found = &F;
						}
					});
			return Found;
		}

		bool IsHead(const World& W, const FamilyTypes& Families, uint32 Person)
		{
			bool Head = false;
			if (Person == 0)
			{
				return false;
			}
			W.Components()
				.GetPool(Families.Family)
				.ForEach([&](EntityHandle, const FamilyInfo& F) { Head = Head || F.Head == Person; });
			return Head;
		}

		void AppendCause(uint32 Cause, std::string& Out)
		{
			switch (static_cast<DeathCause>(Cause))
			{
			case DeathCause::Famine:
				Append(Out, " of famine");
				break;
			case DeathCause::Starvation:
				Append(Out, " of hunger");
				break;
			case DeathCause::Plague:
				Append(Out, " of plague");
				break;
			case DeathCause::Natural:
			default:
				break;
			}
		}

		PersonChronicleState* FindState(World& W, const PersonChronicleTypes& Types)
		{
			PersonChronicleState* Found = nullptr;
			W.Components()
				.GetPool(Types.State)
				.ForEach(
					[&](EntityHandle, PersonChronicleState& S)
					{
						if (Found == nullptr)
						{
							Found = &S;
						}
					});
			return Found;
		}
	} // namespace

	PersonChronicleTypes PersonChronicleTypes::Declare(World& W)
	{
		PersonChronicleTypes T;
		T.State = W.Types().Register<PersonChronicleState>("PersonChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void PersonChronicle::Attach()
	{
		EventBus& Bus = Owner->Events();
		Bus.Subscribe(PersonDiedEvent.TypeHash, this);
		Bus.Subscribe(PersonMarriedEvent.TypeHash, this);
		Bus.Subscribe(FamilyFoundedEvent.TypeHash, this);
		Bus.Subscribe(FamilyExtinctEvent.TypeHash, this);
		Bus.Subscribe(PersonLeftEvent.TypeHash, this);
		Bus.Subscribe(PersonArrivedEvent.TypeHash, this);
		Bus.Subscribe(RegionPromotedEvent.TypeHash, this);
		Bus.Subscribe(RegionDemotedEvent.TypeHash, this);
	}

	bool PersonChronicle::Matters(const Event& E, uint32& Region) const
	{
		const World& W = *Owner;
		if (E.Is(FamilyFoundedEvent) || E.Is(FamilyExtinctEvent))
		{
			Region = E.Get<FamilyPayload>().Region;
			return E.Is(FamilyFoundedEvent) ? Rules.RecordFoundings != 0 : Rules.RecordExtinctions != 0;
		}
		if (E.Is(PersonDiedEvent))
		{
			const PersonPayload P = E.Get<PersonPayload>();
			Region = P.Region;
			if (Rules.RecordCausedDeaths != 0 && E.Cause.IsValid())
			{
				return true;
			}
			// The head is still recorded on the family at dispatch: the family
			// system replaces it at the next yearly tick.
			return Rules.RecordHeadDeaths != 0 && IsHead(W, Families, P.Person);
		}
		if (E.Is(PersonMarriedEvent))
		{
			const MarriagePayload P = E.Get<MarriagePayload>();
			Region = P.Region;
			return Rules.RecordHeadMarriages != 0 && (IsHead(W, Families, P.Person) || IsHead(W, Families, P.Spouse));
		}
		if (E.Is(PersonLeftEvent) || E.Is(PersonArrivedEvent))
		{
			Region = E.Get<PersonPayload>().Region;
			return Rules.RecordCrossings != 0;
		}
		if (E.Is(RegionPromotedEvent) || E.Is(RegionDemotedEvent))
		{
			Region = E.Get<LodPayload>().Region;
			return Rules.RecordFocus != 0;
		}
		return false;
	}

	void PersonChronicle::OnEvent(const Event& E)
	{
		World& W = *Owner;
		PersonChronicleState* S = FindState(W, State);
		if (S == nullptr)
		{
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(State.State).Add(H, PersonChronicleState{});
			S = FindState(W, State);
			if (S == nullptr)
			{
				return;
			}
		}
		uint32 Region = 0;
		if (!Matters(E, Region))
		{
			return;
		}
		const uint32 Year = static_cast<uint32>(E.Tick / History::TicksPerYear);
		if (S->Year != Year || S->Region != Region)
		{
			S->Year = Year;
			S->Region = Region;
			S->InYear = 0;
		}
		if (Rules.MaxRecordsPerYear != 0 && S->InYear >= Rules.MaxRecordsPerYear)
		{
			++S->Dropped;
			return;
		}
		++S->InYear;
		++S->Records;
		History::RecordInfo R;
		R.Event = E.Id.Value;
		R.Tick = E.Tick;
		R.Type = E.TypeHash;
		R.Subject = E.Subject.Value;
		R.Era = History::EraAt(W, Types.History, E.Tick);
		R.Region = Region;
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

	PersonIndex BuildPersonIndex(const World& W, const PersonTypes& Persons)
	{
		PersonIndex Index;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index >= Index.Handles.size())
					{
						Index.Handles.resize(usize{P.Index} + 1u);
					}
					Index.Handles[P.Index] = H;
				});
		return Index;
	}

	void NamePerson(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons, uint32 Person,
					std::string& Out, const PersonIndex* Index)
	{
		const EntityHandle H = HandleOfPerson(W, Persons, Person, Index);
		if (!H.IsNull())
		{
			const History::NameInfo* N = History::NameOf(W, Types.Languages, H);
			if (N != nullptr && History::NameLength(N->Text) > 0)
			{
				Out += N->Text.Chars;
				return;
			}
		}
		Append(Out, "person ");
		AppendNumber(Out, Person);
	}

	void NameFamily(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
					const FamilyTypes& Families, uint32 Family, std::string& Out, const PersonIndex* Index)
	{
		const FamilyInfo* F = FamilyByIndex(W, Families, Family);
		if (F == nullptr)
		{
			Append(Out, "house ");
			AppendNumber(Out, Family);
			return;
		}
		Append(Out, "the house of ");
		NamePerson(W, Types, Persons, F->Founder, Out, Index);
	}

	void DescribePersonEvent(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
							 const FamilyTypes& Families, const Event& E, std::string& Out, const PersonIndex* Index)
	{
		if (!(E.Is(PersonBornEvent) || E.Is(PersonDiedEvent) || E.Is(PersonMarriedEvent) || E.Is(FamilyFoundedEvent) ||
			  E.Is(FamilyExtinctEvent) || E.Is(PersonLeftEvent) || E.Is(PersonArrivedEvent) ||
			  E.Is(RegionPromotedEvent) || E.Is(RegionDemotedEvent)))
		{
			History::DescribeEvent(W, Types, E, Out);
			return;
		}
		// The same prefix as the Phase 03 lines: "Year N, age of X: ".
		std::string Prefix;
		History::DescribeEvent(W, Types, E, Prefix); // gives "Year N, age of X: something happened..."
		const usize Colon = Prefix.find(": ");
		Out.clear();
		Out += Colon != std::string::npos ? Prefix.substr(0, Colon + 2) : std::string();
		std::string Region;
		if (E.Is(PersonBornEvent))
		{
			const PersonPayload P = E.Get<PersonPayload>();
			NamePerson(W, Types, Persons, P.Person, Out, Index);
			Append(Out, " was born");
			if (P.Other != 0)
			{
				Append(Out, " to ");
				NamePerson(W, Types, Persons, P.Other, Out, Index);
			}
			Append(Out, " in ");
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, ".");
		}
		else if (E.Is(PersonDiedEvent))
		{
			const PersonPayload P = E.Get<PersonPayload>();
			NamePerson(W, Types, Persons, P.Person, Out, Index);
			Append(Out, " died");
			AppendCause(P.Other, Out);
			Append(Out, " in ");
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, " at ");
			AppendNumber(Out, P.AgeYears);
			Append(Out, ".");
		}
		else if (E.Is(PersonMarriedEvent))
		{
			const MarriagePayload P = E.Get<MarriagePayload>();
			NamePerson(W, Types, Persons, P.Person, Out, Index);
			Append(Out, " married ");
			NamePerson(W, Types, Persons, P.Spouse, Out, Index);
			Append(Out, " in ");
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, ".");
		}
		else if (E.Is(FamilyFoundedEvent))
		{
			const FamilyPayload P = E.Get<FamilyPayload>();
			NamePerson(W, Types, Persons, P.Head, Out, Index);
			Append(Out, " founded a house in ");
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, ".");
		}
		else if (E.Is(FamilyExtinctEvent))
		{
			const FamilyPayload P = E.Get<FamilyPayload>();
			std::string House;
			NameFamily(W, Types, Persons, Families, P.Family, House, Index);
			House[0] = static_cast<char>(House[0] >= 'a' && House[0] <= 'z' ? House[0] - ('a' - 'A') : House[0]);
			Out += House;
			Append(Out, " died out in ");
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, ".");
		}
		else if (E.Is(PersonLeftEvent) || E.Is(PersonArrivedEvent))
		{
			const PersonPayload P = E.Get<PersonPayload>();
			NamePerson(W, Types, Persons, P.Person, Out, Index);
			Append(Out, E.Is(PersonLeftEvent) ? " left " : " came to ");
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, E.Is(PersonLeftEvent) ? " for " : " from ");
			Region.clear();
			History::NameRegion(W, Types, P.Other, Region);
			Out += Region;
			Append(Out, ".");
		}
		else
		{
			const LodPayload P = E.Get<LodPayload>();
			Append(Out, E.Is(RegionPromotedEvent) ? "the chronicle turned to " : "the chronicle left ");
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			if (E.Is(RegionPromotedEvent))
			{
				Append(Out, " and its ");
				AppendNumber(Out, P.Persons);
				Append(Out, " lives");
			}
			Append(Out, ".");
		}
	}

	uint32 ExportChronicleWithPersons(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
									  const FamilyTypes& Families, std::string& Out, uint32 MaxLines)
	{
		std::vector<History::RecordInfo> Records;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach([&](EntityHandle, const History::RecordInfo& R) { Records.push_back(R); });
		std::sort(Records.begin(), Records.end(), [](const History::RecordInfo& A, const History::RecordInfo& B)
				  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
		uint32 Lines = 0;
		std::string Line;
		const PersonIndex Index = BuildPersonIndex(W, Persons);
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
				DescribePersonEvent(W, Types, Persons, Families, *E, Line, &Index);
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

	void PersonTimeline(const World& W, const PersonTypes& Persons, uint32 Person, std::vector<const Event*>& Out)
	{
		Out.clear();
		if (Person == 0)
		{
			return;
		}
		const EntityHandle H = HandleOfPerson(W, Persons, Person, nullptr);
		const PersistentId Id = H.IsNull() ? PersistentId{} : W.Entities().GetId(H);
		for (const Event& E : W.Log().All())
		{
			bool About = Id.IsValid() && E.Subject == Id;
			if (E.Is(PersonBornEvent) || E.Is(PersonDiedEvent) || E.Is(PersonLeftEvent) || E.Is(PersonArrivedEvent))
			{
				About = About || E.Get<PersonPayload>().Person == Person;
			}
			else if (E.Is(PersonMarriedEvent))
			{
				const MarriagePayload P = E.Get<MarriagePayload>();
				About = About || P.Person == Person || P.Spouse == Person;
			}
			else if (E.Is(FamilyFoundedEvent))
			{
				About = About || E.Get<FamilyPayload>().Head == Person;
			}
			if (About)
			{
				Out.push_back(&E);
			}
		}
	}

	const Event* DeathOf(const World& W, const PersonTypes& Persons, uint32 Person)
	{
		(void)Persons;
		for (const Event& E : W.Log().All())
		{
			if (E.Is(PersonDiedEvent) && E.Get<PersonPayload>().Person == Person)
			{
				return &E;
			}
		}
		return nullptr;
	}

	uint32 ExportPersonStory(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
							 const FamilyTypes& Families, uint32 Person, std::string& Out)
	{
		std::vector<const Event*> Timeline;
		PersonTimeline(W, Persons, Person, Timeline);
		uint32 Lines = 0;
		std::string Line;
		const PersonIndex Index = BuildPersonIndex(W, Persons);
		for (const Event* E : Timeline)
		{
			Line.clear();
			DescribePersonEvent(W, Types, Persons, Families, *E, Line, &Index);
			Out += Line;
			Out += '\n';
			++Lines;
		}
		const Event* Death = DeathOf(W, Persons, Person);
		if (Death != nullptr && Death->Cause.IsValid())
		{
			std::vector<History::WhyStep> Steps;
			History::Why(W, Types, Death->Cause, Steps);
			for (const History::WhyStep& S : Steps)
			{
				if (S.Cause == nullptr)
				{
					continue;
				}
				Line.clear();
				Append(Line, "  because ");
				std::string Text;
				History::DescribeEvent(W, Types, *S.Cause, Text);
				const usize Colon = Text.find(": ");
				Line += Colon != std::string::npos ? Text.substr(Colon + 2) : Text;
				Out += Line;
				Out += '\n';
				++Lines;
			}
		}
		return Lines;
	}

	PersonChronicleStats CheckPersonChronicle(const World& W, const History::PreHistoryTypes& Types,
											  const PersonTypes& Persons, const FamilyTypes& Families,
											  const PersonChronicleTypes& State)
	{
		PersonChronicleStats S;
		W.Components()
			.GetPool(State.State)
			.ForEach([&](EntityHandle, const PersonChronicleState& St) { S.Dropped = St.Dropped; });
		const PersonIndex Index = BuildPersonIndex(W, Persons);
		std::string Line;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const History::RecordInfo& R)
				{
					const Hash64 Kinds[8] = {FamilyFoundedEvent.TypeHash,  FamilyExtinctEvent.TypeHash,
											 PersonDiedEvent.TypeHash,	   PersonMarriedEvent.TypeHash,
											 PersonLeftEvent.TypeHash,	   PersonArrivedEvent.TypeHash,
											 RegionPromotedEvent.TypeHash, RegionDemotedEvent.TypeHash};
					uint32 Kind = 8;
					for (uint32 K = 0; K < 8; ++K)
					{
						Kind = R.Type == Kinds[K] ? K : Kind;
					}
					if (Kind == 8)
					{
						return;
					}
					++S.Records;
					++S.ByType[Kind];
					S.WithRegion += R.Region != 0 ? 1u : 0u;
					S.EraConsistent += R.Era == History::EraAt(W, Types.History, R.Tick) ? 1u : 0u;
					const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
					if (E != nullptr)
					{
						Line.clear();
						DescribePersonEvent(W, Types, Persons, Families, *E, Line, &Index);
						S.Described += Line.find("something happened") == std::string::npos ? 1u : 0u;
					}
				});
		return S;
	}
} // namespace Vaelen::Population
