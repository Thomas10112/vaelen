// VAELEN - VaelenSociety
// Phase 05.07: society in history.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic tests in Tests/Society

#include "Vaelen/Society/SocietyHistory.h"

#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>

namespace Vaelen::Society
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

		bool IsSocietyEvent(const Event& E)
		{
			return E.Is(OrganizationFoundedEvent) || E.Is(OrganizationDisbandedEvent) || E.Is(MemberJoinedEvent) ||
				   E.Is(MemberLeftEvent) || E.Is(HeadSeatedEvent) || E.Is(DecisionMadeEvent) ||
				   E.Is(RaidPlannedEvent) || E.Is(NormChangedEvent) || E.Is(BondEnteredEvent) || E.Is(BondLeftEvent);
		}

		SocietyChronicleState* FindState(World& W, const SocietyChronicleTypes& Types)
		{
			SocietyChronicleState* Found = nullptr;
			W.Components()
				.GetPool(Types.State)
				.ForEach(
					[&](EntityHandle, SocietyChronicleState& S)
					{
						if (Found == nullptr)
						{
							Found = &S;
						}
					});
			return Found;
		}

		const OrganizationInfo* FindOrganization(const World& W, const OrganizationTypes& Types, uint32 Index)
		{
			const OrganizationInfo* Found = nullptr;
			W.Components()
				.GetPool(Types.Organization)
				.ForEach(
					[&](EntityHandle, const OrganizationInfo& O)
					{
						if (O.Index == Index && Found == nullptr)
						{
							Found = &O;
						}
					});
			return Found;
		}

		void NameReligionIndex(const World& W, const History::PreHistoryTypes& Types, uint32 Religion, std::string& Out)
		{
			EntityHandle Found;
			W.Components()
				.GetPool(Types.Religion.Religion)
				.ForEach(
					[&](EntityHandle H, const History::ReligionInfo& R)
					{
						if (R.Index == Religion && Found.IsNull())
						{
							Found = H;
						}
					});
			if (Found.IsNull())
			{
				Append(Out, "faith ");
				AppendNumber(Out, Religion);
				return;
			}
			std::string Name; // NameEntity writes a whole string, it does not append
			History::NameEntity(W, Types, W.Entities().GetId(Found), Name);
			Out += Name;
		}
	} // namespace

	SocietyChronicleTypes SocietyChronicleTypes::Declare(World& W)
	{
		SocietyChronicleTypes T;
		T.State = W.Types().Register<SocietyChronicleState>("SocietyChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void SocietyChronicle::Attach()
	{
		EventBus& Bus = Owner->Events();
		Bus.Subscribe(OrganizationFoundedEvent.TypeHash, this);
		Bus.Subscribe(OrganizationDisbandedEvent.TypeHash, this);
		Bus.Subscribe(HeadSeatedEvent.TypeHash, this);
		Bus.Subscribe(DecisionMadeEvent.TypeHash, this);
		Bus.Subscribe(RaidPlannedEvent.TypeHash, this);
		Bus.Subscribe(NormChangedEvent.TypeHash, this);
		Bus.Subscribe(BondEnteredEvent.TypeHash, this);
		Bus.Subscribe(BondLeftEvent.TypeHash, this);
	}

	bool SocietyChronicle::Matters(const Event& E, uint32& Region) const
	{
		if (E.Is(OrganizationFoundedEvent) || E.Is(OrganizationDisbandedEvent))
		{
			Region = E.Get<OrganizationPayload>().Region;
			return Rules.RecordFoundings != 0;
		}
		if (E.Is(HeadSeatedEvent))
		{
			Region = E.Get<OrganizationPayload>().Region;
			return Rules.RecordHeads != 0;
		}
		if (E.Is(DecisionMadeEvent))
		{
			const DecisionPayload P = E.Get<DecisionPayload>();
			Region = P.Region;
			if (P.Kind == static_cast<uint32>(DecisionKind::StoreGrain))
			{
				return Rules.RecordGrain != 0;
			}
			if (P.Kind == static_cast<uint32>(DecisionKind::Preach))
			{
				return Rules.RecordSermons != 0;
			}
			return false; // raids are recorded through RaidPlanned, training is daily bread
		}
		if (E.Is(RaidPlannedEvent))
		{
			Region = E.Get<RaidPayload>().Region;
			return Rules.RecordRaids != 0;
		}
		if (E.Is(NormChangedEvent))
		{
			Region = 0; // a culture, not a place
			return Rules.RecordCustoms != 0;
		}
		if (E.Is(BondEnteredEvent))
		{
			const BondPayload P = E.Get<BondPayload>();
			Region = P.Region;
			return Rules.RecordEnslavements != 0 && P.Kind == static_cast<uint32>(BondKind::Enslaved) &&
				   P.Reason != static_cast<uint32>(BondEntry::Promotion);
		}
		if (E.Is(BondLeftEvent))
		{
			const BondPayload P = E.Get<BondPayload>();
			Region = P.Region;
			return Rules.RecordManumissions != 0 && P.Reason == static_cast<uint32>(BondExit::Manumission);
		}
		return false;
	}

	void SocietyChronicle::OnEvent(const Event& E)
	{
		World& W = *Owner;
		SocietyChronicleState* S = FindState(W, State);
		if (S == nullptr)
		{
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(State.State).Add(H, SocietyChronicleState{});
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

	void NameOrganization(const World& W, const History::PreHistoryTypes& Types, const SocietyContext& Context,
						  uint32 Organization, std::string& Out)
	{
		const OrganizationInfo* O = FindOrganization(W, Context.Organizations, Organization);
		if (O == nullptr)
		{
			Append(Out, "organisation ");
			AppendNumber(Out, Organization);
			return;
		}
		std::string Region;
		History::NameRegion(W, Types, O->Region, Region);
		switch (static_cast<OrganizationKind>(O->Kind))
		{
		case OrganizationKind::Temple:
			Append(Out, "the temple of ");
			NameReligionIndex(W, Types, O->Religion, Out);
			Append(Out, " in ");
			Out += Region;
			return;
		case OrganizationKind::Council:
		case OrganizationKind::Guild:
		case OrganizationKind::Warband:
		case OrganizationKind::Clan:
		case OrganizationKind::Count:
		default:
			Append(Out, "the ");
			Append(Out, OrganizationKindName(static_cast<OrganizationKind>(O->Kind)));
			Append(Out, " of ");
			Out += Region;
			return;
		}
	}

	void NameCulture(const World& W, const History::PreHistoryTypes& Types, uint32 Culture, std::string& Out)
	{
		EntityHandle Found;
		W.Components()
			.GetPool(Types.Population.Culture)
			.ForEach(
				[&](EntityHandle H, const History::CultureInfo& C)
				{
					if (C.Index == Culture && Found.IsNull())
					{
						Found = H;
					}
				});
		if (Found.IsNull())
		{
			Append(Out, "culture ");
			AppendNumber(Out, Culture);
			return;
		}
		Append(Out, "the ");
		std::string Name; // NameEntity writes a whole string, it does not append
		History::NameEntity(W, Types, W.Entities().GetId(Found), Name);
		Out += Name;
	}

	void DescribeSocietyEvent(const World& W, const History::PreHistoryTypes& Types, const SocietyContext& Context,
							  const Event& E, std::string& Out, const Population::PersonIndex* Index)
	{
		if (!IsSocietyEvent(E))
		{
			Population::DescribePersonEvent(W, Types, Context.Persons, Context.Families, E, Out, Index);
			return;
		}
		std::string Prefix;
		History::DescribeEvent(W, Types, E, Prefix);
		const usize Colon = Prefix.find(": ");
		Out.clear();
		Out += Colon != std::string::npos ? Prefix.substr(0, Colon + 2) : std::string();
		std::string Text;
		if (E.Is(OrganizationFoundedEvent) || E.Is(OrganizationDisbandedEvent))
		{
			const OrganizationPayload P = E.Get<OrganizationPayload>();
			NameOrganization(W, Types, Context, P.Organization, Text);
			Text[0] = static_cast<char>(Text[0] >= 'a' && Text[0] <= 'z' ? Text[0] - ('a' - 'A') : Text[0]);
			Out += Text;
			Append(Out, E.Is(OrganizationFoundedEvent) ? " was founded." : " was disbanded.");
		}
		else if (E.Is(MemberJoinedEvent) || E.Is(MemberLeftEvent) || E.Is(HeadSeatedEvent))
		{
			const OrganizationPayload P = E.Get<OrganizationPayload>();
			Population::NamePerson(W, Types, Context.Persons, P.Person, Out, Index);
			Append(Out, E.Is(MemberJoinedEvent) ? " joined " : (E.Is(MemberLeftEvent) ? " left " : " came to lead "));
			NameOrganization(W, Types, Context, P.Organization, Out);
			Append(Out, ".");
		}
		else if (E.Is(DecisionMadeEvent))
		{
			const DecisionPayload P = E.Get<DecisionPayload>();
			NameOrganization(W, Types, Context, P.Organization, Text);
			Text[0] = static_cast<char>(Text[0] >= 'a' && Text[0] <= 'z' ? Text[0] - ('a' - 'A') : Text[0]);
			Out += Text;
			Append(Out, " ");
			Append(Out, DecisionKindName(static_cast<DecisionKind>(P.Kind)));
			switch (static_cast<DecisionKind>(P.Kind))
			{
			case DecisionKind::StoreGrain:
				Append(Out, " against the next drought.");
				break;
			case DecisionKind::Preach:
				Append(Out, " and ");
				AppendNumber(Out, P.Value);
				Append(Out, " turned to its faith.");
				break;
			case DecisionKind::Train:
				Append(Out, " ");
				AppendNumber(Out, P.Value);
				Append(Out, " of its members.");
				break;
			case DecisionKind::Raid:
			case DecisionKind::Count:
			default:
				Append(Out, ".");
				break;
			}
		}
		else if (E.Is(RaidPlannedEvent))
		{
			const RaidPayload P = E.Get<RaidPayload>();
			NameOrganization(W, Types, Context, P.Organization, Text);
			Text[0] = static_cast<char>(Text[0] >= 'a' && Text[0] <= 'z' ? Text[0] - ('a' - 'A') : Text[0]);
			Out += Text;
			Append(Out, " planned a raid on ");
			std::string Target;
			History::NameRegion(W, Types, P.Target, Target);
			Out += Target;
			Append(Out, " with a strength of ");
			AppendNumber(Out, P.Strength);
			Append(Out, ".");
		}
		else if (E.Is(NormChangedEvent))
		{
			const NormPayload P = E.Get<NormPayload>();
			NameCulture(W, Types, P.Culture, Text);
			Text[0] = static_cast<char>(Text[0] >= 'a' && Text[0] <= 'z' ? Text[0] - ('a' - 'A') : Text[0]);
			Out += Text;
			Append(Out, " changed their ");
			Append(Out, NormFieldName(static_cast<NormField>(P.Field)));
			Append(Out, " from ");
			AppendNumber(Out, P.Before);
			Append(Out, " to ");
			AppendNumber(Out, P.After);
			Append(Out, ".");
		}
		else if (E.Is(BondEnteredEvent))
		{
			const BondPayload P = E.Get<BondPayload>();
			Population::NamePerson(W, Types, Context.Persons, P.Person, Out, Index);
			Append(Out, P.Kind == static_cast<uint32>(BondKind::Enslaved) ? " was enslaved" : " was bound");
			Append(Out, " by ");
			Append(Out, BondEntryName(static_cast<BondEntry>(P.Reason)));
			Append(Out, " in ");
			std::string Region;
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, ".");
		}
		else
		{
			const BondPayload P = E.Get<BondPayload>();
			Population::NamePerson(W, Types, Context.Persons, P.Person, Out, Index);
			Append(Out, " was freed by ");
			Append(Out, BondExitName(static_cast<BondExit>(P.Reason)));
			Append(Out, " in ");
			std::string Region;
			History::NameRegion(W, Types, P.Region, Region);
			Out += Region;
			Append(Out, ".");
		}
	}

	uint32 ExportChronicleWithSociety(const World& W, const History::PreHistoryTypes& Types,
									  const SocietyContext& Context, std::string& Out, uint32 MaxLines)
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
				DescribeSocietyEvent(W, Types, Context, *E, Line, &Index);
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

	uint32 ExportWhyWithSociety(const World& W, const History::PreHistoryTypes& Types, const SocietyContext& Context,
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
			DescribeSocietyEvent(W, Types, Context, *S.Cause, Line, &Index);
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

	SocietyChronicleStats CheckSocietyChronicle(const World& W, const History::PreHistoryTypes& Types,
												const SocietyContext& Context, const SocietyChronicleTypes& State)
	{
		SocietyChronicleStats S;
		W.Components()
			.GetPool(State.State)
			.ForEach([&](EntityHandle, const SocietyChronicleState& St) { S.Dropped = St.Dropped; });
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		std::string Line;
		const Hash64 Kinds[8] = {OrganizationFoundedEvent.TypeHash, OrganizationDisbandedEvent.TypeHash,
								 HeadSeatedEvent.TypeHash,			DecisionMadeEvent.TypeHash,
								 RaidPlannedEvent.TypeHash,			NormChangedEvent.TypeHash,
								 BondEnteredEvent.TypeHash,			BondLeftEvent.TypeHash};
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const History::RecordInfo& R)
				{
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
						DescribeSocietyEvent(W, Types, Context, *E, Line, &Index);
						S.Described += Line.find("something happened") == std::string::npos ? 1u : 0u;
					}
				});
		return S;
	}
} // namespace Vaelen::Society
