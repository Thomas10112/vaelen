// VAELEN - VaelenSim
// Phase 17 task 17.04. See Vaelen/Sim/Causality.h for what the six ends mean
// and why there is no seventh.
//
// STATUS: PROTOTYPE (Phase 17 task 17.04) - Tests/Sim/Test_CauseWalk.cpp
#include "Vaelen/Sim/Causality.h"

#include "Vaelen/Sim/History.h"

namespace Vaelen::History
{
	const char* WalkEndToString(WalkEnd End) noexcept
	{
		switch (End)
		{
		case WalkEnd::Root:
			return "Root";
		case WalkEnd::NoSuchEvent:
			return "NoSuchEvent";
		case WalkEnd::CauseMissing:
			return "CauseMissing";
		case WalkEnd::CauseNotAnEvent:
			return "CauseNotAnEvent";
		case WalkEnd::CauseNotBeforeEffect:
			return "CauseNotBeforeEffect";
		case WalkEnd::DepthExhausted:
			return "DepthExhausted";
		}
		return "?";
	}

	WalkEnd CauseWalk(const EventLog& Log, PersistentId Id, std::vector<const Event*>& Out, WalkLimits Limits)
	{
		Out.clear();
		if (Limits.Depth == 0u)
		{
			// Nothing may be collected, so nothing was - and the end says so
			// rather than pretending the log was empty.
			return WalkEnd::DepthExhausted;
		}

		const Event* Current = FindEvent(Log, Id);
		if (Current == nullptr)
		{
			return WalkEnd::NoSuchEvent;
		}

		for (;;)
		{
			Out.push_back(Current);

			if (!Current->Cause.IsValid())
			{
				return WalkEnd::Root;
			}
			// BEFORE the ordering check: see Causality.h. The kind is the high
			// byte of the id, so an Entity cause slips under the ordering guard
			// and a Region cause trips it, and both are this fault.
			if (!Current->Cause.IsKind(IdKind::Event))
			{
				return WalkEnd::CauseNotAnEvent;
			}
			if (Current->Cause.Value >= Current->Id.Value)
			{
				return WalkEnd::CauseNotBeforeEffect;
			}

			const Event* Next = FindEvent(Log, Current->Cause);
			if (Next == nullptr)
			{
				return WalkEnd::CauseMissing;
			}
			// Asked only once there IS somewhere to go, so a chain that reaches
			// a root on its last allowed step is reported as a root.
			if (Out.size() >= Limits.Depth)
			{
				return WalkEnd::DepthExhausted;
			}
			Current = Next;
		}
	}
} // namespace Vaelen::History
