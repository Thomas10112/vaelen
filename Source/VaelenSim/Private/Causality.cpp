// VAELEN - VaelenSim
// Phase 17 task 17.04. See Vaelen/Sim/Causality.h for what the six ends mean
// and why there is no seventh.
//
// STATUS: PROTOTYPE (Phase 17 task 17.04) - Tests/Sim/Test_CauseWalk.cpp
#include "Vaelen/Sim/Causality.h"

#include "Vaelen/Sim/History.h"

#include <algorithm>
#include <vector>

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

	CauseCensus TakeCauseCensus(const EventLog& Log)
	{
		CauseCensus Census;
		const std::vector<Event>& All = Log.All();
		Census.Events = All.size();
		if (All.empty())
		{
			return Census;
		}

		// ONE FORWARD PASS FOR THE DEPTHS, and it is correct only because of a
		// guarantee this file already relies on: a cause's id strictly precedes
		// its effect's, and the log is in ascending id order. So by the time an
		// event is reached, its cause's depth is already known - no recursion,
		// no memo table beyond this vector, and no stack to blow on a chain a
		// million events long.
		//
		// The one thing that would break it is a cause that does NOT precede
		// its effect, and that is counted rather than trusted: such an event is
		// given depth 0, because there is no honest answer.
		std::vector<uint32> Depth(All.size(), 0u);

		for (usize Index = 0; Index < All.size(); ++Index)
		{
			const Event& E = All[Index];
			if (!E.Cause.IsValid())
			{
				++Census.RootCauses;
				continue;
			}
			++Census.WithCause;

			// The same order as CauseWalk, and for the same reason: the kind is
			// the high byte of the id, so a kind fault can look like an
			// ordering fault or slip under it entirely.
			if (!E.Cause.IsKind(IdKind::Event))
			{
				++Census.NotAnEvent;
				continue;
			}
			if (E.Cause.Value >= E.Id.Value)
			{
				++Census.NotBeforeEffect;
				continue;
			}

			// Binary search for the cause, over the prefix already passed:
			// searching the whole log would find nothing new and searching the
			// suffix cannot, since the id is smaller.
			usize Lo = 0;
			usize Hi = Index;
			while (Lo < Hi)
			{
				const usize Mid = Lo + (Hi - Lo) / 2;
				if (All[Mid].Id.Value < E.Cause.Value)
				{
					Lo = Mid + 1;
				}
				else
				{
					Hi = Mid;
				}
			}
			if (Lo >= Index || All[Lo].Id != E.Cause)
			{
				++Census.Dangling;
				continue;
			}
			Depth[Index] = Depth[Lo] + 1u;
			if (Depth[Index] > Census.MaxDepth)
			{
				Census.MaxDepth = Depth[Index];
			}
		}

		// The median, by sorting a copy. Over a log that is 98.7% roots this is
		// 0, which is the point of reporting it: a maximum alone cannot tell
		// one deep chain from a graph that is deep throughout.
		std::vector<uint32> Sorted = Depth;
		std::sort(Sorted.begin(), Sorted.end());
		Census.MedianDepth = Sorted[Sorted.size() / 2];

		// THE FAN-OUT, by sorting the cause ids rather than by counting into a
		// map: no allocation per distinct cause, and the answer is stable
		// whatever order the log was built in.
		std::vector<uint64> Causes;
		Causes.reserve(static_cast<usize>(Census.WithCause));
		for (const Event& E : All)
		{
			if (E.Cause.IsValid() && E.Cause.IsKind(IdKind::Event))
			{
				Causes.push_back(E.Cause.Value);
			}
		}
		std::sort(Causes.begin(), Causes.end());
		usize Run = 0;
		for (usize Index = 0; Index < Causes.size(); ++Index)
		{
			Run = (Index > 0 && Causes[Index] == Causes[Index - 1]) ? Run + 1u : 1u;
			if (Run > Census.MaxFanOut)
			{
				Census.MaxFanOut = static_cast<uint32>(Run);
				Census.Busiest = PersistentId(Causes[Index]);
			}
		}
		return Census;
	}
} // namespace Vaelen::History
