// VAELEN - VaelenSim
// Append-only event log with running digest; next-tick event delivery.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_EventLog.cpp and Test_EventBus.cpp
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Core/Assert.h"

#include <algorithm>
#include <cstring>

namespace Vaelen
{
	// ---- EventLog ------------------------------------------------------------

	void EventLog::Append(const Event& E)
	{
		Events.push_back(E);
		RunningDigest = HashCombine(RunningDigest, E.Hash());
	}

	void EventLog::Clear() noexcept
	{
		Events.clear();
		RunningDigest = EmptyDigest;
	}

	void EventLog::WriteTo(std::vector<uint8>& Out) const
	{
		const uint64 Count = Events.size();
		const usize Start = Out.size();
		Out.resize(Start + 16 + Count * sizeof(Event));
		std::memcpy(Out.data() + Start, &Count, 8);
		std::memcpy(Out.data() + Start + 8, &RunningDigest, 8);
		if (Count > 0)
		{
			std::memcpy(Out.data() + Start + 16, Events.data(), Count * sizeof(Event));
		}
	}

	bool EventLog::ReadFrom(const uint8* Bytes, usize Size)
	{
		Clear();
		if (Bytes == nullptr || Size < 16)
		{
			return false;
		}
		uint64 Count = 0;
		Hash64 ExpectedDigest = 0;
		std::memcpy(&Count, Bytes, 8);
		std::memcpy(&ExpectedDigest, Bytes + 8, 8);
		if (Size != 16 + Count * sizeof(Event))
		{
			return false;
		}
		std::vector<Event> Loaded(static_cast<usize>(Count));
		if (Count > 0)
		{
			std::memcpy(Loaded.data(), Bytes + 16, Count * sizeof(Event));
		}
		Hash64 Digest = EmptyDigest;
		for (const Event& E : Loaded)
		{
			Digest = HashCombine(Digest, E.Hash());
		}
		if (Digest != ExpectedDigest)
		{
			return false;
		}
		Events = std::move(Loaded);
		RunningDigest = Digest;
		return true;
	}

	uint64 EventLog::CountCausedBy(PersistentId Cause) const noexcept
	{
		uint64 Count = 0;
		for (const Event& E : Events)
		{
			Count += E.Cause == Cause ? 1u : 0u;
		}
		return Count;
	}

	// ---- EventBus ------------------------------------------------------------

	void EventBus::Enqueue(const Event& E)
	{
		Log->Append(E);
		if (Dispatching)
		{
			Deferred.push_back(E);
		}
		else
		{
			Pending.push_back(E);
		}
	}

	bool EventBus::Subscribe(Hash64 TypeHash, IEventListener* Listener)
	{
		VAELEN_CHECKF(Listener != nullptr, "EventBus::Subscribe requires a listener");
		if (Listener == nullptr)
		{
			return false;
		}
		const std::string_view Name(Listener->GetListenerName());
		VAELEN_CHECKF(!Name.empty(), "event listeners must have a name");
		if (Name.empty())
		{
			return false;
		}
		for (const Subscription& S : Subscriptions)
		{
			if (S.TypeHash == TypeHash && S.Listener == Listener)
			{
				VAELEN_CHECKF(false, "listener '%s' is already subscribed to this event type",
							  Listener->GetListenerName());
				return false;
			}
		}
		Subscription New;
		New.TypeHash = TypeHash;
		New.ListenerHash = HashString(Name);
		New.Listener = Listener;
		// Keep sorted by (type, listener hash, name): delivery order is then a
		// function of the set of listeners only.
		const auto Position = std::upper_bound(Subscriptions.begin(), Subscriptions.end(), New,
											   [](const Subscription& A, const Subscription& B)
											   {
												   if (A.TypeHash != B.TypeHash)
												   {
													   return A.TypeHash < B.TypeHash;
												   }
												   if (A.ListenerHash != B.ListenerHash)
												   {
													   return A.ListenerHash < B.ListenerHash;
												   }
												   return std::string_view(A.Listener->GetListenerName()) <
														  std::string_view(B.Listener->GetListenerName());
											   });
		Subscriptions.insert(Position, New);
		return true;
	}

	bool EventBus::Unsubscribe(Hash64 TypeHash, IEventListener* Listener) noexcept
	{
		for (usize i = 0; i < Subscriptions.size(); ++i)
		{
			if (Subscriptions[i].TypeHash == TypeHash && Subscriptions[i].Listener == Listener)
			{
				Subscriptions.erase(Subscriptions.begin() + static_cast<std::ptrdiff_t>(i));
				return true;
			}
		}
		return false;
	}

	uint32 EventBus::SubscriberCount(Hash64 TypeHash) const noexcept
	{
		uint32 Count = 0;
		for (const Subscription& S : Subscriptions)
		{
			Count += S.TypeHash == TypeHash ? 1u : 0u;
		}
		return Count;
	}

	void EventBus::SetPending(const std::vector<Event>& InPending)
	{
		VAELEN_CHECKF(!Dispatching, "EventBus::SetPending while dispatching");
		if (Dispatching)
		{
			return;
		}
		Pending = InPending;
		Deferred.clear();
	}

	uint32 EventBus::Dispatch(SimTick Tick)
	{
		VAELEN_CHECKF(!Dispatching, "EventBus::Dispatch is not re-entrant");
		if (Dispatching)
		{
			return 0;
		}
		Dispatching = true;
		uint32 Delivered = 0;
		usize Consumed = 0;
		for (; Consumed < Pending.size(); ++Consumed)
		{
			const Event& E = Pending[Consumed];
			if (E.Tick >= Tick)
			{
				break; // published at or after this tick: next time
			}
			// Subscriptions are sorted by type: find the range.
			const auto First = std::lower_bound(Subscriptions.begin(), Subscriptions.end(), E.TypeHash,
												[](const Subscription& S, Hash64 T) { return S.TypeHash < T; });
			for (auto It = First; It != Subscriptions.end() && It->TypeHash == E.TypeHash; ++It)
			{
				It->Listener->OnEvent(E);
			}
			++Delivered;
		}
		Pending.erase(Pending.begin(), Pending.begin() + static_cast<std::ptrdiff_t>(Consumed));
		Dispatching = false;
		if (!Deferred.empty())
		{
			Pending.insert(Pending.end(), Deferred.begin(), Deferred.end());
			Deferred.clear();
		}
		return Delivered;
	}
} // namespace Vaelen
