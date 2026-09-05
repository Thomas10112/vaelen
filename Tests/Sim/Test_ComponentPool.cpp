// VAELEN - VaelenSim tests
// ComponentPool<T>: add/get/remove, swap-remove order, stale handles,
// iteration, snapshot state, determinism, assertion paths, soak.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/ComponentPool.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/EntityRegistry.h"

#include <vector>

using namespace Vaelen;

namespace
{
	struct Stock
	{
		int64 Grain = 0;
		int64 Iron = 0;
		bool operator==(const Stock&) const = default;
	};

	ComponentType<Stock> StockType()
	{
		ComponentType<Stock> Type;
		Type.Id = 0;
		return Type;
	}

	EntityHandle H(uint32 Index, uint32 Generation = 1)
	{
		return EntityHandle::Make(Index, Generation);
	}
} // namespace

VAELEN_TEST(ComponentPool, AddGetHasRemove)
{
	ComponentPool<Stock> Pool(StockType());
	VT_CHECK(Pool.IsEmpty());
	VT_CHECK_EQ(Pool.GetTypeId(), ComponentTypeId{0});
	Stock& S = Pool.Add(H(5), Stock{10, 2});
	VT_CHECK_EQ(S.Grain, int64{10});
	VT_CHECK(Pool.Has(H(5)));
	VT_CHECK(!Pool.Has(H(4)));
	VT_CHECK(!Pool.Has(H(500)));
	VT_CHECK(!Pool.Has(EntityHandle::Null()));
	VT_CHECK_EQ(Pool.Size(), uint32{1});
	VT_REQUIRE(Pool.TryGet(H(5)) != nullptr);
	VT_CHECK_EQ(Pool.TryGet(H(5))->Iron, int64{2});
	VT_CHECK(Pool.TryGet(H(6)) == nullptr);
	Pool.Get(H(5)).Grain = 11;
	VT_CHECK_EQ(Pool.Get(H(5)).Grain, int64{11});
	VT_CHECK(Pool.Remove(H(5)));
	VT_CHECK(!Pool.Has(H(5)));
	VT_CHECK(!Pool.Remove(H(5)));
	VT_CHECK_EQ(Pool.Size(), uint32{0});
	Pool.Add(H(5)); // default value
	VT_CHECK_EQ(Pool.Get(H(5)).Grain, int64{0});
}

VAELEN_TEST(ComponentPool, SwapRemoveKeepsDenseArraysConsistent)
{
	ComponentPool<Stock> Pool(StockType());
	for (uint32 i = 0; i < 6; ++i)
	{
		Pool.Add(H(i * 10), Stock{static_cast<int64>(i), 0});
	}
	VT_CHECK(Pool.Remove(H(10))); // dense 1 <- last (entity 50)
	VT_CHECK_EQ(Pool.Size(), uint32{5});
	VT_CHECK(Pool.EntityAt(1) == H(50));
	VT_CHECK_EQ(Pool.DataAt(1).Grain, int64{5});
	VT_CHECK_EQ(Pool.Get(H(50)).Grain, int64{5});
	for (uint32 Dense = 0; Dense < Pool.Size(); ++Dense)
	{
		VT_CHECK_EQ(Pool.Get(Pool.EntityAt(Dense)).Grain, Pool.DataAt(Dense).Grain);
	}
	VT_CHECK(Pool.Remove(H(40))); // remove the last: no swap
	VT_CHECK_EQ(Pool.Size(), uint32{4});
	VT_CHECK(!Pool.Has(H(40)));
	const std::vector<EntityHandle> Expected{H(0), H(50), H(20), H(30)};
	VT_CHECK(Pool.Entities() == Expected);
}

VAELEN_TEST(ComponentPool, StaleGenerationIsNotAMatch)
{
	ComponentPool<Stock> Pool(StockType());
	Pool.Add(H(3, 1), Stock{1, 1});
	VT_CHECK(!Pool.Has(H(3, 2)));
	VT_CHECK(Pool.TryGet(H(3, 2)) == nullptr);
	VT_CHECK(!Pool.Remove(H(3, 2)));
	VT_CHECK(Pool.Has(H(3, 1)));

	// A new generation of the same slot replaces the stale entry (reported).
	VaelenTest::ScopedAssertCapture Capture;
	Pool.Add(H(3, 2), Stock{7, 7});
	VT_CHECK_EQ(Pool.Size(), uint32{1});
	VT_CHECK(!Pool.Has(H(3, 1)));
	VT_CHECK_EQ(Pool.Get(H(3, 2)).Grain, int64{7});
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.EnsureCount, 1);
#endif
}

VAELEN_TEST(ComponentPool, ForEachVisitsDenseOrderAndAllowsMutation)
{
	ComponentPool<Stock> Pool(StockType());
	Pool.Add(H(2), Stock{1, 0});
	Pool.Add(H(7), Stock{2, 0});
	Pool.Add(H(4), Stock{3, 0});
	std::vector<uint32> Order;
	Pool.ForEach(
		[&](EntityHandle Handle, Stock& S)
		{
			Order.push_back(Handle.Index());
			S.Iron = S.Grain * 10;
		});
	const std::vector<uint32> Expected{2, 7, 4};
	VT_CHECK(Order == Expected);
	VT_CHECK_EQ(Pool.Get(H(4)).Iron, int64{30});
	int64 Total = 0;
	static_cast<const ComponentPool<Stock>&>(Pool).ForEach([&](EntityHandle, const Stock& S) { Total += S.Iron; });
	VT_CHECK_EQ(Total, int64{60});
}

VAELEN_TEST(ComponentPool, StateRoundTrip)
{
	ComponentPool<Stock> Source(StockType());
	for (uint32 i = 0; i < 20; ++i)
	{
		Source.Add(H(i * 3, 1 + (i % 2)), Stock{static_cast<int64>(i), -static_cast<int64>(i)});
	}
	Source.Remove(H(9, 1));
	Source.Remove(H(3, 1));
	ComponentPool<Stock> Restored(StockType());
	VT_REQUIRE(Restored.SetState(Source.GetState()));
	VT_CHECK_EQ(Restored.Size(), Source.Size());
	VT_CHECK(Restored.Entities() == Source.Entities());
	VT_CHECK(Restored.Data() == Source.Data());
	for (uint32 Dense = 0; Dense < Source.Size(); ++Dense)
	{
		const EntityHandle Handle = Source.EntityAt(Dense);
		VT_CHECK(Restored.Has(Handle));
		VT_CHECK(Restored.Get(Handle) == Source.Get(Handle));
	}
	VT_CHECK(!Restored.Has(H(9, 1)));
	// Operations after the restore behave as on the source.
	VT_CHECK(Restored.Remove(H(30, 1)) == Source.Remove(H(30, 1)));
	VT_CHECK(Restored.Entities() == Source.Entities());

	// Inconsistent states are rejected and leave the pool empty.
	ComponentPool<Stock>::State Bad = Source.GetState();
	Bad.Data.pop_back();
	VT_CHECK(!Restored.SetState(Bad));
	VT_CHECK(Restored.IsEmpty());
	ComponentPool<Stock>::State Duplicate = Source.GetState();
	Duplicate.Entities.push_back(Duplicate.Entities[0]);
	Duplicate.Data.push_back(Stock{});
	VT_CHECK(!Restored.SetState(Duplicate));
	ComponentPool<Stock>::State WithNull = Source.GetState();
	WithNull.Entities[0] = EntityHandle::Null();
	VT_CHECK(!Restored.SetState(WithNull));
	VT_CHECK(Restored.SetState(Source.GetState()));
}

VAELEN_TEST(ComponentPool, DeterministicAcrossInstances)
{
	auto Run = [](ComponentPool<Stock>& Pool)
	{
		for (uint32 Step = 0; Step < 20000; ++Step)
		{
			const uint32 Index = (Step * 7919u) % 500u;
			if (Pool.Has(H(Index)))
			{
				Pool.Remove(H(Index));
			}
			else
			{
				Pool.Add(H(Index), Stock{Step, 0});
			}
		}
	};
	ComponentPool<Stock> A(StockType());
	ComponentPool<Stock> B(StockType());
	Run(A);
	Run(B);
	VT_CHECK(A.Entities() == B.Entities());
	VT_CHECK(A.Data() == B.Data());
}

VAELEN_TEST(ComponentPool, OneMillionOperationsWithRegistry)
{
	// Soak against a real registry: components follow entity lifetimes.
	EntityRegistry Registry;
	IdAllocator Ids;
	ComponentPool<Stock> Pool(StockType());
	std::vector<EntityHandle> Live;
	for (uint32 Step = 0; Step < 1000000; ++Step)
	{
		const uint32 Choice = Step % 4;
		if (Choice == 0 || Live.size() < 100)
		{
			const EntityHandle Handle = Registry.Create(Ids, IdKind::Entity);
			Pool.Add(Handle, Stock{static_cast<int64>(Step), 0});
			Live.push_back(Handle);
		}
		else if (Choice == 1)
		{
			const usize Victim = (Step * 2654435761u) % Live.size();
			const EntityHandle Handle = Live[Victim];
			VT_REQUIRE(Pool.Remove(Handle));
			VT_REQUIRE(Registry.Destroy(Handle));
			Live[Victim] = Live.back();
			Live.pop_back();
		}
		else
		{
			const EntityHandle Handle = Live[(Step * 40503u) % Live.size()];
			Pool.Get(Handle).Iron += 1;
		}
	}
	VT_CHECK_EQ(Pool.Size(), static_cast<uint32>(Live.size()));
	VT_CHECK_EQ(Pool.Size(), Registry.GetAliveCount());
	for (uint32 Dense = 0; Dense < Pool.Size(); ++Dense)
	{
		VT_CHECK(Registry.IsAlive(Pool.EntityAt(Dense)));
	}
}

#if VAELEN_ASSERTS_ENABLED
VAELEN_TEST(ComponentPool, MisuseIsACheckFailure)
{
	VaelenTest::ScopedAssertCapture Capture;
	ComponentPool<Stock> Pool(StockType());
	Pool.Add(H(1), Stock{1, 1});
	Stock& Same = Pool.Add(H(1), Stock{9, 9}); // duplicate add: unchanged
	VT_CHECK_EQ(Same.Grain, int64{1});
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_EQ(Pool.Size(), uint32{1});
	(void)Pool.Get(H(2)); // missing: scratch value
	VT_CHECK_EQ(Capture.CheckCount, 2);
	Pool.Add(EntityHandle::Null());
	VT_CHECK_EQ(Capture.CheckCount, 3);
	VT_CHECK_EQ(Pool.Size(), uint32{1});
}
#endif
