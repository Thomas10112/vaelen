// VAELEN - VaelenSim tests
// ComponentStore: pool creation by typed id, RemoveAll across pools in id
// order, counts, misuse.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/ComponentStore.h"
#include "Vaelen/Sim/EntityRegistry.h"

using namespace Vaelen;

namespace
{
	struct Position
	{
		int32 X = 0;
		int32 Y = 0;
	};
	struct Health
	{
		uint16 Value = 100;
	};
	struct Wealth
	{
		int64 Coins = 0;
	};
} // namespace

VAELEN_TEST(ComponentStore, PoolsAreAddressedByTypedId)
{
	ComponentTypeRegistry Types;
	const ComponentType<Position> PositionType = Types.Register<Position>("Position");
	const ComponentType<Health> HealthType = Types.Register<Health>("Health");
	const ComponentType<Wealth> WealthType = Types.Register<Wealth>("Wealth");
	ComponentStore Store(Types);
	VT_CHECK_EQ(Store.PoolCount(), uint32{0});
	ComponentPool<Position>& Positions = Store.CreatePool(PositionType);
	ComponentPool<Wealth>& Wealths = Store.CreatePool(WealthType);
	VT_CHECK_EQ(Store.PoolCount(), uint32{2});
	VT_CHECK(Store.HasPool(PositionType.Id));
	VT_CHECK(!Store.HasPool(HealthType.Id));
	VT_CHECK(Store.HasPool(WealthType.Id));
	VT_CHECK(&Store.GetPool(PositionType) == &Positions);
	VT_CHECK(&Store.GetPool(WealthType) == &Wealths);
	VT_CHECK(Store.GetPoolBase(PositionType.Id) == &Positions);
	VT_CHECK(Store.GetPoolBase(HealthType.Id) == nullptr);
	VT_CHECK(&Store.GetTypes() == &Types);
	const ComponentStore& ConstStore = Store;
	VT_CHECK(&ConstStore.GetPool(WealthType) == &Wealths);
}

VAELEN_TEST(ComponentStore, RemoveAllCleansEveryPoolInIdOrder)
{
	ComponentTypeRegistry Types;
	const ComponentType<Position> PositionType = Types.Register<Position>("Position");
	const ComponentType<Health> HealthType = Types.Register<Health>("Health");
	const ComponentType<Wealth> WealthType = Types.Register<Wealth>("Wealth");
	ComponentStore Store(Types);
	Store.CreatePool(PositionType);
	Store.CreatePool(HealthType);
	Store.CreatePool(WealthType);

	EntityRegistry Registry;
	IdAllocator Ids;
	const EntityHandle A = Registry.Create(Ids, IdKind::Person);
	const EntityHandle B = Registry.Create(Ids, IdKind::Person);
	Store.GetPool(PositionType).Add(A, Position{1, 2});
	Store.GetPool(HealthType).Add(A);
	Store.GetPool(WealthType).Add(A, Wealth{50});
	Store.GetPool(PositionType).Add(B, Position{3, 4});
	VT_CHECK_EQ(Store.CountComponents(A), uint32{3});
	VT_CHECK_EQ(Store.CountComponents(B), uint32{1});

	VT_CHECK_EQ(Store.RemoveAll(A), uint32{3});
	VT_CHECK_EQ(Store.CountComponents(A), uint32{0});
	VT_CHECK_EQ(Store.RemoveAll(A), uint32{0});
	VT_CHECK(Store.GetPool(PositionType).Has(B));
	VT_CHECK_EQ(Store.GetPool(PositionType).Get(B).Y, int32{4});
	VT_CHECK(Registry.Destroy(A));

	// The slot of A is reused by C: no stale component reaches C.
	const EntityHandle C = Registry.Create(Ids, IdKind::Person);
	VT_CHECK_EQ(C.Index(), A.Index());
	VT_CHECK_EQ(Store.CountComponents(C), uint32{0});

	Store.ClearAll();
	VT_CHECK_EQ(Store.CountComponents(B), uint32{0});
	VT_CHECK_EQ(Store.PoolCount(), uint32{3});
}

VAELEN_TEST(ComponentStore, MisuseYieldsScratchPools)
{
	VaelenTest::ScopedAssertCapture Capture;
	ComponentTypeRegistry Types;
	const ComponentType<Position> PositionType = Types.Register<Position>("Position");
	ComponentType<Health> Unregistered;
	Unregistered.Id = 7;
	ComponentStore Store(Types);

	ComponentPool<Health>& Scratch = Store.CreatePool(Unregistered);
	VT_CHECK_EQ(Store.PoolCount(), uint32{0});
	VT_CHECK(Scratch.IsEmpty());

	ComponentPool<Position>& Real = Store.CreatePool(PositionType);
	ComponentPool<Position>& Again = Store.CreatePool(PositionType);
	VT_CHECK(&Real == &Again);
	VT_CHECK_EQ(Store.PoolCount(), uint32{1});

	ComponentType<Health> WrongLayout;
	WrongLayout.Id = PositionType.Id; // Health registered? no: layout mismatch with Position
	ComponentPool<Health>& Mismatch = Store.CreatePool(WrongLayout);
	VT_CHECK(Mismatch.IsEmpty());
	VT_CHECK_EQ(Store.PoolCount(), uint32{1});

	ComponentPool<Health>& NoPool = Store.GetPool(Unregistered);
	VT_CHECK(NoPool.IsEmpty());
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 4);
#else
	VT_CHECK_EQ(Capture.CheckCount, 0);
#endif
}
