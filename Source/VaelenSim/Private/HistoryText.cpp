// VAELEN - VaelenSim
// Phase 03.07: queryable history and the chronicle as text.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim

#include "Vaelen/Sim/HistoryText.h"

#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>

namespace Vaelen::History
{
	namespace
	{
		void Append(std::string& Out, const char* Text)
		{
			Out += Text;
		}

		void AppendNumber(std::string& Out, uint64 Value)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
			Out += Buffer;
		}

		template <typename T, typename Pred>
		EntityHandle FindEntity(const World& W, ComponentType<T> Type, Pred&& Match)
		{
			EntityHandle Found;
			W.Components().GetPool(Type).ForEach(
				[&](EntityHandle H, const T& C)
				{
					if (Found.IsNull() && Match(C))
					{
						Found = H;
					}
				});
			return Found;
		}

		bool AppendName(const World& W, const PreHistoryTypes& Types, EntityHandle H, std::string& Out)
		{
			const NameInfo* N = H.IsNull() ? nullptr : NameOf(W, Types.Languages, H);
			if (N == nullptr)
			{
				return false;
			}
			Out += N->Text.Chars;
			return true;
		}

		void NameCulture(const World& W, const PreHistoryTypes& Types, uint32 Culture, std::string& Out)
		{
			const EntityHandle H =
				FindEntity(W, Types.Population.Culture, [&](const CultureInfo& C) { return C.Index == Culture; });
			if (!AppendName(W, Types, H, Out))
			{
				Append(Out, "culture ");
				AppendNumber(Out, Culture);
			}
		}

		void NameReligion(const World& W, const PreHistoryTypes& Types, uint32 Religion, std::string& Out)
		{
			const EntityHandle H =
				FindEntity(W, Types.Religion.Religion, [&](const ReligionInfo& R) { return R.Index == Religion; });
			if (!AppendName(W, Types, H, Out))
			{
				Append(Out, "religion ");
				AppendNumber(Out, Religion);
			}
		}

		void NameEra(const World& W, const PreHistoryTypes& Types, uint32 Era, std::string& Out)
		{
			const EntityHandle H = FindEntity(W, Types.History.Era, [&](const EraInfo& E) { return E.Index == Era; });
			if (!AppendName(W, Types, H, Out))
			{
				Append(Out, "era ");
				AppendNumber(Out, Era);
			}
		}

		void NameLanguage(const World& W, const PreHistoryTypes& Types, uint32 Language, std::string& Out)
		{
			const EntityHandle H =
				FindEntity(W, Types.Languages.Language, [&](const LanguageInfo& L) { return L.Index == Language; });
			if (!AppendName(W, Types, H, Out))
			{
				Append(Out, "language ");
				AppendNumber(Out, Language);
			}
		}

		uint32 RegionOfSubject(const World& W, const PreHistoryTypes& Types, PersistentId Subject) noexcept
		{
			if (!Subject.IsValid())
			{
				return 0;
			}
			const EntityHandle H = W.Entities().Find(Subject);
			if (H.IsNull())
			{
				return 0;
			}
			const WorldGen::RegionInfo* R = W.Components().GetPool(Types.World.RegionTypes_.Region).TryGet(H);
			return R != nullptr ? R->Index : 0u;
		}

		void Prefix(const World& W, const PreHistoryTypes& Types, uint64 Tick, std::string& Out)
		{
			Append(Out, "Year ");
			AppendNumber(Out, Tick / TicksPerYear);
			const uint32 Era = EraAt(W, Types.History, Tick);
			if (Era != 0)
			{
				Append(Out, ", age of ");
				NameEra(W, Types, Era, Out);
			}
			Append(Out, ": ");
		}
	} // namespace

	namespace
	{
		void AppendRegion(const World& W, const PreHistoryTypes& Types, uint32 Region, std::string& Out)
		{
			const EntityHandle H = FindEntity(W, Types.World.RegionTypes_.Region,
											  [&](const WorldGen::RegionInfo& R) { return R.Index == Region; });
			if (!AppendName(W, Types, H, Out))
			{
				Append(Out, "region ");
				AppendNumber(Out, Region);
			}
		}

		// Disaster names read as common nouns inside a sentence.
		void AppendDisaster(uint32 Kind, std::string& Out)
		{
			const char* Name = DisasterName(static_cast<DisasterKind>(Kind));
			Out += static_cast<char>(Name[0] >= 'A' && Name[0] <= 'Z' ? Name[0] - 'A' + 'a' : Name[0]);
			Out += Name + 1;
		}
	} // namespace

	void NameRegion(const World& W, const PreHistoryTypes& Types, uint32 Region, std::string& Out)
	{
		Out.clear();
		AppendRegion(W, Types, Region, Out);
	}

	namespace
	{
		void AppendEntity(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::string& Out);
	}

	void NameEntity(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::string& Out)
	{
		Out.clear();
		AppendEntity(W, Types, Id, Out);
	}

	namespace
	{
		void AppendEntity(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::string& Out)
		{
			const EntityHandle H = Id.IsValid() ? W.Entities().Find(Id) : EntityHandle{};
			if (H.IsNull())
			{
				Append(Out, "entity ");
				AppendNumber(Out, Id.Value);
				return;
			}
			if (AppendName(W, Types, H, Out))
			{
				return;
			}
			if (const WorldGen::RegionInfo* R = W.Components().GetPool(Types.World.RegionTypes_.Region).TryGet(H))
			{
				Append(Out, "region ");
				AppendNumber(Out, R->Index);
				return;
			}
			if (const CultureInfo* C = W.Components().GetPool(Types.Population.Culture).TryGet(H))
			{
				Append(Out, "culture ");
				AppendNumber(Out, C->Index);
				return;
			}
			if (const ReligionInfo* R = W.Components().GetPool(Types.Religion.Religion).TryGet(H))
			{
				Append(Out, "religion ");
				AppendNumber(Out, R->Index);
				return;
			}
			if (const EraInfo* E = W.Components().GetPool(Types.History.Era).TryGet(H))
			{
				Append(Out, "era ");
				AppendNumber(Out, E->Index);
				return;
			}
			if (const LanguageInfo* L = W.Components().GetPool(Types.Languages.Language).TryGet(H))
			{
				Append(Out, "language ");
				AppendNumber(Out, L->Index);
				return;
			}
			if (const WorldGen::RiverInfo* R = W.Components().GetPool(Types.World.Types.River).TryGet(H))
			{
				Append(Out, "river ");
				AppendNumber(Out, R->Index);
				return;
			}
			if (const WorldGen::LakeInfo* L = W.Components().GetPool(Types.World.Types.Lake).TryGet(H))
			{
				Append(Out, "lake ");
				AppendNumber(Out, L->Index);
				return;
			}
			Append(Out, "entity ");
			AppendNumber(Out, Id.Value);
		}
	} // namespace

	const Event* OriginOf(const World& W, const PreHistoryTypes& Types, PersistentId Entity)
	{
		if (!Entity.IsValid())
		{
			return nullptr;
		}
		const RecordInfo* Earliest = nullptr;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const RecordInfo& R)
				{
					if (R.Subject == Entity.Value && (Earliest == nullptr || R.Tick < Earliest->Tick ||
													  (R.Tick == Earliest->Tick && R.Event < Earliest->Event)))
					{
						Earliest = &R;
					}
				});
		return Earliest != nullptr ? FindEvent(W.Log(), PersistentId{Earliest->Event}) : nullptr;
	}

	void Why(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::vector<WhyStep>& Out, uint32 MaxDepth)
	{
		Out.clear();
		const Event* Start = FindEvent(W.Log(), Id);
		if (Start == nullptr)
		{
			Start = OriginOf(W, Types, Id);
		}
		if (Start == nullptr)
		{
			return;
		}
		std::vector<const Event*> Chain;
		CauseChain(W.Log(), Start->Id, Chain, MaxDepth);
		Out.reserve(Chain.size());
		for (const Event* E : Chain)
		{
			WhyStep Step;
			Step.Cause = E;
			Step.Era = EraAt(W, Types.History, E->Tick);
			Step.Region = RegionOfSubject(W, Types, E->Subject);
			Out.push_back(Step);
		}
	}

	void RegionTimeline(const World& W, const PreHistoryTypes& Types, uint32 Region, std::vector<RecordInfo>& Out)
	{
		Out.clear();
		if (Region == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const RecordInfo& R)
				{
					if (R.Region == Region)
					{
						Out.push_back(R);
					}
				});
		std::sort(Out.begin(), Out.end(), [](const RecordInfo& A, const RecordInfo& B)
				  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
	}

	void DescribeEvent(const World& W, const PreHistoryTypes& Types, const Event& E, std::string& Out)
	{
		Out.clear();
		Prefix(W, Types, E.Tick, Out);
		if (E.Is(EraOpenedEvent))
		{
			const EraPayload P = E.Get<EraPayload>();
			Append(Out, "the age of ");
			NameEra(W, Types, P.Index, Out);
			Append(Out, P.Trigger == static_cast<uint32>(EraTrigger::Founding) ? " began."
						: P.Trigger == static_cast<uint32>(EraTrigger::Span)   ? " began as the old age ran its course."
																			   : " began, born of what came before.");
		}
		else if (E.Is(EraClosedEvent))
		{
			Append(Out, "the age of ");
			NameEra(W, Types, E.Get<EraPayload>().Index, Out);
			Append(Out, " ended.");
		}
		else if (E.Is(CultureFoundedEvent))
		{
			const CulturePayload P = E.Get<CulturePayload>();
			Append(Out, "the ");
			NameCulture(W, Types, P.Culture, Out);
			Append(Out, " first settled ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else if (E.Is(CultureSplitEvent))
		{
			const CulturePayload P = E.Get<CulturePayload>();
			Append(Out, "far from home, the people of ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " became the ");
			NameCulture(W, Types, P.Culture, Out);
			Append(Out, ", no longer the ");
			NameCulture(W, Types, P.Parent, Out);
			Append(Out, ".");
		}
		else if (E.Is(RegionSettledEvent))
		{
			const RegionPeople P = E.Get<RegionPeople>();
			Append(Out, "the ");
			NameCulture(W, Types, P.Culture, Out);
			Append(Out, " settled ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else if (E.Is(RegionAbandonedEvent))
		{
			AppendRegion(W, Types, E.Get<RegionPeople>().Region, Out);
			Append(Out, " was abandoned.");
		}
		else if (E.Is(MigrationWaveEvent))
		{
			const RegionPeople P = E.Get<RegionPeople>();
			AppendNumber(Out, P.People);
			Append(Out, " of the ");
			NameCulture(W, Types, P.Culture, Out);
			Append(Out, " left ");
			AppendRegion(W, Types, P.Reserved, Out);
			Append(Out, " for ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else if (E.Is(LanguageFoundedEvent))
		{
			const LanguagePayload P = E.Get<LanguagePayload>();
			Append(Out, "the ");
			NameCulture(W, Types, P.Culture, Out);
			Append(Out, " began to speak ");
			NameLanguage(W, Types, P.Language, Out);
			if (P.Parent != 0)
			{
				Append(Out, ", a tongue drifted from ");
				NameLanguage(W, Types, P.Parent, Out);
			}
			Append(Out, ".");
		}
		else if (E.Is(LanguageDriftedEvent))
		{
			NameLanguage(W, Types, E.Get<LanguagePayload>().Language, Out);
			Append(Out, " changed one of its sounds.");
		}
		else if (E.Is(ReligionFoundedEvent))
		{
			const ReligionPayload P = E.Get<ReligionPayload>();
			Append(Out, "in ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " the ");
			NameCulture(W, Types, P.Culture, Out);
			Append(Out, " founded the faith of ");
			NameReligion(W, Types, P.Religion, Out);
			Append(Out, ".");
		}
		else if (E.Is(SchismEvent))
		{
			const ReligionPayload P = E.Get<ReligionPayload>();
			Append(Out, "in ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " the faith of ");
			NameReligion(W, Types, P.Religion, Out);
			Append(Out, " broke away from ");
			NameReligion(W, Types, P.Parent, Out);
			Append(Out, ".");
		}
		else if (E.Is(RegionConvertedEvent))
		{
			const ConversionPayload P = E.Get<ConversionPayload>();
			AppendRegion(W, Types, P.Region, Out);
			if (P.Religion == 0)
			{
				Append(Out, " lost its faith.");
			}
			else
			{
				Append(Out, " turned to ");
				NameReligion(W, Types, P.Religion, Out);
				Append(Out, ".");
			}
		}
		else if (E.Is(OmenEvent))
		{
			const OmenPayload P = E.Get<OmenPayload>();
			Append(Out, "omens of ");
			AppendDisaster(P.Kind, Out);
			Append(Out, " were seen over ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else if (E.Is(DisasterStruckEvent))
		{
			const DisasterPayload P = E.Get<DisasterPayload>();
			Append(Out, P.Severity >= 3 ? "a terrible " : (P.Severity == 2 ? "a great " : "a "));
			AppendDisaster(P.Kind, Out);
			Append(Out, " struck ");
			AppendRegion(W, Types, P.Region, Out);
			if (P.Deaths > 0)
			{
				Append(Out, " and ");
				AppendNumber(Out, P.Deaths);
				Append(Out, " died");
			}
			Append(Out, ".");
		}
		else if (E.Is(NamedEvent))
		{
			AppendEntity(W, Types, E.Subject, Out);
			Append(Out, " received its name.");
		}
		else
		{
			Append(Out, "something happened to ");
			AppendEntity(W, Types, E.Subject, Out);
			Append(Out, " (event type ");
			AppendNumber(Out, E.TypeHash);
			Append(Out, ").");
		}
		// Lower-case the first letter after the prefix only when it is a name?
		// No: names keep their capital; the sentence starts after ": ".
	}

	void DescribeRecord(const World& W, const PreHistoryTypes& Types, const RecordInfo& R, std::string& Out)
	{
		const Event* E = FindEvent(W.Log(), PersistentId{R.Event});
		if (E != nullptr)
		{
			DescribeEvent(W, Types, *E, Out);
			return;
		}
		Out.clear();
		Prefix(W, Types, R.Tick, Out);
		Append(Out, "a record of ");
		AppendEntity(W, Types, PersistentId{R.Subject}, Out);
		Append(Out, " (event ");
		AppendNumber(Out, R.Event);
		Append(Out, " no longer in the log).");
	}

	uint32 ExportChronicle(const World& W, const PreHistoryTypes& Types, std::string& Out, uint32 MaxLines)
	{
		std::vector<RecordInfo> Records;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach([&](EntityHandle, const RecordInfo& R) { Records.push_back(R); });
		std::sort(Records.begin(), Records.end(), [](const RecordInfo& A, const RecordInfo& B)
				  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
		Out.clear();
		uint32 Lines = 0;
		std::string Line;
		for (const RecordInfo& R : Records)
		{
			if (MaxLines != 0 && Lines >= MaxLines)
			{
				break;
			}
			DescribeRecord(W, Types, R, Line);
			Out += Line;
			Out += '\n';
			++Lines;
		}
		return Lines;
	}

	uint32 ExportRegionChronicle(const World& W, const PreHistoryTypes& Types, uint32 Region, std::string& Out)
	{
		std::vector<RecordInfo> Records;
		RegionTimeline(W, Types, Region, Records);
		Out.clear();
		std::string Line;
		for (const RecordInfo& R : Records)
		{
			DescribeRecord(W, Types, R, Line);
			Out += Line;
			Out += '\n';
		}
		return static_cast<uint32>(Records.size());
	}

	uint32 ExportWhy(const World& W, const PreHistoryTypes& Types, PersistentId Id, std::string& Out)
	{
		std::vector<WhyStep> Steps;
		Why(W, Types, Id, Steps);
		Out.clear();
		std::string Line;
		for (usize i = 0; i < Steps.size(); ++i)
		{
			DescribeEvent(W, Types, *Steps[i].Cause, Line);
			Out += i == 0 ? "" : "because ";
			Out += Line;
			Out += '\n';
		}
		return static_cast<uint32>(Steps.size());
	}

	ChronicleStats CheckChronicle(const World& W, const PreHistoryTypes& Types)
	{
		ChronicleStats S;
		std::string Line;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const RecordInfo& R)
				{
					++S.Records;
					const Event* E = FindEvent(W.Log(), PersistentId{R.Event});
					if (E == nullptr)
					{
						return;
					}
					++S.Resolved;
					S.EraConsistent += EraAt(W, Types.History, R.Tick) == R.Era ? 1u : 0u;
					S.WithRegion += R.Region != 0 ? 1u : 0u;
					DescribeEvent(W, Types, *E, Line);
					S.Described += Line.find("something happened") == std::string::npos ? 1u : 0u;
				});
		return S;
	}
} // namespace Vaelen::History
