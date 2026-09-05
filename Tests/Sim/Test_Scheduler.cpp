// VAELEN - VaelenSim tests
// Scheduler: order independent of registration, dependency handling, cycle
// and unknown-dependency detection, LOD periods, per-system streams that do
// not depend on other systems, determinism across worlds.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <vector>

using namespace Vaelen;

namespace
{
	/// Records its own ticks and the first random draw of every tick.
	class RecordingSystem final : public ISystem
	{
	public:
		RecordingSystem(const char* InName, std::vector<std::string_view> InDeps = {}, SimLod InLod = SimLod::Full)
			: Name(InName), Deps(std::move(InDeps)), Lod(InLod)
		{
		}
		const char* GetName() const noexcept override { return Name; }
		std::vector<std::string_view> GetDependencies() const override { return Deps; }
		SimLod GetLod() const noexcept override { return Lod; }
		void Tick(TickContext& Context) override
		{
			Ticks.push_back(Context.Tick);
			Draws.push_back(Context.Random->NextU64());
			LastContext = Context;
			if (Log != nullptr)
			{
				Log->push_back(Name);
			}
		}
		const char* Name;
		std::vector<std::string_view> Deps;
		SimLod Lod;
		std::vector<SimTick> Ticks;
		std::vector<uint64> Draws;
		TickContext LastContext;
		std::vector<std::string>* Log = nullptr;
	};

	struct Counter
	{
		uint64 Value = 0;
	};

	/// A system that mutates world state from its random stream.
	class GrowthSystem final : public ISystem
	{
	public:
		explicit GrowthSystem(ComponentType<Counter> InType) : Type(InType) {}
		const char* GetName() const noexcept override { return "Growth"; }
		void Tick(TickContext& Context) override
		{
			Context.Components->GetPool(Type).ForEach([&](EntityHandle, Counter& C)
													  { C.Value += Context.Random->Below(10); });
			if (Context.Tick % 7 == 0)
			{
				IdAllocator Ids; // deterministic local ids for this test only
				Ids.ReserveUpTo(IdKind::Entity, Context.Tick * 100);
				const EntityHandle Fresh = Context.Entities->Create(Ids, IdKind::Entity);
				Context.Components->GetPool(Type).Add(Fresh, Counter{Context.Tick});
			}
		}
		ComponentType<Counter> Type;
	};

	std::vector<std::string> Names(const std::vector<std::string_view>& Order)
	{
		return std::vector<std::string>(Order.begin(), Order.end());
	}
} // namespace

VAELEN_TEST(Scheduler, OrderIsIndependentOfRegistrationOrder)
{
	RecordingSystem Alpha("Alpha"), Beta("Beta", {"Alpha"}), Gamma("Gamma", {"Alpha"}),
		Delta("Delta", {"Beta", "Gamma"});
	Scheduler Forward;
	VT_CHECK(Forward.Add(&Alpha) && Forward.Add(&Beta) && Forward.Add(&Gamma) && Forward.Add(&Delta));
	Scheduler Backward;
	VT_CHECK(Backward.Add(&Delta) && Backward.Add(&Gamma) && Backward.Add(&Beta) && Backward.Add(&Alpha));
	VT_REQUIRE(Forward.Build() == Scheduler::BuildResult::Ok);
	VT_REQUIRE(Backward.Build() == Scheduler::BuildResult::Ok);
	const std::vector<std::string> A = Names(Forward.GetOrder());
	const std::vector<std::string> B = Names(Backward.GetOrder());
	VT_CHECK(A == B);
	VT_REQUIRE_EQ(A.size(), usize{4});
	VT_CHECK(A[0] == "Alpha");
	VT_CHECK(A[3] == "Delta");
	// Beta and Gamma are independent: the smaller name hash runs first.
	const bool BetaFirst = HashString("Beta") < HashString("Gamma");
	VT_CHECK(A[1] == (BetaFirst ? "Beta" : "Gamma"));
	VT_CHECK(Forward.IsBuilt() && Backward.IsBuilt());
}

VAELEN_TEST(Scheduler, IndependentSystemsRunInNameHashOrder)
{
	RecordingSystem S1("Weather"), S2("Economy"), S3("Migration"), S4("Agriculture");
	Scheduler Sched;
	Sched.Add(&S1);
	Sched.Add(&S2);
	Sched.Add(&S3);
	Sched.Add(&S4);
	VT_REQUIRE(Sched.Build() == Scheduler::BuildResult::Ok);
	const std::vector<std::string> Order = Names(Sched.GetOrder());
	VT_REQUIRE_EQ(Order.size(), usize{4});
	for (usize i = 1; i < Order.size(); ++i)
	{
		VT_CHECK(HashString(Order[i - 1]) < HashString(Order[i]));
	}
}

VAELEN_TEST(Scheduler, BuildErrorsAreReported)
{
	RecordingSystem A("A", {"Missing"});
	Scheduler Unknown;
	Unknown.Add(&A);
	VT_CHECK(Unknown.Build() == Scheduler::BuildResult::UnknownDependency);
	VT_CHECK(Unknown.GetBuildError() == "A");
	VT_CHECK(!Unknown.IsBuilt());

	RecordingSystem X("X", {"Y"}), Y("Y", {"Z"}), Z("Z", {"X"}), Free("Free");
	Scheduler Cycle;
	Cycle.Add(&Free);
	Cycle.Add(&X);
	Cycle.Add(&Y);
	Cycle.Add(&Z);
	VT_CHECK(Cycle.Build() == Scheduler::BuildResult::Cycle);
	VT_CHECK(!Cycle.GetBuildError().empty());
	VT_CHECK(Cycle.GetOrder().empty());

	RecordingSystem Self("Self", {"Self"});
	Scheduler SelfCycle;
	SelfCycle.Add(&Self);
	VT_CHECK(SelfCycle.Build() == Scheduler::BuildResult::Cycle);
	VT_CHECK(SelfCycle.GetBuildError() == "Self");

	Scheduler BadLod;
	BadLod.Add(&Free);
	LodSchedule Schedule;
	Schedule.Period[0] = 2;
	VT_CHECK(!Schedule.IsValid());
	BadLod.SetLodSchedule(Schedule);
	VT_CHECK(BadLod.Build() == Scheduler::BuildResult::InvalidLodSchedule);

	// Fixing the problem makes Build succeed; Remove invalidates again.
	Z.Deps.clear();
	VT_CHECK(Cycle.Build() == Scheduler::BuildResult::Ok);
	VT_CHECK_EQ(Cycle.Count(), uint32{4});
	VT_CHECK(Cycle.Remove("Free"));
	VT_CHECK(!Cycle.Remove("Free"));
	VT_CHECK(!Cycle.IsBuilt());
	VT_CHECK(!Cycle.Contains("Free") && Cycle.Contains("X"));
}

VAELEN_TEST(Scheduler, LodPeriodsDecideWhoTicks)
{
	RecordingSystem Full("Full"), Detailed("Detailed", {}, SimLod::Detailed),
		Aggregate("Aggregate", {}, SimLod::Aggregate), Statistic("Statistic", {}, SimLod::Statistic),
		World("World", {}, SimLod::World);
	Scheduler Sched;
	for (RecordingSystem* S : {&Full, &Detailed, &Aggregate, &Statistic, &World})
	{
		VT_CHECK(Sched.Add(S));
	}
	VT_REQUIRE(Sched.Build() == Scheduler::BuildResult::Ok);
	VT_CHECK(Sched.GetLodSchedule() == LodSchedule{});

	SimClock Clock;
	const RandomStream WorldStream(42);
	EntityRegistry Entities;
	ComponentTypeRegistry Types;
	ComponentStore Components(Types);
	uint32 TotalRan = 0;
	for (int i = 0; i < 8640 * 2; ++i)
	{
		TotalRan += Sched.RunTick(Clock, WorldStream, Entities, Components);
	}
	VT_CHECK_EQ(Clock.Now(), SimTick{17280});
	VT_CHECK_EQ(Full.Ticks.size(), usize{17280});
	VT_CHECK_EQ(Detailed.Ticks.size(), usize{17280 / 4});
	VT_CHECK_EQ(Aggregate.Ticks.size(), usize{17280 / 24});
	VT_CHECK_EQ(Statistic.Ticks.size(), usize{17280 / 720});
	VT_CHECK_EQ(World.Ticks.size(), usize{2});
	VT_CHECK_EQ(TotalRan, static_cast<uint32>(17280 + 4320 + 720 + 24 + 2));
	VT_CHECK_EQ(World.Ticks[1], SimTick{8640});
	VT_CHECK(Sched.IsDue(SimLod::Aggregate, 48) && !Sched.IsDue(SimLod::Aggregate, 49));
	VT_CHECK(!Sched.IsDue(static_cast<SimLod>(9), 0));
	const std::vector<uint64>& Counts = Sched.GetTickCounts();
	VT_REQUIRE_EQ(Counts.size(), usize{5});
	uint64 Sum = 0;
	for (const uint64 C : Counts)
	{
		Sum += C;
	}
	VT_CHECK_EQ(Sum, uint64{TotalRan});

	// The context carried the world objects and the tick.
	VT_CHECK(Full.LastContext.Clock == &Clock);
	VT_CHECK(Full.LastContext.Entities == &Entities);
	VT_CHECK(Full.LastContext.Components == &Components);
	VT_CHECK(Full.LastContext.Events == nullptr);
	VT_CHECK_EQ(Full.LastContext.Tick, SimTick{17279});
}

VAELEN_TEST(Scheduler, SystemStreamsDependOnlyOnWorldSeedNameAndTick)
{
	// Alpha's draws are the same whether or not Beta exists, and equal to the
	// documented derivation WorldStream.Derive(name).Fork(tick).
	RecordingSystem AlphaAlone("Alpha");
	RecordingSystem AlphaWithBeta("Alpha");
	RecordingSystem Beta("Beta");
	const RandomStream WorldStream(2024);
	auto Run = [&](Scheduler& Sched, int TickCount)
	{
		SimClock Clock;
		EntityRegistry Entities;
		ComponentTypeRegistry Types;
		ComponentStore Components(Types);
		VT_REQUIRE(Sched.Build() == Scheduler::BuildResult::Ok);
		for (int i = 0; i < TickCount; ++i)
		{
			Sched.RunTick(Clock, WorldStream, Entities, Components);
		}
	};
	Scheduler Alone;
	Alone.Add(&AlphaAlone);
	Run(Alone, 50);
	Scheduler Pair;
	Pair.Add(&Beta);
	Pair.Add(&AlphaWithBeta);
	Run(Pair, 50);
	VT_CHECK(AlphaAlone.Draws == AlphaWithBeta.Draws);
	VT_CHECK(AlphaAlone.Draws != Beta.Draws);
	for (usize Tick = 0; Tick < 50; ++Tick)
	{
		RandomStream Expected = WorldStream.Derive("Alpha").Fork(Tick);
		VT_CHECK_EQ(AlphaAlone.Draws[Tick], Expected.NextU64());
	}
	// A different world seed changes every draw.
	RecordingSystem AlphaOther("Alpha");
	Scheduler Other;
	Other.Add(&AlphaOther);
	{
		const RandomStream OtherStream(2025);
		SimClock Clock;
		EntityRegistry Entities;
		ComponentTypeRegistry Types;
		ComponentStore Components(Types);
		VT_REQUIRE(Other.Build() == Scheduler::BuildResult::Ok);
		for (int i = 0; i < 50; ++i)
		{
			Other.RunTick(Clock, OtherStream, Entities, Components);
		}
	}
	VT_CHECK(AlphaOther.Draws != AlphaAlone.Draws);
}

VAELEN_TEST(Scheduler, ExecutionLogFollowsDependencies)
{
	std::vector<std::string> Log;
	RecordingSystem Harvest("Harvest"), Market("Market", {"Harvest"}), Consume("Consume", {"Market"});
	Harvest.Log = &Log;
	Market.Log = &Log;
	Consume.Log = &Log;
	Scheduler Sched;
	Sched.Add(&Consume);
	Sched.Add(&Market);
	Sched.Add(&Harvest);
	VT_REQUIRE(Sched.Build() == Scheduler::BuildResult::Ok);
	SimClock Clock;
	const RandomStream WorldStream(1);
	EntityRegistry Entities;
	ComponentTypeRegistry Types;
	ComponentStore Components(Types);
	Sched.RunTick(Clock, WorldStream, Entities, Components);
	Sched.RunTick(Clock, WorldStream, Entities, Components);
	const std::vector<std::string> Expected{"Harvest", "Market", "Consume", "Harvest", "Market", "Consume"};
	VT_CHECK(Log == Expected);
}

VAELEN_TEST(Scheduler, TwoWorldsWithTheSameSeedEvolveIdentically)
{
	auto RunWorld = [](uint64 Seed, ComponentPool<Counter>::State& OutState, EntityRegistry::State& OutEntities)
	{
		ComponentTypeRegistry Types;
		const ComponentType<Counter> CounterType = Types.Register<Counter>("Counter");
		ComponentStore Components(Types);
		ComponentPool<Counter>& Pool = Components.CreatePool(CounterType);
		EntityRegistry Entities;
		IdAllocator Ids;
		for (int i = 0; i < 20; ++i)
		{
			Pool.Add(Entities.Create(Ids, IdKind::Person));
		}
		GrowthSystem Growth(CounterType);
		Scheduler Sched;
		Sched.Add(&Growth);
		Sched.Build();
		SimClock Clock;
		const RandomStream WorldStream(Seed);
		for (int i = 0; i < 1000; ++i)
		{
			Sched.RunTick(Clock, WorldStream, Entities, Components);
		}
		OutState = Pool.GetState();
		OutEntities = Entities.GetState();
	};
	ComponentPool<Counter>::State A, B, C;
	EntityRegistry::State EA, EB, EC;
	RunWorld(7, A, EA);
	RunWorld(7, B, EB);
	RunWorld(8, C, EC);
	VT_CHECK(A.Entities == B.Entities);
	VT_CHECK(EA == EB);
	VT_REQUIRE_EQ(A.Data.size(), B.Data.size());
	bool Same = true;
	bool DiffersFromC = A.Data.size() != C.Data.size();
	for (usize i = 0; i < A.Data.size(); ++i)
	{
		Same = Same && A.Data[i].Value == B.Data[i].Value;
		DiffersFromC = DiffersFromC || A.Data[i].Value != C.Data[i].Value;
	}
	VT_CHECK(Same);
	VT_CHECK(DiffersFromC);
	VT_CHECK_EQ(A.Entities.size(), usize{20 + 143}); // one entity every 7 ticks over 1000 ticks
}

#if VAELEN_ASSERTS_ENABLED
VAELEN_TEST(Scheduler, MisuseIsACheckFailure)
{
	VaelenTest::ScopedAssertCapture Capture;
	Scheduler Sched;
	VT_CHECK(!Sched.Add(nullptr));
	RecordingSystem A("A");
	VT_CHECK(Sched.Add(&A));
	RecordingSystem AgainA("A");
	VT_CHECK(!Sched.Add(&AgainA));
	RecordingSystem Unnamed("");
	VT_CHECK(!Sched.Add(&Unnamed));
	SimClock Clock;
	const RandomStream WorldStream(1);
	EntityRegistry Entities;
	ComponentTypeRegistry Types;
	ComponentStore Components(Types);
	VT_CHECK_EQ(Sched.RunTick(Clock, WorldStream, Entities, Components), uint32{0}); // not built
	VT_CHECK_EQ(Clock.Now(), SimTick{0});
	VT_CHECK_EQ(Capture.CheckCount, 4);
}
#endif
