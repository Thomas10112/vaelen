// VAELEN - VaelenSim tests
// World: assembly, build, tick, entity lifecycle with components, determinism.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

using namespace Vaelen;

namespace
{
	struct Position
	{
		int32 X = 0;
		int32 Y = 0;
	};

	class Drift final : public ISystem
	{
	public:
		explicit Drift(ComponentType<Position> InType) : Type(InType) {}
		const char* GetName() const noexcept override { return "Drift"; }
		void Tick(TickContext& Context) override
		{
			ComponentPool<Position>& Pool = Context.Components->GetPool(Type);
			Pool.ForEach(
				[&](EntityHandle, Position& P)
				{
					P.X += static_cast<int32>(Context.Random->Below(3)) - 1;
					P.Y += static_cast<int32>(Context.Random->Below(3)) - 1;
				});
			++Ticks;
		}
		ComponentType<Position> Type;
		uint32 Ticks = 0;
	};
} // namespace

VAELEN_TEST(World, BuildTickAndEntityLifecycle)
{
	WorldConfig Config;
	Config.Seed = 99;
	World W(Config);
	const ComponentType<Position> PosType = W.Types().Register<Position>("Position");
	W.Components().CreatePool(PosType);
	Drift System(PosType);
	VT_CHECK(W.Systems().Add(&System));
	VT_CHECK(W.Build());
	VT_CHECK_EQ(W.Now(), SimTick{0});

	const EntityHandle A = W.CreateEntity(IdKind::Entity);
	const EntityHandle B = W.CreateEntity(IdKind::Entity);
	VT_CHECK(W.Entities().IsAlive(A) && W.Entities().IsAlive(B));
	VT_CHECK(W.Entities().GetId(A).IsKind(IdKind::Entity));
	W.Components().GetPool(PosType).Add(A, Position{});
	W.Components().GetPool(PosType).Add(B, Position{10, 10});

	VT_CHECK_EQ(W.Tick(), 1u);
	VT_CHECK_EQ(W.TickMany(9), uint64{9});
	VT_CHECK_EQ(W.Now(), SimTick{10});
	VT_CHECK_EQ(System.Ticks, 10u);

	VT_CHECK(W.DestroyEntity(A));
	VT_CHECK(!W.DestroyEntity(A));
	VT_CHECK(!W.Entities().IsAlive(A));
	VT_CHECK(!W.Components().GetPool(PosType).Has(A));
	VT_CHECK(W.Components().GetPool(PosType).Has(B));
	VT_CHECK_EQ(W.Entities().GetAliveCount(), 1u);
}

VAELEN_TEST(World, TwoWorldsWithTheSameSeedAndInputsAreIdentical)
{
	auto Run = [](uint64 Seed, uint32 Entities, uint64 Ticks)
	{
		WorldConfig Config;
		Config.Seed = Seed;
		World W(Config);
		const ComponentType<Position> PosType = W.Types().Register<Position>("Position");
		W.Components().CreatePool(PosType);
		Drift System(PosType);
		W.Systems().Add(&System);
		W.Build();
		for (uint32 i = 0; i < Entities; ++i)
		{
			W.Components().GetPool(PosType).Add(W.CreateEntity(IdKind::Entity), Position{});
		}
		W.TickMany(Ticks);
		return ComputeStateDigest(W);
	};
	VT_CHECK_EQ(Run(5, 20, 200), Run(5, 20, 200));
	VT_CHECK_NE(Run(5, 20, 200), Run(6, 20, 200));
	VT_CHECK_NE(Run(5, 20, 200), Run(5, 21, 200));
	VT_CHECK_NE(Run(5, 20, 200), Run(5, 20, 201));
}

VAELEN_TEST(World, TickBeforeBuildIsReportedAndRunsNothing)
{
	VaelenTest::ScopedAssertCapture Capture;
	WorldConfig Config;
	World W(Config);
	VT_CHECK_EQ(W.Tick(), 0u);
	VT_CHECK_EQ(W.Now(), SimTick{0});
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 1);
#endif
}
