// VAELEN - VaelenSim
// Eras, chronicle and history queries.
//
// STATUS: VALIDATED (Phase 03) - covered by Tests/Sim/Test_History.cpp
#include "Vaelen/Sim/History.h"
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/World.h"

namespace Vaelen::History
{
	HistoryTypes HistoryTypes::Declare(World& W)
	{
		HistoryTypes T;
		T.Era = W.Types().Register<EraInfo>("EraInfo");
		T.Record = W.Types().Register<RecordInfo>("RecordInfo");
		T.State = W.Types().Register<HistoryState>("HistoryState");
		W.Components().CreatePool(T.Era);
		W.Components().CreatePool(T.Record);
		W.Components().CreatePool(T.State);
		return T;
	}

	namespace
	{
		/// The single history state, or nullptr with a report when missing.
		HistoryState* FindState(World& W, const HistoryTypes& Types) noexcept
		{
			HistoryState* Found = nullptr;
			W.Components()
				.GetPool(Types.State)
				.ForEach(
					[&](EntityHandle, HistoryState& S)
					{
						if (Found == nullptr)
						{
							Found = &S;
						}
					});
			VAELEN_CHECKF(Found != nullptr, "history not initialised (call InitializeHistory on a fresh world)");
			return Found;
		}

		EntityHandle FindEra(World& W, const HistoryTypes& Types, uint32 Index) noexcept
		{
			EntityHandle Found;
			W.Components().GetPool(Types.Era).ForEach(
				[&](EntityHandle H, EraInfo& E)
				{
					if (E.Index == Index)
					{
						Found = H;
					}
				});
			return Found;
		}
	} // namespace

	EntityHandle InitializeHistory(World& W, const HistoryTypes& Types)
	{
		const bool Fresh = W.Components().GetPool(Types.State).Size() == 0;
		VAELEN_CHECKF(Fresh, "InitializeHistory called twice");
		if (!Fresh)
		{
			return EntityHandle{};
		}
		const EntityHandle H = W.CreateEntity(IdKind::Document);
		W.Components().GetPool(Types.State).Add(H, HistoryState{});
		return H;
	}

	void EraSystem::RequestEra(PersistentId Cause)
	{
		HistoryState* S = FindState(*Owner, Types);
		if (S == nullptr || S->PendingRequest != 0)
		{
			return;
		}
		S->PendingRequest = 1;
		S->PendingCause = Cause.Value;
	}

	void EraSystem::Tick(TickContext& Context)
	{
		HistoryState* S = FindState(*Owner, Types);
		if (S == nullptr || Context.Events == nullptr)
		{
			return;
		}
		uint32 Trigger = 0;
		uint64 Cause = 0;
		bool Open = false;
		if (S->OpenEra == 0)
		{
			Open = true;
			Trigger = static_cast<uint32>(EraTrigger::Founding);
		}
		else
		{
			const EntityHandle Current = FindEra(*Owner, Types, S->OpenEra);
			EraInfo& E = Owner->Components().GetPool(Types.Era).Get(Current);
			const bool SpanReached = Context.Tick >= E.Start + Rules.SpanTicks;
			if (S->PendingRequest != 0 || SpanReached)
			{
				Open = true;
				Trigger = S->PendingRequest != 0 ? static_cast<uint32>(EraTrigger::Requested)
												 : static_cast<uint32>(EraTrigger::Span);
				Cause = S->PendingRequest != 0 ? S->PendingCause : 0;
				E.End = Context.Tick;
				Context.Events->Publish(Context.Tick, EraClosedEvent, EraPayload{E.Index, Trigger},
										Owner->Entities().GetId(Current), PersistentId(Cause));
			}
		}
		if (!Open)
		{
			return;
		}
		S->PendingRequest = 0;
		S->PendingCause = 0;
		EraInfo Next;
		Next.Index = ++S->EraCount;
		Next.Trigger = Trigger;
		Next.Start = Context.Tick;
		Next.Cause = Cause;
		const EntityHandle H = Owner->CreateEntity(IdKind::Era);
		Owner->Components().GetPool(Types.Era).Add(H, Next);
		S->OpenEra = Next.Index;
		Context.Events->Publish(Context.Tick, EraOpenedEvent, EraPayload{Next.Index, Trigger},
								Owner->Entities().GetId(H), PersistentId(Cause));
	}

	bool Chronicle::Chronicle_(Hash64 TypeHash)
	{
		return Owner->Events().Subscribe(TypeHash, this);
	}

	void Chronicle::OnEvent(const Event& E)
	{
		HistoryState* S = FindState(*Owner, Types);
		if (S == nullptr)
		{
			return;
		}
		RecordInfo R;
		R.Event = E.Id.Value;
		R.Tick = E.Tick;
		R.Type = E.TypeHash;
		R.Subject = E.Subject.Value;
		R.Era = EraAt(*Owner, Types, E.Tick);
		if (E.Subject.IsValid())
		{
			const EntityHandle SubjectHandle = Owner->Entities().Find(E.Subject);
			if (!SubjectHandle.IsNull())
			{
				const WorldGen::RegionInfo* Region = Owner->Components().GetPool(Regions.Region).TryGet(SubjectHandle);
				R.Region = Region != nullptr ? Region->Index : 0;
			}
		}
		++S->RecordCount;
		const EntityHandle H = Owner->CreateEntity(IdKind::Document);
		Owner->Components().GetPool(Types.Record).Add(H, R);
	}

	uint32 EraAt(const World& W, const HistoryTypes& Types, uint64 Tick)
	{
		uint32 Found = 0;
		W.Components().GetPool(Types.Era).ForEach(
			[&](EntityHandle, const EraInfo& E)
			{
				if (Tick >= E.Start && (E.End == 0 || Tick < E.End) && E.Index > Found)
				{
					Found = E.Index;
				}
			});
		return Found;
	}

	const Event* FindEvent(const EventLog& Log, PersistentId Id) noexcept
	{
		// Ids are monotonic and the log is append-only: binary search by id.
		const std::vector<Event>& All = Log.All();
		usize Lo = 0;
		usize Hi = All.size();
		while (Lo < Hi)
		{
			const usize Mid = Lo + (Hi - Lo) / 2;
			if (All[Mid].Id.Value < Id.Value)
			{
				Lo = Mid + 1;
			}
			else
			{
				Hi = Mid;
			}
		}
		return (Lo < All.size() && All[Lo].Id == Id) ? &All[Lo] : nullptr;
	}

	void CauseChain(const EventLog& Log, PersistentId Id, std::vector<const Event*>& Out, uint32 MaxDepth)
	{
		Out.clear();
		const Event* Current = FindEvent(Log, Id);
		while (Current != nullptr && Out.size() < MaxDepth)
		{
			Out.push_back(Current);
			if (!Current->Cause.IsValid() || Current->Cause.Value >= Current->Id.Value)
			{
				break; // root cause, or a link that cannot precede its effect
			}
			Current = FindEvent(Log, Current->Cause);
		}
	}

	void EventsInEra(const World& W, const HistoryTypes& Types, uint32 EraIndex, std::vector<const Event*>& Out)
	{
		Out.clear();
		uint64 Start = 0;
		uint64 End = 0;
		bool Found = false;
		W.Components().GetPool(Types.Era).ForEach(
			[&](EntityHandle, const EraInfo& E)
			{
				if (E.Index == EraIndex)
				{
					Start = E.Start;
					End = E.End;
					Found = true;
				}
			});
		if (!Found)
		{
			return;
		}
		for (const Event& E : W.Log().All())
		{
			if (E.Tick >= Start && (End == 0 || E.Tick < End))
			{
				Out.push_back(&E);
			}
		}
	}

	void EventsAbout(const EventLog& Log, PersistentId Subject, std::vector<const Event*>& Out)
	{
		Out.clear();
		for (const Event& E : Log.All())
		{
			if (E.Subject == Subject)
			{
				Out.push_back(&E);
			}
		}
	}
} // namespace Vaelen::History
