// VAELEN - VaelenMilitary
// Phase 08.07: war in the chronicle.
//
// STATUS: VALIDATED (Phase 08) - integration/text/deterministic tests in Tests/Military

#include "Vaelen/Military/MilitaryHistory.h"

#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace Vaelen::Military
{
	namespace
	{
		void AppendNumber(std::string& Out, uint64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
			Out += Buffer;
		}

		/// "one man" and "sixty-five men" are the same sentence with one word wrong.
		void AppendMen(std::string& Out, uint64 Count)
		{
			AppendNumber(Out, Count);
			Out += Count == 1 ? " man" : " men";
		}

		void AppendPeople(std::string& Out, uint64 Count)
		{
			AppendNumber(Out, Count);
			Out += Count == 1 ? " person" : " people";
		}

		void AppendRegion(const World& W, const History::PreHistoryTypes& Types, uint32 Region, std::string& Out)
		{
			std::string Name;
			History::NameRegion(W, Types, Region, Name);
			Out += Name;
		}

		/// A polity by its seat where the politics context is at hand, by its
		/// number where it is not. The military layer knows a polity is a thing
		/// with a number; only 07.01 knows it is a thing with a seat.
		void AppendPolity(const World& W, const History::PreHistoryTypes& Types, const MilitaryContext& Context,
						  uint32 Polity, std::string& Out)
		{
			if (Context.Politics != nullptr)
			{
				Politics::NamePolity(W, Types, *Context.Politics, Polity, Out);
				return;
			}
			Out += "polity ";
			AppendNumber(Out, Polity);
		}

		bool IsMilitaryEvent(const Event& E)
		{
			return E.Is(ArmyRaisedEvent) || E.Is(ArmyDisbandedEvent) || E.Is(ArmyStarvedEvent) ||
				   E.Is(LevyReleasedEvent) || E.Is(ArmyMarchedEvent) || E.Is(ArmyForagedEvent) ||
				   E.Is(ArmyArrivedEvent) || E.Is(BattleFoughtEvent) || E.Is(ArmyBrokenEvent) ||
				   E.Is(ArmyRetreatedEvent) || E.Is(SiegeLaidEvent) || E.Is(SiegeLiftedEvent) || E.Is(SeatTakenEvent) ||
				   E.Is(WarBeganEvent) || E.Is(WarEndedEvent) || E.Is(WarDeadEvent) || E.Is(PeopleFledEvent);
		}
	} // namespace

	MilitaryChronicleTypes MilitaryChronicleTypes::Declare(World& W)
	{
		MilitaryChronicleTypes T;
		T.State = W.Types().Register<MilitaryChronicleState>("MilitaryChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void MilitaryChronicle::Attach()
	{
		EventBus& Bus = Owner->Events();
		Bus.Subscribe(ArmyRaisedEvent.TypeHash, this);
		Bus.Subscribe(ArmyStarvedEvent.TypeHash, this);
		Bus.Subscribe(BattleFoughtEvent.TypeHash, this);
		Bus.Subscribe(ArmyBrokenEvent.TypeHash, this);
		Bus.Subscribe(SiegeLaidEvent.TypeHash, this);
		Bus.Subscribe(SeatTakenEvent.TypeHash, this);
		Bus.Subscribe(WarBeganEvent.TypeHash, this);
		Bus.Subscribe(WarEndedEvent.TypeHash, this);
		Bus.Subscribe(WarDeadEvent.TypeHash, this);
		Bus.Subscribe(PeopleFledEvent.TypeHash, this);
	}

	bool MilitaryChronicle::Matters(const Event& E, uint32& Region) const
	{
		const Politics::PolityPayload P = E.Get<Politics::PolityPayload>();
		if (E.Is(ArmyRaisedEvent) || E.Is(ArmyStarvedEvent))
		{
			Region = P.Region;
			return Rules.RecordLevies != 0;
		}
		if (E.Is(BattleFoughtEvent) || E.Is(ArmyBrokenEvent))
		{
			Region = P.Region;
			return Rules.RecordBattles != 0;
		}
		if (E.Is(SiegeLaidEvent) || E.Is(SeatTakenEvent))
		{
			Region = P.Region;
			return Rules.RecordSieges != 0;
		}
		if (E.Is(WarBeganEvent) || E.Is(WarEndedEvent))
		{
			// A war belongs to no one region; it is filed at the seat of the
			// side the record names, so that a seat's history has its wars in it.
			Region = 0;
			if (Context.Politics != nullptr)
			{
				const Politics::PolityInfo* Master = Politics::PolityOf(*Owner, Context.Politics->Polities, P.Polity);
				Region = Master != nullptr ? Master->Seat : 0u;
			}
			return Rules.RecordWars != 0;
		}
		if (E.Is(WarDeadEvent))
		{
			// A region losing two or three men is not history; a region losing
			// eight in a year is the year it is remembered for.
			Region = P.Region;
			return Rules.RecordTolls != 0 && P.Value >= Rules.DeadWorthRecording;
		}
		if (E.Is(PeopleFledEvent))
		{
			Region = P.Region;
			return Rules.RecordTolls != 0 && P.Value >= Rules.FledWorthRecording;
		}
		return false;
	}

	void MilitaryChronicle::OnEvent(const Event& E)
	{
		World& W = *Owner;
		MilitaryChronicleState* S = nullptr;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, MilitaryChronicleState& St)
				{
					if (S == nullptr)
					{
						S = &St;
					}
				});
		if (S == nullptr)
		{
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(State.State).Add(H, MilitaryChronicleState{});
			W.Components()
				.GetPool(State.State)
				.ForEach(
					[&](EntityHandle, MilitaryChronicleState& St)
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
		if (S->InYear >= Rules.MaxRecordsPerYear)
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

	void NameArmy(const World& W, const History::PreHistoryTypes& Types, const MilitaryContext& Context, uint32 Army,
				  std::string& Out)
	{
		const ArmyInfo* Host = ArmyOf(W, Context.Armies, Army);
		if (Host == nullptr || Host->Polity == 0)
		{
			Out += "host ";
			AppendNumber(Out, Army);
			return;
		}
		Out += "the host of ";
		AppendPolity(W, Types, Context, Host->Polity, Out);
	}

	void NameWar(const World& W, const History::PreHistoryTypes& Types, const MilitaryContext& Context, uint32 War,
				 std::string& Out)
	{
		const WarInfo* Fight = WarOf(W, Context.Wars, War);
		if (Fight == nullptr || Fight->A == 0 || Fight->B == 0)
		{
			Out += "war ";
			AppendNumber(Out, War);
			return;
		}
		Out += "the war between ";
		AppendPolity(W, Types, Context, Fight->A, Out);
		Out += " and ";
		AppendPolity(W, Types, Context, Fight->B, Out);
	}

	void DescribeMilitaryEvent(const World& W, const History::PreHistoryTypes& Types, const MilitaryContext& Context,
							   const Event& E, std::string& Out, const Population::PersonIndex* Index)
	{
		if (!IsMilitaryEvent(E))
		{
			if (Context.Politics != nullptr)
			{
				Politics::DescribePoliticsEvent(W, Types, *Context.Politics, E, Out, Index);
			}
			else
			{
				History::DescribeEvent(W, Types, E, Out);
			}
			return;
		}
		// The year and the age, in the same hand as every layer below: a
		// chronicle read out of order is not a chronicle.
		std::string Prefix;
		History::DescribeEvent(W, Types, E, Prefix);
		const usize Colon = Prefix.find(": ");
		Out.clear();
		Out += Colon != std::string::npos ? Prefix.substr(0, Colon + 2) : std::string();
		const Politics::PolityPayload P = E.Get<Politics::PolityPayload>();
		if (E.Is(ArmyRaisedEvent))
		{
			AppendPolity(W, Types, Context, P.Polity, Out);
			Out += " called up ";
			AppendMen(Out, P.Value);
			Out += '.';
			return;
		}
		if (E.Is(ArmyDisbandedEvent))
		{
			NameArmy(W, Types, Context, P.Person, Out);
			Out += " went home from ";
			AppendRegion(W, Types, P.Region, Out);
			Out += '.';
			return;
		}
		if (E.Is(ArmyStarvedEvent))
		{
			NameArmy(W, Types, Context, P.Person, Out);
			Out += " lost ";
			AppendMen(Out, P.Value);
			Out += " in ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " for want of grain.";
			return;
		}
		if (E.Is(LevyReleasedEvent))
		{
			AppendMen(Out, P.Value);
			Out += " of ";
			AppendRegion(W, Types, P.Region, Out);
			Out += P.Person == static_cast<uint32>(LevyEnd::Fallen) ? " did not come back." : " came off the levy.";
			return;
		}
		if (E.Is(ArmyMarchedEvent))
		{
			NameArmy(W, Types, Context, P.Person, Out);
			Out += " marched into ";
			AppendRegion(W, Types, P.Region, Out);
			Out += '.';
			return;
		}
		if (E.Is(ArmyForagedEvent))
		{
			NameArmy(W, Types, Context, P.Person, Out);
			Out += " took ";
			AppendNumber(Out, P.Value);
			Out += " grain out of ";
			AppendRegion(W, Types, P.Region, Out);
			Out += '.';
			return;
		}
		if (E.Is(ArmyArrivedEvent))
		{
			NameArmy(W, Types, Context, P.Person, Out);
			Out += " came up in ";
			AppendRegion(W, Types, P.Region, Out);
			Out += '.';
			return;
		}
		if (E.Is(BattleFoughtEvent))
		{
			AppendPolity(W, Types, Context, P.Polity, Out);
			Out += " won a battle in ";
			AppendRegion(W, Types, P.Region, Out);
			Out += "; the beaten side left ";
			AppendMen(Out, P.Value);
			Out += " on it.";
			return;
		}
		if (E.Is(ArmyBrokenEvent))
		{
			NameArmy(W, Types, Context, P.Person, Out);
			Out += " was broken in ";
			AppendRegion(W, Types, P.Region, Out);
			Out += "; ";
			AppendMen(Out, P.Value);
			Out += P.Value == 1 ? " was lost with it." : " were lost with it.";
			return;
		}
		if (E.Is(ArmyRetreatedEvent))
		{
			NameArmy(W, Types, Context, P.Person, Out);
			Out += " fell back on ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " with ";
			AppendMen(Out, P.Value);
			Out += '.';
			return;
		}
		if (E.Is(SiegeLaidEvent))
		{
			AppendPolity(W, Types, Context, P.Polity, Out);
			Out += " sat down before ";
			AppendRegion(W, Types, P.Region, Out);
			Out += '.';
			return;
		}
		if (E.Is(SiegeLiftedEvent))
		{
			AppendPolity(W, Types, Context, P.Polity, Out);
			Out += " gave up the siege of ";
			AppendRegion(W, Types, P.Region, Out);
			Out += '.';
			return;
		}
		if (E.Is(SeatTakenEvent))
		{
			AppendPolity(W, Types, Context, P.Polity, Out);
			Out += " stormed ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " and took it from ";
			AppendPolity(W, Types, Context, P.Value, Out);
			Out += '.';
			return;
		}
		if (E.Is(WarBeganEvent))
		{
			AppendPolity(W, Types, Context, P.Polity, Out);
			Out += " and ";
			AppendPolity(W, Types, Context, P.Value, Out);
			Out += " went to war.";
			return;
		}
		if (E.Is(WarEndedEvent))
		{
			NameWar(W, Types, Context, P.Person, Out);
			Out += " ended after ";
			AppendNumber(Out, P.Value);
			Out += P.Value == 1 ? " year" : " years";
			if (P.Polity == 0)
			{
				Out += " with neither side the winner.";
				return;
			}
			Out += ", won by ";
			AppendPolity(W, Types, Context, P.Polity, Out);
			Out += '.';
			return;
		}
		if (E.Is(WarDeadEvent))
		{
			AppendRegion(W, Types, P.Region, Out);
			Out += " buried ";
			AppendMen(Out, P.Value);
			Out += " of its own.";
			return;
		}
		if (E.Is(PeopleFledEvent))
		{
			AppendPeople(Out, P.Value);
			Out += " left ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " for ";
			AppendRegion(W, Types, P.Person, Out);
			Out += '.';
			return;
		}
	}

	uint32 ExportChronicleWithMilitary(const World& W, const History::PreHistoryTypes& Types,
									   const MilitaryContext& Context, std::string& Out, uint32 MaxLines)
	{
		std::vector<History::RecordInfo> Records;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach([&](EntityHandle, const History::RecordInfo& R) { Records.push_back(R); });
		std::sort(Records.begin(), Records.end(), [](const History::RecordInfo& A, const History::RecordInfo& B)
				  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
		const Population::PersonIndex Index = Context.Politics != nullptr
												  ? Population::BuildPersonIndex(W, Context.Politics->Persons)
												  : Population::PersonIndex{};
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
				DescribeMilitaryEvent(W, Types, Context, *E, Line, &Index);
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

	uint32 ExportWhyWithMilitary(const World& W, const History::PreHistoryTypes& Types, const MilitaryContext& Context,
								 PersistentId Id, std::string& Out)
	{
		std::vector<History::WhyStep> Steps;
		History::Why(W, Types, Id, Steps);
		const Population::PersonIndex Index = Context.Politics != nullptr
												  ? Population::BuildPersonIndex(W, Context.Politics->Persons)
												  : Population::PersonIndex{};
		uint32 Lines = 0;
		std::string Line;
		for (const History::WhyStep& Step : Steps)
		{
			if (Step.Cause == nullptr)
			{
				continue;
			}
			Line.clear();
			DescribeMilitaryEvent(W, Types, Context, *Step.Cause, Line, &Index);
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

	MilitaryChronicleStats CheckMilitaryChronicle(const World& W, const History::PreHistoryTypes& Types,
												  const MilitaryContext& Context, const MilitaryChronicleTypes& State)
	{
		MilitaryChronicleStats S;
		const MilitaryChronicleState* Tally = nullptr;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, const MilitaryChronicleState& St)
				{
					if (Tally == nullptr)
					{
						Tally = &St;
					}
				});
		if (Tally != nullptr)
		{
			S.Dropped = Tally->Dropped;
		}
		const Population::PersonIndex Index = Context.Politics != nullptr
												  ? Population::BuildPersonIndex(W, Context.Politics->Persons)
												  : Population::PersonIndex{};
		std::string Line;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const History::RecordInfo& R)
				{
					const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
					if (E == nullptr || !IsMilitaryEvent(*E))
					{
						return;
					}
					++S.Records;
					S.WithRegion += R.Region != 0 ? 1u : 0u;
					S.EraConsistent += R.Era == History::EraAt(W, Types.History, R.Tick) ? 1u : 0u;
					Line.clear();
					DescribeMilitaryEvent(W, Types, Context, *E, Line, &Index);
					S.Described += Line.empty() ? 0u : 1u;
					if (E->Is(ArmyRaisedEvent) || E->Is(ArmyStarvedEvent))
					{
						++S.ByType[0];
					}
					else if (E->Is(BattleFoughtEvent) || E->Is(ArmyBrokenEvent))
					{
						++S.ByType[1];
					}
					else if (E->Is(SiegeLaidEvent) || E->Is(SeatTakenEvent))
					{
						++S.ByType[2];
					}
					else if (E->Is(WarBeganEvent) || E->Is(WarEndedEvent))
					{
						++S.ByType[3];
					}
					else if (E->Is(WarDeadEvent) || E->Is(PeopleFledEvent))
					{
						++S.ByType[4];
					}
				});
		return S;
	}
} // namespace Vaelen::Military
