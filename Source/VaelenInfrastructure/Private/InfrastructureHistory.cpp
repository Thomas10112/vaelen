// VAELEN - VaelenInfrastructure
// Phase 09.07: infrastructure in the chronicle.
//
// STATUS: PROTOTYPE (Phase 09) - integration/text/deterministic tests in Tests/Infrastructure

#include "Vaelen/Infrastructure/InfrastructureHistory.h"

#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace Vaelen::Infrastructure
{
	namespace
	{
		void AppendNumber(std::string& Out, uint64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
			Out += Buffer;
		}

		void AppendRegion(const World& W, const History::PreHistoryTypes& Types, uint32 Region, std::string& Out)
		{
			std::string Name;
			History::NameRegion(W, Types, Region, Name);
			Out += Name;
		}

		/// "a granary", "a mill of three": the size is worth saying only when
		/// there is more than one of the thing.
		void AppendWork(uint32 Kind, uint32 Size, std::string& Out)
		{
			const char* Name = Kind < WorkCount ? WorkName(static_cast<Work>(Kind)) : "work";
			Out += "a ";
			Out += Name;
			if (Size > 1)
			{
				Out += " of ";
				AppendNumber(Out, Size);
			}
		}

		bool IsWorksEvent(const Event& E)
		{
			return E.Is(BuildingRaisedEvent) || E.Is(BuildingEnlargedEvent) || E.Is(BuildingFellEvent) ||
				   E.Is(RoadCutEvent) || E.Is(RoadLostEvent) || E.Is(PlaceSettledEvent) || E.Is(PlaceGrewEvent) ||
				   E.Is(PlaceEmptiedEvent);
		}
	} // namespace

	WorksChronicleTypes WorksChronicleTypes::Declare(World& W)
	{
		WorksChronicleTypes T;
		T.State = W.Types().Register<WorksChronicleState>("WorksChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void WorksChronicle::Attach()
	{
		EventBus& Bus = Owner->Events();
		Bus.Subscribe(BuildingRaisedEvent.TypeHash, this);
		Bus.Subscribe(BuildingEnlargedEvent.TypeHash, this);
		Bus.Subscribe(BuildingFellEvent.TypeHash, this);
		Bus.Subscribe(RoadCutEvent.TypeHash, this);
		Bus.Subscribe(RoadLostEvent.TypeHash, this);
		Bus.Subscribe(PlaceSettledEvent.TypeHash, this);
		Bus.Subscribe(PlaceGrewEvent.TypeHash, this);
		Bus.Subscribe(PlaceEmptiedEvent.TypeHash, this);
	}

	bool WorksChronicle::Matters(const Event& E, uint32& Region) const
	{
		if (E.Is(BuildingRaisedEvent) || E.Is(BuildingEnlargedEvent) || E.Is(BuildingFellEvent))
		{
			const WorksPayload P = E.Get<WorksPayload>();
			Region = P.Region;
			if (E.Is(BuildingFellEvent))
			{
				return Rules.RecordFalls != 0;
			}
			if (E.Is(BuildingEnlargedEvent))
			{
				// An enlargement from two to three is not history; a work raised
				// where there was none is.
				return Rules.RecordEnlargements != 0;
			}
			return Rules.RecordRaisings != 0;
		}
		if (E.Is(RoadCutEvent) || E.Is(RoadLostEvent))
		{
			// A road belongs to no one region; it is filed at the end it runs
			// from, so that a region's history has its roads in it.
			const RoadPayload P = E.Get<RoadPayload>();
			Region = P.From;
			return Rules.RecordRoads != 0;
		}
		if (E.Is(PlaceSettledEvent) || E.Is(PlaceEmptiedEvent) || E.Is(PlaceGrewEvent))
		{
			const PlacePayload P = E.Get<PlacePayload>();
			Region = P.Region;
			if (E.Is(PlaceGrewEvent))
			{
				// A town growing by one is not history; a town reaching a size
				// worth remembering is.
				return Rules.RecordPlaces != 0 && P.Size >= Rules.SizeWorthRecording;
			}
			return Rules.RecordPlaces != 0;
		}
		return false;
	}

	void WorksChronicle::OnEvent(const Event& E)
	{
		World& W = *Owner;
		WorksChronicleState* S = nullptr;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, WorksChronicleState& St)
				{
					if (S == nullptr)
					{
						S = &St;
					}
				});
		if (S == nullptr)
		{
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(State.State).Add(H, WorksChronicleState{});
			W.Components()
				.GetPool(State.State)
				.ForEach(
					[&](EntityHandle, WorksChronicleState& St)
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

	void NameWork(const World& W, const History::PreHistoryTypes& Types, uint32 Region, Work Kind, std::string& Out)
	{
		const uint32 K = static_cast<uint32>(Kind);
		Out += "the ";
		Out += K < WorkCount ? WorkName(Kind) : "work";
		if (Region != 0)
		{
			Out += " of ";
			AppendRegion(W, Types, Region, Out);
		}
	}

	void NameRoad(const World& W, const History::PreHistoryTypes& Types, const WorksContext& Context, uint32 Route,
				  std::string& Out)
	{
		const RoadInfo* Road = RoadOn(W, Context.Roads, Route);
		if (Road == nullptr || Road->From == 0 || Road->To == 0)
		{
			Out += "road ";
			AppendNumber(Out, Route);
			return;
		}
		Out += "the road between ";
		AppendRegion(W, Types, Road->From, Out);
		Out += " and ";
		AppendRegion(W, Types, Road->To, Out);
	}

	void DescribeWorksEvent(const World& W, const History::PreHistoryTypes& Types, const WorksContext& Context,
							const Event& E, std::string& Out, const Population::PersonIndex* Index)
	{
		if (!IsWorksEvent(E))
		{
			if (Context.Military != nullptr)
			{
				Military::DescribeMilitaryEvent(W, Types, *Context.Military, E, Out, Index);
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

		if (E.Is(BuildingRaisedEvent) || E.Is(BuildingEnlargedEvent) || E.Is(BuildingFellEvent))
		{
			const WorksPayload P = E.Get<WorksPayload>();
			if (E.Is(BuildingFellEvent))
			{
				NameWork(W, Types, P.Region, static_cast<Work>(P.Kind), Out);
				Out += " fell in.";
				return;
			}
			AppendRegion(W, Types, P.Region, Out);
			Out += E.Is(BuildingRaisedEvent) ? " raised " : " made ";
			AppendWork(P.Kind, P.Amount, Out);
			Out += E.Is(BuildingRaisedEvent) ? "." : " of it.";
			return;
		}
		if (E.Is(RoadCutEvent) || E.Is(RoadLostEvent))
		{
			const RoadPayload P = E.Get<RoadPayload>();
			if (E.Is(RoadLostEvent) && P.Grade == 0)
			{
				Out += "the way between ";
				AppendRegion(W, Types, P.From, Out);
				Out += " and ";
				AppendRegion(W, Types, P.To, Out);
				Out += " is a track again.";
				return;
			}
			Out += E.Is(RoadCutEvent) ? "a road was cut between " : "the road fell back between ";
			AppendRegion(W, Types, P.From, Out);
			Out += " and ";
			AppendRegion(W, Types, P.To, Out);
			Out += E.Is(RoadCutEvent) ? "." : ".";
			return;
		}
		if (E.Is(PlaceSettledEvent) || E.Is(PlaceGrewEvent) || E.Is(PlaceEmptiedEvent))
		{
			const PlacePayload P = E.Get<PlacePayload>();
			if (E.Is(PlaceSettledEvent))
			{
				Out += "a town was settled in ";
				AppendRegion(W, Types, P.Region, Out);
				Out += '.';
				return;
			}
			if (E.Is(PlaceEmptiedEvent))
			{
				Out += "the town of ";
				AppendRegion(W, Types, P.Region, Out);
				Out += " emptied.";
				return;
			}
			Out += "the town of ";
			AppendRegion(W, Types, P.Region, Out);
			Out += " grew to ";
			AppendNumber(Out, P.Size);
			Out += '.';
			return;
		}
	}

	uint32 ExportChronicleWithWorks(const World& W, const History::PreHistoryTypes& Types, const WorksContext& Context,
									std::string& Out, uint32 MaxLines)
	{
		std::vector<History::RecordInfo> Records;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach([&](EntityHandle, const History::RecordInfo& R) { Records.push_back(R); });
		std::sort(Records.begin(), Records.end(), [](const History::RecordInfo& A, const History::RecordInfo& B)
				  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
		const Population::PersonIndex Index = Context.Military != nullptr && Context.Military->Politics != nullptr
												  ? Population::BuildPersonIndex(W, Context.Military->Politics->Persons)
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
				DescribeWorksEvent(W, Types, Context, *E, Line, &Index);
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

	uint32 ExportWhyWithWorks(const World& W, const History::PreHistoryTypes& Types, const WorksContext& Context,
							  PersistentId Id, std::string& Out)
	{
		std::vector<History::WhyStep> Steps;
		History::Why(W, Types, Id, Steps);
		const Population::PersonIndex Index = Context.Military != nullptr && Context.Military->Politics != nullptr
												  ? Population::BuildPersonIndex(W, Context.Military->Politics->Persons)
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
			DescribeWorksEvent(W, Types, Context, *Step.Cause, Line, &Index);
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

	WorksChronicleStats CheckWorksChronicle(const World& W, const History::PreHistoryTypes& Types,
											const WorksContext& Context, const WorksChronicleTypes& State)
	{
		WorksChronicleStats S;
		const WorksChronicleState* Tally = nullptr;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, const WorksChronicleState& St)
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
		const Population::PersonIndex Index = Context.Military != nullptr && Context.Military->Politics != nullptr
												  ? Population::BuildPersonIndex(W, Context.Military->Politics->Persons)
												  : Population::PersonIndex{};
		std::string Line;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const History::RecordInfo& R)
				{
					const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
					if (E == nullptr || !IsWorksEvent(*E))
					{
						return;
					}
					++S.Records;
					S.WithRegion += R.Region != 0 ? 1u : 0u;
					S.EraConsistent += R.Era == History::EraAt(W, Types.History, R.Tick) ? 1u : 0u;
					Line.clear();
					DescribeWorksEvent(W, Types, Context, *E, Line, &Index);
					S.Described += Line.empty() ? 0u : 1u;
					if (E->Is(BuildingRaisedEvent) || E->Is(BuildingEnlargedEvent))
					{
						++S.ByType[0];
					}
					else if (E->Is(BuildingFellEvent))
					{
						++S.ByType[1];
					}
					else if (E->Is(RoadCutEvent) || E->Is(RoadLostEvent))
					{
						++S.ByType[2];
					}
					else if (E->Is(PlaceSettledEvent) || E->Is(PlaceGrewEvent) || E->Is(PlaceEmptiedEvent))
					{
						++S.ByType[3];
					}
				});
		return S;
	}
} // namespace Vaelen::Infrastructure
