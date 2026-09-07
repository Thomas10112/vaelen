// VAELEN - VaelenPolitics
// Phase 07.07: politics in the chronicle.
//
// STATUS: VALIDATED (Phase 07) - integration/text/deterministic tests in Tests/Politics

#include "Vaelen/Politics/PoliticsHistory.h"

#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>

namespace Vaelen::Politics
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

		void Capitalise(std::string& Out, const std::string& Text)
		{
			if (Text.empty())
			{
				return;
			}
			const char First = Text[0];
			Out += static_cast<char>(First >= 'a' && First <= 'z' ? First - ('a' - 'A') : First);
			Out.append(Text, 1, std::string::npos);
		}

		void AppendRegion(const World& W, const History::PreHistoryTypes& Types, uint32 Region, std::string& Out)
		{
			std::string Name;
			History::NameRegion(W, Types, Region, Name);
			Out += Name;
		}

		void AppendPerson(const World& W, const History::PreHistoryTypes& Types, const PoliticsContext& Context,
						  uint32 Person, std::string& Out, const Population::PersonIndex* Index)
		{
			std::string Name;
			Population::NamePerson(W, Types, Context.Persons, Person, Name, Index);
			Out += Name;
		}

		bool IsPoliticsEvent(const Event& E)
		{
			return E.Is(PolityFoundedEvent) || E.Is(PolityDissolvedEvent) || E.Is(RulerSeatedEvent) ||
				   E.Is(RegionClaimedEvent) || E.Is(RegionLostEvent) || E.Is(LawChangedEvent) || E.Is(DuesPaidEvent) ||
				   E.Is(DuesUnpaidEvent) || E.Is(RegionTakenEvent) || E.Is(RegionAnnexedEvent) ||
				   E.Is(RegionSlippedEvent) || E.Is(UpkeepUnpaidEvent) || E.Is(SeatFellVacantEvent) ||
				   E.Is(SuccessionSettledEvent) || E.Is(SuccessionDisputedEvent) || E.Is(FactionFormedEvent) ||
				   E.Is(FactionRevoltedEvent) || E.Is(FactionFadedEvent) || E.Is(ContactMadeEvent) ||
				   E.Is(StanceChangedEvent) || E.Is(RegionContestedEvent);
		}

		PoliticsChronicleState* FindState(World& W, const PoliticsChronicleTypes& Types)
		{
			PoliticsChronicleState* Found = nullptr;
			W.Components()
				.GetPool(Types.State)
				.ForEach(
					[&](EntityHandle, PoliticsChronicleState& S)
					{
						if (Found == nullptr)
						{
							Found = &S;
						}
					});
			return Found;
		}
	} // namespace

	PoliticsChronicleTypes PoliticsChronicleTypes::Declare(World& W)
	{
		PoliticsChronicleTypes T;
		T.State = W.Types().Register<PoliticsChronicleState>("PoliticsChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void PoliticsChronicle::Attach()
	{
		EventBus& Bus = Owner->Events();
		Bus.Subscribe(PolityFoundedEvent.TypeHash, this);
		Bus.Subscribe(PolityDissolvedEvent.TypeHash, this);
		Bus.Subscribe(SeatFellVacantEvent.TypeHash, this);
		Bus.Subscribe(SuccessionDisputedEvent.TypeHash, this);
		Bus.Subscribe(LawChangedEvent.TypeHash, this);
		Bus.Subscribe(FactionRevoltedEvent.TypeHash, this);
		Bus.Subscribe(ContactMadeEvent.TypeHash, this);
		Bus.Subscribe(StanceChangedEvent.TypeHash, this);
		Bus.Subscribe(RegionAnnexedEvent.TypeHash, this);
	}

	bool PoliticsChronicle::Matters(const Event& E, uint32& Region) const
	{
		const PolityPayload P = E.Get<PolityPayload>();
		if (E.Is(PolityFoundedEvent) || E.Is(PolityDissolvedEvent))
		{
			Region = P.Region;
			return Rules.RecordFoundings != 0;
		}
		if (E.Is(SeatFellVacantEvent) || E.Is(SuccessionDisputedEvent))
		{
			// A settled succession is not history: the council seated its head,
			// as it does. A seat left empty, and a seat taken from the one the
			// custom named, are.
			Region = P.Region;
			return Rules.RecordSuccessions != 0;
		}
		if (E.Is(LawChangedEvent))
		{
			// A tax moving a notch is not history; a tax at its floor or its
			// ceiling is - the polity has run out of room in one direction.
			Region = 0;
			if (Rules.RecordExtremeLaws == 0)
			{
				return false;
			}
			const PolityInfo* Master = PolityOf(*Owner, Context.Polities, P.Polity);
			Region = Master != nullptr ? Master->Seat : 0u;
			return P.Value == Context.LawBounds.TaxFloor || P.Value == Context.LawBounds.TaxCeiling;
		}
		if (E.Is(FactionRevoltedEvent))
		{
			Region = P.Region;
			return Rules.RecordRevolts != 0;
		}
		if (E.Is(ContactMadeEvent))
		{
			const PolityInfo* Master = PolityOf(*Owner, Context.Polities, P.Polity);
			Region = Master != nullptr ? Master->Seat : 0u;
			return Rules.RecordWars != 0;
		}
		if (E.Is(StanceChangedEvent))
		{
			// Only the two ends of the scale are history: a pact sworn and a war
			// begun. Drifting through rivalry and back is not.
			const PolityInfo* Master = PolityOf(*Owner, Context.Polities, P.Polity);
			Region = Master != nullptr ? Master->Seat : 0u;
			return Rules.RecordWars != 0 &&
				   (P.Value == static_cast<uint32>(Stance::War) || P.Value == static_cast<uint32>(Stance::Pact));
		}
		if (E.Is(RegionAnnexedEvent))
		{
			Region = P.Region;
			return Rules.RecordAnnexations != 0;
		}
		return false;
	}

	void PoliticsChronicle::OnEvent(const Event& E)
	{
		World& W = *Owner;
		PoliticsChronicleState* S = FindState(W, State);
		if (S == nullptr)
		{
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(State.State).Add(H, PoliticsChronicleState{});
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

	void NamePolity(const World& W, const History::PreHistoryTypes& Types, const PoliticsContext& Context,
					uint32 Polity, std::string& Out)
	{
		const PolityInfo* Master = PolityOf(W, Context.Polities, Polity);
		if (Master == nullptr || Master->Seat == 0)
		{
			Out += "polity ";
			AppendNumber(Out, Polity);
			return;
		}
		Out += "the polity of ";
		AppendRegion(W, Types, Master->Seat, Out);
	}

	void NameFaction(const World& W, const History::PreHistoryTypes& Types, const PoliticsContext& Context,
					 uint32 Faction, std::string& Out)
	{
		const FactionInfo* Party = FactionOf(W, Context.Factions, Faction);
		if (Party == nullptr || Party->Region == 0)
		{
			Out += "faction ";
			AppendNumber(Out, Faction);
			return;
		}
		Out += "the faction of ";
		AppendRegion(W, Types, Party->Region, Out);
	}

	void DescribePoliticsEvent(const World& W, const History::PreHistoryTypes& Types, const PoliticsContext& Context,
							   const Event& E, std::string& Out, const Population::PersonIndex* Index)
	{
		if (!IsPoliticsEvent(E))
		{
			if (Context.Economy != nullptr)
			{
				Economy::DescribeEconomyEvent(W, Types, *Context.Economy, E, Out, Index);
			}
			else
			{
				Population::DescribePersonEvent(W, Types, Context.Persons, Population::FamilyTypes{}, E, Out, Index);
			}
			return;
		}
		std::string Prefix;
		History::DescribeEvent(W, Types, E, Prefix);
		const usize Colon = Prefix.find(": ");
		Out.clear();
		Out += Colon != std::string::npos ? Prefix.substr(0, Colon + 2) : std::string();
		const PolityPayload P = E.Get<PolityPayload>();
		std::string Text;
		auto Polity = [&](uint32 Index_)
		{
			Text.clear();
			NamePolity(W, Types, Context, Index_, Text);
			return Text;
		};

		if (E.Is(PolityFoundedEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " was founded in ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else if (E.Is(PolityDissolvedEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " came to an end, holding ");
			AppendNumber(Out, P.Value);
			Append(Out, P.Value == 1 ? " region." : " regions.");
		}
		else if (E.Is(RulerSeatedEvent))
		{
			AppendPerson(W, Types, Context, P.Person, Out, Index);
			Append(Out, " took the seat of ");
			Out += Polity(P.Polity);
			Append(Out, ".");
		}
		else if (E.Is(RegionClaimedEvent))
		{
			Capitalise(Out,
					   [&]
					   {
						   std::string R;
						   History::NameRegion(W, Types, P.Region, R);
						   return R;
					   }());
			Append(Out, " came under ");
			Out += Polity(P.Polity);
			Append(Out, ".");
		}
		else if (E.Is(RegionLostEvent))
		{
			Capitalise(Out,
					   [&]
					   {
						   std::string R;
						   History::NameRegion(W, Types, P.Region, R);
						   return R;
					   }());
			Append(Out, " was left by ");
			Out += Polity(P.Polity);
			Append(Out, ".");
		}
		else if (E.Is(LawChangedEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			if (P.Value == 0)
			{
				Append(Out, " demanded nothing more.");
			}
			else
			{
				Append(Out, " demanded ");
				AppendNumber(Out, P.Value);
				Append(Out, " in every thousand of the harvest.");
			}
		}
		else if (E.Is(DuesPaidEvent) || E.Is(DuesUnpaidEvent))
		{
			Capitalise(Out,
					   [&]
					   {
						   std::string R;
						   History::NameRegion(W, Types, P.Region, R);
						   return R;
					   }());
			Append(Out, E.Is(DuesPaidEvent) ? " paid " : " could not pay ");
			AppendNumber(Out, P.Value);
			Append(Out, " of grain to ");
			Out += Polity(P.Polity);
			Append(Out, ".");
		}
		else if (E.Is(RegionTakenEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " took ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ", ");
			AppendNumber(Out, P.Value);
			Append(Out, P.Value == 1 ? " hop from its seat." : " hops from its seat.");
		}
		else if (E.Is(RegionAnnexedEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " took ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " from ");
			Out += Polity(P.Person);
			Append(Out, ".");
		}
		else if (E.Is(RegionSlippedEvent))
		{
			Capitalise(Out,
					   [&]
					   {
						   std::string R;
						   History::NameRegion(W, Types, P.Region, R);
						   return R;
					   }());
			Append(Out, " slipped from ");
			Out += Polity(P.Polity);
			Append(Out, ".");
		}
		else if (E.Is(UpkeepUnpaidEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " could not pay ");
			AppendNumber(Out, P.Value);
			Append(Out, " of grain to keep its word abroad.");
		}
		else if (E.Is(SeatFellVacantEvent))
		{
			Append(Out, "The seat of ");
			Out += Polity(P.Polity);
			Append(Out, " fell empty.");
		}
		else if (E.Is(SuccessionSettledEvent))
		{
			AppendPerson(W, Types, Context, P.Person, Out, Index);
			Append(Out, " succeeded to the seat of ");
			Out += Polity(P.Polity);
			Append(Out, ".");
		}
		else if (E.Is(SuccessionDisputedEvent))
		{
			AppendPerson(W, Types, Context, P.Person, Out, Index);
			Append(Out, " took the seat of ");
			Out += Polity(P.Polity);
			Append(Out, ", and not ");
			AppendPerson(W, Types, Context, P.Value, Out, Index);
			Append(Out, ", whom the custom named.");
		}
		else if (E.Is(FactionFormedEvent))
		{
			Append(Out, "A faction rose in ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " against ");
			Out += Polity(P.Polity);
			if (P.Person != 0)
			{
				Append(Out, ", for ");
				AppendPerson(W, Types, Context, P.Person, Out, Index);
			}
			Append(Out, ".");
		}
		else if (E.Is(FactionRevoltedEvent))
		{
			Capitalise(Out,
					   [&]
					   {
						   std::string R;
						   History::NameRegion(W, Types, P.Region, R);
						   return R;
					   }());
			Append(Out, " threw off ");
			Out += Polity(P.Polity);
			Append(Out, ".");
		}
		else if (E.Is(FactionFadedEvent))
		{
			Append(Out, "The faction in ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " came to nothing.");
		}
		else if (E.Is(ContactMadeEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " and ");
			Out += Polity(P.Person);
			Append(Out, " came to know each other.");
		}
		else if (E.Is(StanceChangedEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " and ");
			Out += Polity(P.Person);
			switch (static_cast<Stance>(P.Value))
			{
			case Stance::Pact:
				Append(Out, " swore a pact.");
				break;
			case Stance::Peace:
				Append(Out, " made peace.");
				break;
			case Stance::Rivalry:
				Append(Out, " fell into rivalry.");
				break;
			case Stance::War:
			default:
				Append(Out, " went to war.");
				break;
			}
		}
		else if (E.Is(RegionContestedEvent))
		{
			Capitalise(Out, Polity(P.Polity));
			Append(Out, " laid claim to ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ", held by ");
			Out += Polity(P.Person);
			Append(Out, ".");
		}
	}

	uint32 ExportChronicleWithPolitics(const World& W, const History::PreHistoryTypes& Types,
									   const PoliticsContext& Context, std::string& Out, uint32 MaxLines)
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
				DescribePoliticsEvent(W, Types, Context, *E, Line, &Index);
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

	uint32 ExportWhyWithPolitics(const World& W, const History::PreHistoryTypes& Types, const PoliticsContext& Context,
								 PersistentId Id, std::string& Out)
	{
		std::vector<History::WhyStep> Steps;
		History::Why(W, Types, Id, Steps);
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		uint32 Lines = 0;
		std::string Line;
		for (const History::WhyStep& S : Steps)
		{
			if (S.Cause == nullptr)
			{
				continue;
			}
			Line.clear();
			DescribePoliticsEvent(W, Types, Context, *S.Cause, Line, &Index);
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

	PoliticsChronicleStats CheckPoliticsChronicle(const World& W, const History::PreHistoryTypes& Types,
												  const PoliticsContext& Context, const PoliticsChronicleTypes& State)
	{
		PoliticsChronicleStats S;
		const PoliticsChronicleState* Tally = nullptr;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, const PoliticsChronicleState& St)
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
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		std::string Line;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const History::RecordInfo& R)
				{
					const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
					if (E == nullptr || !IsPoliticsEvent(*E))
					{
						return;
					}
					++S.Records;
					S.WithRegion += R.Region != 0 ? 1u : 0u;
					S.EraConsistent += R.Era == History::EraAt(W, Types.History, R.Tick) ? 1u : 0u;
					Line.clear();
					DescribePoliticsEvent(W, Types, Context, *E, Line, &Index);
					S.Described += Line.empty() ? 0u : 1u;
					if (E->Is(PolityFoundedEvent) || E->Is(PolityDissolvedEvent))
					{
						++S.ByType[0];
					}
					else if (E->Is(SeatFellVacantEvent) || E->Is(SuccessionDisputedEvent))
					{
						++S.ByType[1];
					}
					else if (E->Is(LawChangedEvent))
					{
						++S.ByType[2];
					}
					else if (E->Is(FactionRevoltedEvent))
					{
						++S.ByType[3];
					}
					else if (E->Is(ContactMadeEvent) || E->Is(StanceChangedEvent))
					{
						++S.ByType[4];
					}
					else if (E->Is(RegionAnnexedEvent))
					{
						++S.ByType[5];
					}
				});
		return S;
	}
} // namespace Vaelen::Politics
