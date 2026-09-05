// VAELEN - VaelenSim tests
// EntityHandle layout, null semantics, ordering and hashing.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/EntityHandle.h"

#include <type_traits>
#include <unordered_set>

using namespace Vaelen;

static_assert(sizeof(EntityHandle) == 8);
static_assert(std::is_trivially_copyable_v<EntityHandle>);
static_assert(EntityHandle::Null().IsNull());
static_assert(!EntityHandle::Make(0, EntityHandle::FirstGeneration).IsNull());
static_assert(EntityHandle::Make(7, 3).Index() == 7);
static_assert(EntityHandle::Make(7, 3).Generation() == 3);
static_assert(EntityHandle::Make(7, 3).Value == (uint64{3} << 32 | 7));

VAELEN_TEST(EntityHandle, LayoutRoundTrip)
{
	const EntityHandle H = EntityHandle::Make(EntityHandle::MaxIndex, EntityHandle::MaxGeneration);
	VT_CHECK_EQ(H.Index(), EntityHandle::MaxIndex);
	VT_CHECK_EQ(H.Generation(), EntityHandle::MaxGeneration);
	VT_CHECK(!H.IsNull());
	VT_CHECK(static_cast<bool>(H));

	const EntityHandle Low = EntityHandle::Make(0, 1);
	VT_CHECK_EQ(Low.Value, uint64{1} << 32);
	VT_CHECK_EQ(Low.Index(), uint32{0});
	VT_CHECK_EQ(Low.Generation(), uint32{1});
}

VAELEN_TEST(EntityHandle, NullIsDefaultAndNeverLive)
{
	EntityHandle Default;
	VT_CHECK(Default.IsNull());
	VT_CHECK(!Default);
	VT_CHECK(Default == EntityHandle::Null());
	VT_CHECK_EQ(Default.Value, uint64{0});
	// Index 0 with a live generation is a real handle, not null.
	VT_CHECK(!EntityHandle::Make(0, EntityHandle::FirstGeneration).IsNull());
}

VAELEN_TEST(EntityHandle, OrderingAndHashing)
{
	const EntityHandle A = EntityHandle::Make(1, 1);
	const EntityHandle B = EntityHandle::Make(2, 1);
	const EntityHandle C = EntityHandle::Make(1, 2);
	VT_CHECK(A < B);
	VT_CHECK(B < C); // generation is the high word
	VT_CHECK(A != C);
	VT_CHECK(A.Hash() != C.Hash());
	VT_CHECK_EQ(GetTypeHash(A), static_cast<uint32>(A.Hash() ^ (A.Hash() >> 32)));

	std::unordered_set<EntityHandle> Set;
	for (uint32 i = 0; i < 10000; ++i)
	{
		Set.insert(EntityHandle::Make(i, 1 + (i % 3)));
	}
	VT_CHECK_EQ(Set.size(), usize{10000});
}
