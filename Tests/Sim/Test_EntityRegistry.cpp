// VAELEN - VaelenSim tests
// EntityRegistry: create/destroy, generations, id lookup, free-list reuse,
// determinism, snapshot state, assertion paths, one-million-cycle soak.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Ids.h"
#include "Vaelen/Sim/EntityRegistry.h"

#include <vector>

using namespace Vaelen;

namespace
{
	PersistentId Person(uint64 Serial)
	{
		return PersistentId::Make(IdKind::Person, Serial);
	}
} // namespace

VAELEN_TEST(EntityRegistry, CreateAssignsDenseSlotsAndFirstGeneration)
{
	EntityRegistry Registry;
	const EntityHandle A = Registry.Create(Person(1));
	const EntityHandle B = Registry.Create(Person(2));
	VT_CHECK_EQ(A.Index(), uint32{0});
	VT_CHECK_EQ(B.Index(), uint32{1});
	VT_CHECK_EQ(A.Generation(), EntityHandle::FirstGeneration);
	VT_CHECK_EQ(B.Generation(), EntityHandle::FirstGeneration);
	VT_CHECK(Registry.IsAlive(A));
	VT_CHECK(Registry.IsAlive(B));
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{2});
	VT_CHECK_EQ(Registry.GetSlotCount(), uint32{2});
	VT_CHECK_EQ(Registry.GetFreeCount(), uint32{0});
	VT_CHECK(Registry.GetId(A) == Person(1));
	VT_CHECK(Registry.Find(Person(2)) == B);
	VT_CHECK(Registry.Find(Person(3)).IsNull());
	VT_CHECK(Registry.Find(PersistentId::Invalid()).IsNull());
}

VAELEN_TEST(EntityRegistry, CreateFromAllocator)
{
	IdAllocator Ids;
	EntityRegistry Registry;
	const EntityHandle A = Registry.Create(Ids, IdKind::Settlement);
	const EntityHandle B = Registry.Create(Ids, IdKind::Settlement);
	VT_CHECK(Registry.GetId(A) == PersistentId::Make(IdKind::Settlement, 1));
	VT_CHECK(Registry.GetId(B) == PersistentId::Make(IdKind::Settlement, 2));
	VT_CHECK_EQ(Ids.GetAllocatedCount(IdKind::Settlement), uint64{2});
}

VAELEN_TEST(EntityRegistry, DestroyInvalidatesHandleAndRecyclesSlot)
{
	EntityRegistry Registry;
	const EntityHandle A = Registry.Create(Person(1));
	const EntityHandle B = Registry.Create(Person(2));
	VT_CHECK(Registry.Destroy(A));
	VT_CHECK(!Registry.IsAlive(A));
	VT_CHECK(Registry.GetId(A) == PersistentId::Invalid());
	VT_CHECK(Registry.Find(Person(1)).IsNull());
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{1});
	VT_CHECK_EQ(Registry.GetFreeCount(), uint32{1});
	VT_CHECK(!Registry.Destroy(A)); // already destroyed
	VT_CHECK(Registry.IsAlive(B));

	// The slot is reused with the next generation; the old handle stays dead.
	const EntityHandle C = Registry.Create(Person(3));
	VT_CHECK_EQ(C.Index(), A.Index());
	VT_CHECK_EQ(C.Generation(), A.Generation() + 1);
	VT_CHECK(Registry.IsAlive(C));
	VT_CHECK(!Registry.IsAlive(A));
	VT_CHECK(Registry.GetId(A) == PersistentId::Invalid());
	VT_CHECK(Registry.GetId(C) == Person(3));
	VT_CHECK_EQ(Registry.GetSlotCount(), uint32{2});
	VT_CHECK_EQ(Registry.GetFreeCount(), uint32{0});
}

VAELEN_TEST(EntityRegistry, FreeListIsLifo)
{
	EntityRegistry Registry;
	EntityHandle H[4];
	for (uint64 i = 0; i < 4; ++i)
	{
		H[i] = Registry.Create(Person(i + 1));
	}
	VT_CHECK(Registry.Destroy(H[1]));
	VT_CHECK(Registry.Destroy(H[3]));
	VT_CHECK(Registry.Destroy(H[2]));
	VT_CHECK_EQ(Registry.GetFreeCount(), uint32{3});
	// Last freed is reused first: 2, then 3, then 1.
	VT_CHECK_EQ(Registry.Create(Person(10)).Index(), uint32{2});
	VT_CHECK_EQ(Registry.Create(Person(11)).Index(), uint32{3});
	VT_CHECK_EQ(Registry.Create(Person(12)).Index(), uint32{1});
	VT_CHECK_EQ(Registry.Create(Person(13)).Index(), uint32{4}); // list empty: grow
}

VAELEN_TEST(EntityRegistry, InvalidHandlesAreRejected)
{
	EntityRegistry Registry;
	const EntityHandle A = Registry.Create(Person(1));
	VT_CHECK(!Registry.IsAlive(EntityHandle::Null()));
	VT_CHECK(!Registry.Destroy(EntityHandle::Null()));
	VT_CHECK(!Registry.IsAlive(EntityHandle::Make(99, 1)));		   // out of range
	VT_CHECK(!Registry.IsAlive(EntityHandle::Make(A.Index(), 2))); // wrong generation
	VT_CHECK(!Registry.IsAlive(EntityHandle::Make(A.Index(), 0))); // never a live generation
	VT_CHECK(!Registry.Destroy(EntityHandle::Make(99, 1)));
	VT_CHECK(Registry.GetId(EntityHandle::Make(99, 1)) == PersistentId::Invalid());
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{1});
}

VAELEN_TEST(EntityRegistry, ForEachAliveVisitsInSlotOrder)
{
	EntityRegistry Registry;
	for (uint64 i = 1; i <= 6; ++i)
	{
		Registry.Create(Person(i));
	}
	VT_CHECK(Registry.Destroy(Registry.Find(Person(2))));
	VT_CHECK(Registry.Destroy(Registry.Find(Person(5))));
	Registry.Create(Person(7)); // reuses slot 4 (LIFO: 5 was freed last)

	std::vector<uint64> Serials;
	std::vector<uint32> Indices;
	Registry.ForEachAlive(
		[&](EntityHandle Handle, PersistentId Id)
		{
			Indices.push_back(Handle.Index());
			Serials.push_back(Id.Serial());
		});
	const std::vector<uint32> ExpectedIndices{0, 2, 3, 4, 5};
	const std::vector<uint64> ExpectedSerials{1, 3, 4, 7, 6};
	VT_CHECK(Indices == ExpectedIndices);
	VT_CHECK(Serials == ExpectedSerials);
}

VAELEN_TEST(EntityRegistry, DeterministicAcrossInstances)
{
	// Same operation sequence -> identical handles and identical state.
	auto Run = [](EntityRegistry& Registry, std::vector<EntityHandle>& Out)
	{
		IdAllocator Ids;
		std::vector<EntityHandle> Live;
		for (uint32 Step = 0; Step < 5000; ++Step)
		{
			if (Step % 3 != 2 || Live.empty())
			{
				Live.push_back(Registry.Create(Ids, IdKind::Item));
			}
			else
			{
				const usize Victim = (Step * 7919u) % Live.size();
				Registry.Destroy(Live[Victim]);
				Live.erase(Live.begin() + static_cast<std::ptrdiff_t>(Victim));
			}
			Out.push_back(Live.back());
		}
	};
	EntityRegistry A;
	EntityRegistry B;
	std::vector<EntityHandle> HandlesA;
	std::vector<EntityHandle> HandlesB;
	Run(A, HandlesA);
	Run(B, HandlesB);
	VT_CHECK(HandlesA == HandlesB);
	VT_CHECK(A.GetState() == B.GetState());
	VT_CHECK_EQ(A.GetAliveCount(), B.GetAliveCount());
}

VAELEN_TEST(EntityRegistry, StateRoundTripRestoresHandlesExactly)
{
	EntityRegistry Source;
	IdAllocator Ids;
	std::vector<EntityHandle> Handles;
	for (int i = 0; i < 50; ++i)
	{
		Handles.push_back(Source.Create(Ids, IdKind::Family));
	}
	for (int i = 0; i < 50; i += 4)
	{
		Source.Destroy(Handles[static_cast<usize>(i)]);
	}
	Handles.push_back(Source.Create(Ids, IdKind::Family)); // reuses a slot with generation 2

	EntityRegistry Restored;
	VT_REQUIRE(Restored.SetState(Source.GetState()));
	VT_CHECK(Restored.GetState() == Source.GetState());
	VT_CHECK_EQ(Restored.GetAliveCount(), Source.GetAliveCount());
	VT_CHECK_EQ(Restored.GetFreeCount(), Source.GetFreeCount());
	for (const EntityHandle& H : Handles)
	{
		VT_CHECK_EQ(Restored.IsAlive(H), Source.IsAlive(H));
		VT_CHECK(Restored.GetId(H) == Source.GetId(H));
	}
	// Lookups were rebuilt, and the next allocation matches the source.
	Source.ForEachAlive([&](EntityHandle H, PersistentId Id) { VT_CHECK(Restored.Find(Id) == H); });
	VT_CHECK(Restored.Create(Person(999)) == Source.Create(Person(999)));
}

VAELEN_TEST(EntityRegistry, SetStateRejectsInconsistentStates)
{
	EntityRegistry Registry;
	Registry.Create(Person(1));
	EntityRegistry::State Good = Registry.GetState();

	EntityRegistry::State DuplicateId = Good;
	DuplicateId.Slots.push_back(DuplicateId.Slots[0]);
	DuplicateId.AliveCount = 2;
	VT_CHECK(!Registry.SetState(DuplicateId));
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{0}); // left empty

	EntityRegistry::State WrongCount = Good;
	WrongCount.AliveCount = 5;
	VT_CHECK(!Registry.SetState(WrongCount));

	EntityRegistry::State FreeListCycle = Good;
	EntityRegistry::Slot Free;
	Free.Generation = 2;
	Free.NextFree = 1; // points at itself
	FreeListCycle.Slots.push_back(Free);
	FreeListCycle.FreeHead = 1;
	VT_CHECK(!Registry.SetState(FreeListCycle));

	EntityRegistry::State LiveOnFreeList = Good;
	LiveOnFreeList.FreeHead = 0;
	VT_CHECK(!Registry.SetState(LiveOnFreeList));

	EntityRegistry::State DeadWithId = Good;
	DeadWithId.Slots[0].Alive = false;
	DeadWithId.AliveCount = 0;
	VT_CHECK(!Registry.SetState(DeadWithId)); // a free slot must not carry an id

	VT_CHECK(Registry.SetState(Good));
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{1});
}

VAELEN_TEST(EntityRegistry, ExhaustedGenerationRetiresTheSlot)
{
	EntityRegistry Registry;
	EntityRegistry::State St;
	EntityRegistry::Slot S;
	S.Id = Person(1);
	S.Generation = EntityHandle::MaxGeneration;
	S.Alive = true;
	St.Slots.push_back(S);
	St.AliveCount = 1;
	VT_REQUIRE(Registry.SetState(St));

	const EntityHandle Last = EntityHandle::Make(0, EntityHandle::MaxGeneration);
	VT_CHECK(Registry.IsAlive(Last));
	VT_CHECK(Registry.Destroy(Last));
	VT_CHECK(!Registry.IsAlive(Last));
	VT_CHECK_EQ(Registry.GetFreeCount(), uint32{0}); // retired, not on the free list
	VT_CHECK(Registry.GetState().Slots[0].Retired);
	const EntityHandle Next = Registry.Create(Person(2));
	VT_CHECK_EQ(Next.Index(), uint32{1}); // a new slot, never slot 0 again
	VT_CHECK(!Registry.IsAlive(Last));
}

VAELEN_TEST(EntityRegistry, ClearEmptiesEverything)
{
	EntityRegistry Registry;
	const EntityHandle A = Registry.Create(Person(1));
	Registry.Clear();
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{0});
	VT_CHECK_EQ(Registry.GetSlotCount(), uint32{0});
	VT_CHECK(!Registry.IsAlive(A));
	VT_CHECK(Registry.Find(Person(1)).IsNull());
	VT_CHECK_EQ(Registry.Create(Person(1)).Index(), uint32{0});
}

VAELEN_TEST(EntityRegistry, OneMillionCreateDestroyCycles)
{
	// Long-duration soak: the table stays bounded and every invariant holds.
	EntityRegistry Registry;
	IdAllocator Ids;
	std::vector<EntityHandle> Live;
	Live.reserve(1024);
	uint64 Destroyed = 0;
	for (uint32 Step = 0; Step < 1000000; ++Step)
	{
		if (Live.size() < 1000 && (Step & 1) == 0)
		{
			Live.push_back(Registry.Create(Ids, IdKind::Person));
		}
		else if (!Live.empty())
		{
			const usize Victim = (Step * 2654435761u) % Live.size();
			const EntityHandle H = Live[Victim];
			VT_REQUIRE(Registry.IsAlive(H));
			VT_REQUIRE(Registry.Destroy(H));
			VT_REQUIRE(!Registry.IsAlive(H));
			++Destroyed;
			Live[Victim] = Live.back();
			Live.pop_back();
		}
	}
	VT_CHECK_EQ(Registry.GetAliveCount(), static_cast<uint32>(Live.size()));
	VT_CHECK(Registry.GetSlotCount() <= 1000);
	VT_CHECK_EQ(Registry.GetSlotCount(), Registry.GetAliveCount() + Registry.GetFreeCount());
	VT_CHECK_EQ(Ids.GetAllocatedCount(IdKind::Person), Destroyed + Live.size());
	for (const EntityHandle& H : Live)
	{
		VT_CHECK(Registry.IsAlive(H));
		VT_CHECK(Registry.Find(Registry.GetId(H)) == H);
	}
}

#if VAELEN_ASSERTS_ENABLED
VAELEN_TEST(EntityRegistry, CreateWithInvalidOrDuplicateIdIsACheckFailure)
{
	VaelenTest::ScopedAssertCapture Capture;
	EntityRegistry Registry;
	VT_CHECK(Registry.Create(PersistentId::Invalid()).IsNull());
	VT_CHECK_EQ(Capture.CheckCount, 1);
	const EntityHandle A = Registry.Create(Person(1));
	VT_CHECK(!A.IsNull());
	VT_CHECK(Registry.Create(Person(1)).IsNull());
	VT_CHECK_EQ(Capture.CheckCount, 2);
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{1});
	VT_CHECK_EQ(Registry.GetSlotCount(), uint32{1});
}
#else
VAELEN_TEST(EntityRegistry, CreateWithInvalidOrDuplicateIdReturnsNull)
{
	EntityRegistry Registry;
	VT_CHECK(Registry.Create(PersistentId::Invalid()).IsNull());
	VT_CHECK(!Registry.Create(Person(1)).IsNull());
	VT_CHECK(Registry.Create(Person(1)).IsNull());
	VT_CHECK_EQ(Registry.GetAliveCount(), uint32{1});
}
#endif
