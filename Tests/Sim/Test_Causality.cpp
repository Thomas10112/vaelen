// VAELEN - VaelenSim tests
// Phase 17 task 17.05: what is actually in the causal graph, counted.
//
// THE PANEL MEASURED 7 CAUSES IN 535 EVENTS - 1.31% - with a deepest chain of
// ONE EDGE, and three of its four planning angles had designed a walker, an
// index and a renderer over that. Then this census ran over a fresh AELVOR 128
// at 300+120 years: 16,842,422 events, 30.94% with a cause, deepest chain
// THREE edges. The panel had measured three ten-year worlds. A figure written
// once into a report becomes folklore; a figure a test re-computes stays true
// or stops being green - and this one stopped being true the first time it was
// re-computed on the world that matters.
//
// THE PLANTED LOG IS THE INSTRUMENT'S CALIBRATION. A real world's log cannot
// calibrate this: it has no dangling causes, no cause of the wrong kind and no
// chain deeper than one edge, so a census that reported zero for all of them
// would agree with reality while measuring nothing.
//
// STATUS: PROTOTYPE (Phase 17)
#include "VaelenTest.h"

#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/Causality.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/EventBus.h"

#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;

namespace
{
	constexpr EventType<NoPayload> SomethingHappened = MakeEventType<NoPayload>("EraOpened");

	PersistentId EventId(uint64 Serial)
	{
		return PersistentId::Make(IdKind::Event, Serial);
	}

	void Add(EventLog& Log, uint64 Serial, PersistentId Cause)
	{
		Event E;
		E.Id = EventId(Serial);
		E.Tick = Serial * 10u;
		E.TypeHash = SomethingHappened.TypeHash;
		E.Cause = Cause;
		Log.Append(E);
	}

	/// THE PLANTED GRAPH, and every figure it is meant to produce is in its
	/// shape rather than in a comment beside the assertion.
	///
	///   1 .. 8   a chain of SEVEN EDGES (eight events), which is MaxDepth
	///   20       a root, with FIVE effects hanging off it - 21..25 - the
	///            fan-out
	///   30       a cause of 29, which PRECEDES it and is not in the log: one
	///            DANGLING. The first version wrote 99 here and both the census
	///            and the walk called it NotBeforeEffect - correctly, and in
	///            agreement - because a cause that has not happened yet is that
	///            fault whether or not it is also absent.
	///   31       caused by a PERSON: one NotAnEvent
	///   32       caused by event 40, which had not happened: one
	///            NotBeforeEffect
	///
	/// 17 events in all. Appended in ascending id order, because FindEvent and
	/// the census both binary search and every real log is built that way.
	void Plant(EventLog& Log)
	{
		Add(Log, 1, PersistentId::Invalid());
		for (uint64 Serial = 2; Serial <= 8; ++Serial)
		{
			Add(Log, Serial, EventId(Serial - 1));
		}
		Add(Log, 20, PersistentId::Invalid());
		for (uint64 Serial = 21; Serial <= 25; ++Serial)
		{
			Add(Log, Serial, EventId(20));
		}
		Add(Log, 30, EventId(29));
		Add(Log, 31, PersistentId::Make(IdKind::Person, 4));
		Add(Log, 32, EventId(40));
		Add(Log, 40, PersistentId::Invalid());
	}
} // namespace

VAELEN_TEST(Causality, ThePlantedGraphIsCountedExactly)
{
	EventLog Log;
	Plant(Log);
	const CauseCensus C = TakeCauseCensus(Log);

	VT_CHECK_MSG(C.Events == 18u, "%llu events", static_cast<unsigned long long>(C.Events));
	// 7 in the chain (2..8), 5 on the fan-out, and the three faulty ones.
	VT_CHECK_MSG(C.WithCause == 15u, "%llu with a cause", static_cast<unsigned long long>(C.WithCause));
	VT_CHECK_MSG(C.RootCauses == 3u, "%llu roots", static_cast<unsigned long long>(C.RootCauses));
	VT_CHECK_MSG(C.Dangling == 1u, "%llu dangling", static_cast<unsigned long long>(C.Dangling));
	VT_CHECK_MSG(C.NotAnEvent == 1u, "%llu not an event", static_cast<unsigned long long>(C.NotAnEvent));
	VT_CHECK_MSG(C.NotBeforeEffect == 1u, "%llu not before its effect",
				 static_cast<unsigned long long>(C.NotBeforeEffect));
	VT_CHECK_MSG(C.MaxDepth == 7u, "deepest chain %u edges", C.MaxDepth);
	VT_CHECK_MSG(C.MaxFanOut == 5u, "widest fan-out %u", C.MaxFanOut);
	VT_CHECK_MSG(C.Busiest == EventId(20), "the busiest event is %llu",
				 static_cast<unsigned long long>(C.Busiest.Serial()));

	// The median, over the eighteen: the depths are 0..7 for the chain, 0 for
	// event 20 and 1 for its five effects, and 0 for the four faulty or root
	// ones. Sorted, the ninth is 1.
	VT_CHECK_MSG(C.MedianDepth == 1u, "median depth %u", C.MedianDepth);

	// The parts must add up to the whole, which is the one clause that catches
	// a field counted twice or not at all.
	VT_CHECK_MSG(C.RootCauses + C.WithCause == C.Events, "%llu + %llu is not %llu",
				 static_cast<unsigned long long>(C.RootCauses), static_cast<unsigned long long>(C.WithCause),
				 static_cast<unsigned long long>(C.Events));
}

VAELEN_TEST(Causality, WithTheEdgesRemovedTheDepthGoes)
{
	// THE CONTROL: the same eighteen events with every cause cleared. If the
	// depth or the fan-out survives the edges being removed, they were never
	// reading them.
	EventLog Planted;
	Plant(Planted);

	EventLog Bare;
	for (const Event& E : Planted.All())
	{
		Add(Bare, E.Id.Serial(), PersistentId::Invalid());
	}

	const CauseCensus C = TakeCauseCensus(Bare);
	VT_CHECK_MSG(C.Events == 18u, "the same eighteen events: %llu", static_cast<unsigned long long>(C.Events));
	VT_CHECK_MSG(C.MaxDepth == 0u, "depth %u survived the edges being removed", C.MaxDepth);
	VT_CHECK_MSG(C.MaxFanOut == 0u, "fan-out %u survived the edges being removed", C.MaxFanOut);
	VT_CHECK(C.MedianDepth == 0u);
	VT_CHECK(C.RootCauses == 18u);
	VT_CHECK(C.WithCause == 0u);
	VT_CHECK(C.Dangling == 0u && C.NotAnEvent == 0u && C.NotBeforeEffect == 0u);
	VT_CHECK_MSG(!C.Busiest.IsValid(), "there is no busiest event when nothing causes anything");

	// AND THE PLANTED ONE STILL REPORTS ITS DEPTH, in the same run, so that a
	// census which had stopped working entirely could not pass the clause
	// above.
	VT_CHECK(TakeCauseCensus(Planted).MaxDepth == 7u);
}

VAELEN_TEST(Causality, AnEmptyLogIsAllZerosAndNotAVerdict)
{
	// An empty log reports every field 0 - including `Events`, which is what
	// tells it apart from a log full of events that cause nothing. The census
	// reports what is there; "no problems found" is a reader's conclusion and
	// not a measurement.
	EventLog Empty;
	const CauseCensus C = TakeCauseCensus(Empty);
	VT_CHECK(C.Events == 0u);
	VT_CHECK(C.WithCause == 0u);
	VT_CHECK(C.RootCauses == 0u);
	VT_CHECK(C.Dangling == 0u);
	VT_CHECK(C.NotAnEvent == 0u);
	VT_CHECK(C.NotBeforeEffect == 0u);
	VT_CHECK(C.MaxDepth == 0u);
	VT_CHECK(C.MedianDepth == 0u);
	VT_CHECK(C.MaxFanOut == 0u);
	VT_CHECK(!C.Busiest.IsValid());
}

VAELEN_TEST(Causality, TheCensusAgreesWithTheWalk)
{
	// TWO INSTRUMENTS OVER ONE LOG. The census computes depth in a single
	// forward pass, relying on causes preceding effects; `CauseWalk` climbs
	// backwards one binary search at a time. They share no code, so the one
	// thing that would make both wrong in the same direction is the data being
	// different from what both assume - which is what the planted faults are
	// there to rule out.
	EventLog Log;
	Plant(Log);
	const CauseCensus C = TakeCauseCensus(Log);

	std::vector<const Event*> Chain;
	// The deepest chain, walked from its tip: eight events, seven edges.
	const WalkEnd End = CauseWalk(Log, EventId(8), Chain);
	VT_CHECK_MSG(End == WalkEnd::Root, "%s", WalkEndToString(End));
	VT_CHECK_MSG(Chain.size() == static_cast<usize>(C.MaxDepth) + 1u,
				 "the walk found %zu events, the census says %u edges", Chain.size(), C.MaxDepth);

	// And the three faults the census counted are the three ends the walk
	// names, one each.
	VT_CHECK(CauseWalk(Log, EventId(30), Chain) == WalkEnd::CauseMissing);
	VT_CHECK(CauseWalk(Log, EventId(31), Chain) == WalkEnd::CauseNotAnEvent);
	VT_CHECK(CauseWalk(Log, EventId(32), Chain) == WalkEnd::CauseNotBeforeEffect);
}
