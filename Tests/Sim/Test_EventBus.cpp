// VAELEN - VaelenSim tests
// EventBus: next-tick delivery in publish order, type filtering, listener
// order by name hash, deferral of events published during dispatch, ids,
// causal links, integration with the scheduler.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <vector>

using namespace Vaelen;

namespace
{
	struct Birth
	{
		uint32 Settlement = 0;
	};
	struct Death
	{
		uint32 Settlement = 0;
		uint8 Cause = 0;
		uint8 Reserved[3] = {};
	};
	constexpr EventType<Birth> BirthEvent = MakeEventType<Birth>("Birth");
	constexpr EventType<Death> DeathEvent = MakeEventType<Death>("Death");
	constexpr EventType<NoPayload> DawnEvent = MakeEventType<NoPayload>("Dawn");

	class Recorder final : public IEventListener
	{
	public:
		explicit Recorder(const char* InName, std::vector<std::string>* InLog = nullptr) : Name(InName), Log(InLog) {}
		const char* GetListenerName() const noexcept override { return Name; }
		void OnEvent(const Event& E) override
		{
			Received.push_back(E);
			if (Log != nullptr)
			{
				Log->push_back(std::string(Name) + ":" + std::to_string(E.Id.Serial()));
			}
			if (Bus != nullptr && Republish)
			{
				Bus->Publish(E.Tick, DawnEvent, PersistentId::Invalid(), E.Id);
			}
		}
		const char* Name;
		std::vector<std::string>* Log;
		std::vector<Event> Received;
		EventBus* Bus = nullptr;
		bool Republish = false;
	};

	struct World
	{
		IdAllocator Ids;
		EventLog Log;
		EventBus Bus{Ids, Log};
	};
} // namespace

VAELEN_TEST(EventBus, EventsAreDeliveredNextTickInPublishOrder)
{
	World W;
	Recorder Census("Census");
	VT_CHECK(W.Bus.Subscribe(BirthEvent.TypeHash, &Census));
	VT_CHECK(W.Bus.Subscribe(DeathEvent.TypeHash, &Census));
	VT_CHECK_EQ(W.Bus.SubscriberCount(BirthEvent.TypeHash), uint32{1});

	const PersistentId B1 = W.Bus.Publish(0, BirthEvent, Birth{1});
	const PersistentId D1 = W.Bus.Publish(0, DeathEvent, Death{1, 2});
	const PersistentId B2 = W.Bus.Publish(0, BirthEvent, Birth{2});
	VT_CHECK(B1.IsKind(IdKind::Event) && D1.IsKind(IdKind::Event) && B2.IsKind(IdKind::Event));
	VT_CHECK(B1.Serial() < D1.Serial() && D1.Serial() < B2.Serial());
	VT_CHECK_EQ(W.Bus.PendingCount(), uint64{3});
	VT_CHECK_EQ(W.Log.Count(), uint64{3}); // logged at publish time

	// Same tick: nothing delivered yet.
	VT_CHECK_EQ(W.Bus.Dispatch(0), uint32{0});
	VT_CHECK_EQ(Census.Received.size(), usize{0});
	// Next tick: everything, in publish order.
	VT_CHECK_EQ(W.Bus.Dispatch(1), uint32{3});
	VT_REQUIRE_EQ(Census.Received.size(), usize{3});
	VT_CHECK(Census.Received[0].Id == B1);
	VT_CHECK(Census.Received[1].Id == D1);
	VT_CHECK(Census.Received[2].Id == B2);
	VT_CHECK(Census.Received[1].Is(DeathEvent));
	VT_CHECK_EQ(Census.Received[1].Get<Death>().Cause, uint8{2});
	VT_CHECK_EQ(W.Bus.PendingCount(), uint64{0});
	VT_CHECK_EQ(W.Bus.Dispatch(2), uint32{0});

	// Events from several ticks are delivered together, oldest first.
	W.Bus.Publish(1, BirthEvent, Birth{3});
	W.Bus.Publish(2, BirthEvent, Birth{4});
	W.Bus.Publish(3, BirthEvent, Birth{5});
	VT_CHECK_EQ(W.Bus.Dispatch(3), uint32{2}); // ticks 1 and 2 only
	VT_CHECK_EQ(W.Bus.PendingCount(), uint64{1});
	VT_CHECK_EQ(W.Bus.Dispatch(4), uint32{1});
}

VAELEN_TEST(EventBus, ListenersAreFilteredByTypeAndOrderedByName)
{
	World W;
	std::vector<std::string> Order;
	Recorder Zulu("Zulu", &Order), Alpha("Alpha", &Order), Mike("Mike", &Order), OnlyDeaths("Graves", &Order);
	// Subscribe in an arbitrary order; delivery follows the name hash.
	W.Bus.Subscribe(BirthEvent.TypeHash, &Zulu);
	W.Bus.Subscribe(BirthEvent.TypeHash, &Alpha);
	W.Bus.Subscribe(BirthEvent.TypeHash, &Mike);
	W.Bus.Subscribe(DeathEvent.TypeHash, &OnlyDeaths);
	W.Bus.Publish(0, BirthEvent, Birth{1});
	W.Bus.Publish(0, DeathEvent, Death{1, 1});
	VT_CHECK_EQ(W.Bus.Dispatch(1), uint32{2});

	std::vector<std::string> Expected;
	std::vector<const char*> Listeners{"Zulu", "Alpha", "Mike"};
	for (usize i = 0; i < Listeners.size(); ++i)
	{
		for (usize j = i + 1; j < Listeners.size(); ++j)
		{
			if (HashString(Listeners[j]) < HashString(Listeners[i]))
			{
				const char* Tmp = Listeners[i];
				Listeners[i] = Listeners[j];
				Listeners[j] = Tmp;
			}
		}
	}
	for (const char* L : Listeners)
	{
		Expected.push_back(std::string(L) + ":1");
	}
	Expected.push_back("Graves:2");
	VT_CHECK(Order == Expected);
	VT_CHECK_EQ(OnlyDeaths.Received.size(), usize{1});
	VT_CHECK_EQ(Alpha.Received.size(), usize{1});

	// The other subscription order gives the same delivery order.
	World W2;
	std::vector<std::string> Order2;
	Recorder Zulu2("Zulu", &Order2), Alpha2("Alpha", &Order2), Mike2("Mike", &Order2);
	W2.Bus.Subscribe(BirthEvent.TypeHash, &Mike2);
	W2.Bus.Subscribe(BirthEvent.TypeHash, &Zulu2);
	W2.Bus.Subscribe(BirthEvent.TypeHash, &Alpha2);
	W2.Bus.Publish(0, BirthEvent, Birth{1});
	W2.Bus.Dispatch(1);
	Expected.pop_back();
	VT_CHECK(Order2 == Expected);

	VT_CHECK(W.Bus.Unsubscribe(BirthEvent.TypeHash, &Zulu));
	VT_CHECK(!W.Bus.Unsubscribe(BirthEvent.TypeHash, &Zulu));
	VT_CHECK_EQ(W.Bus.SubscriberCount(BirthEvent.TypeHash), uint32{2});
	W.Bus.Publish(1, BirthEvent, Birth{9});
	W.Bus.Dispatch(2);
	VT_CHECK_EQ(Zulu.Received.size(), usize{1});
	VT_CHECK_EQ(Alpha.Received.size(), usize{2});
}

VAELEN_TEST(EventBus, EventsPublishedDuringDispatchWaitForTheNextTick)
{
	World W;
	Recorder Chain("Chain");
	Chain.Bus = &W.Bus;
	Chain.Republish = true;
	Recorder DawnWatcher("DawnWatcher");
	W.Bus.Subscribe(BirthEvent.TypeHash, &Chain);
	W.Bus.Subscribe(DawnEvent.TypeHash, &DawnWatcher);
	const PersistentId Root = W.Bus.Publish(0, BirthEvent, Birth{1});
	VT_CHECK(W.Bus.Dispatch(1) == 1);
	VT_CHECK(!W.Bus.IsDispatching());
	// The republished Dawn is pending, logged, and causally linked to Root.
	VT_CHECK_EQ(W.Bus.PendingCount(), uint64{1});
	VT_CHECK_EQ(W.Log.Count(), uint64{2});
	VT_CHECK(W.Log.At(1).Cause == Root);
	VT_CHECK(W.Log.At(1).Is(DawnEvent));
	VT_CHECK_EQ(DawnWatcher.Received.size(), usize{0});
	// It carries the tick of the dispatch that produced it (0 here, as the
	// listener republished with E.Tick), so the next dispatch delivers it.
	VT_CHECK_EQ(W.Bus.Dispatch(2), uint32{1});
	VT_REQUIRE_EQ(DawnWatcher.Received.size(), usize{1});
	VT_CHECK(DawnWatcher.Received[0].Cause == Root);
	VT_CHECK_EQ(W.Log.CountCausedBy(Root), uint64{1});
}

VAELEN_TEST(EventBus, NoSubscribersAndEmptyPayloads)
{
	World W;
	VT_CHECK_EQ(W.Bus.SubscriberCount(DawnEvent.TypeHash), uint32{0});
	const PersistentId Id = W.Bus.Publish(5, DawnEvent, PersistentId::Make(IdKind::Settlement, 3));
	VT_CHECK(Id.IsValid());
	VT_CHECK_EQ(W.Bus.Dispatch(6), uint32{1}); // delivered to nobody, still consumed
	VT_CHECK_EQ(W.Bus.PendingCount(), uint64{0});
	VT_CHECK_EQ(W.Log.Count(), uint64{1});
	VT_CHECK(W.Log.At(0).Subject == PersistentId::Make(IdKind::Settlement, 3));
	VT_CHECK_EQ(W.Log.At(0).PayloadSize, uint32{0});
	VT_CHECK_EQ(W.Log.At(0).Tick, SimTick{5});
}

namespace
{
	/// Publishes a Birth every tick; listens to Deaths.
	class Village final : public ISystem, public IEventListener
	{
	public:
		const char* GetName() const noexcept override { return "Village"; }
		const char* GetListenerName() const noexcept override { return "Village"; }
		void Tick(TickContext& Context) override
		{
			Context.Events->Publish(Context.Tick, BirthEvent, Birth{static_cast<uint32>(Context.Tick)});
			TicksSeen.push_back(Context.Tick);
		}
		void OnEvent(const Event& E) override { DeathsSeen.push_back(E.Tick); }
		std::vector<SimTick> TicksSeen;
		std::vector<SimTick> DeathsSeen;
	};

	/// Listens to Births, answers each with a Death caused by it.
	class Reaper final : public ISystem, public IEventListener
	{
	public:
		const char* GetName() const noexcept override { return "Reaper"; }
		const char* GetListenerName() const noexcept override { return "Reaper"; }
		void Tick(TickContext&) override {}
		void OnEvent(const Event& E) override
		{
			Bus->Publish(E.Tick + 1, DeathEvent, Death{E.Get<Birth>().Settlement, 1}, PersistentId::Invalid(), E.Id);
		}
		EventBus* Bus = nullptr;
	};
} // namespace

VAELEN_TEST(EventBus, SchedulerDispatchesBeforeSystemsRun)
{
	IdAllocator Ids;
	EventLog Log;
	EventBus Bus(Ids, Log);
	Village V;
	Reaper R;
	R.Bus = &Bus;
	Bus.Subscribe(BirthEvent.TypeHash, &R);
	Bus.Subscribe(DeathEvent.TypeHash, &V);
	Scheduler Sched;
	Sched.Add(&V);
	Sched.Add(&R);
	VT_REQUIRE(Sched.Build() == Scheduler::BuildResult::Ok);

	SimClock Clock;
	const RandomStream WorldStream(3);
	EntityRegistry Entities;
	ComponentTypeRegistry Types;
	ComponentStore Components(Types);
	for (int i = 0; i < 10; ++i)
	{
		Sched.RunTick(Clock, WorldStream, Entities, Components, &Bus);
	}
	// Births at ticks 0..9 (10). Birth of tick t is delivered at tick t+1 to
	// the Reaper, which publishes a Death stamped t+1, delivered at t+2 to
	// the Village: deaths for births 0..7 arrive within the 10 ticks.
	VT_CHECK_EQ(V.TicksSeen.size(), usize{10});
	VT_CHECK_EQ(V.DeathsSeen.size(), usize{8});
	VT_CHECK_EQ(V.DeathsSeen[0], SimTick{1});
	VT_CHECK_EQ(V.DeathsSeen[7], SimTick{8});
	VT_CHECK_EQ(Log.Count(), uint64{10 + 9}); // 10 births, 9 deaths published (tick 9's birth not yet delivered)
	// Every death is caused by a birth, and the chain is recorded.
	for (uint64 i = 0; i < Log.Count(); ++i)
	{
		const Event& E = Log.At(i);
		if (E.Is(DeathEvent))
		{
			VT_CHECK(E.Cause.IsValid());
			VT_CHECK(Log.At(E.Cause.Serial() - 1).Is(BirthEvent));
		}
	}
	// Two identical worlds produce the same log digest.
	{
		IdAllocator Ids2;
		EventLog Log2;
		EventBus Bus2(Ids2, Log2);
		Village V2;
		Reaper R2;
		R2.Bus = &Bus2;
		Bus2.Subscribe(BirthEvent.TypeHash, &R2);
		Bus2.Subscribe(DeathEvent.TypeHash, &V2);
		Scheduler Sched2;
		Sched2.Add(&R2);
		Sched2.Add(&V2);
		Sched2.Build();
		SimClock Clock2;
		EntityRegistry Entities2;
		ComponentStore Components2(Types);
		for (int i = 0; i < 10; ++i)
		{
			Sched2.RunTick(Clock2, WorldStream, Entities2, Components2, &Bus2);
		}
		VT_CHECK_EQ(Log2.Digest(), Log.Digest());
		VT_CHECK(Log2.All() == Log.All());
	}
}

#if VAELEN_ASSERTS_ENABLED
VAELEN_TEST(EventBus, MisuseIsACheckFailure)
{
	VaelenTest::ScopedAssertCapture Capture;
	World W;
	Recorder R("R");
	VT_CHECK(!W.Bus.Subscribe(BirthEvent.TypeHash, nullptr));
	VT_CHECK(W.Bus.Subscribe(BirthEvent.TypeHash, &R));
	VT_CHECK(!W.Bus.Subscribe(BirthEvent.TypeHash, &R));
	Recorder Unnamed("");
	VT_CHECK(!W.Bus.Subscribe(BirthEvent.TypeHash, &Unnamed));
	VT_CHECK_EQ(Capture.CheckCount, 3);
	VT_CHECK_EQ(W.Bus.SubscriberCount(BirthEvent.TypeHash), uint32{1});
}
#endif
