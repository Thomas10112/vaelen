// VAELEN - VaelenSim tests
// ComponentTypeRegistry: ordered ids, name lookup, digest, assertion paths.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/ComponentType.h"

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
	struct alignas(16) Wide
	{
		uint64 A = 0;
		uint64 B = 0;
	};
} // namespace

static_assert(sizeof(ComponentTypeId) == 2);
static_assert(!ComponentType<Position>{}.IsValid());

VAELEN_TEST(ComponentType, RegistrationOrderDefinesIds)
{
	ComponentTypeRegistry Types;
	VT_CHECK_EQ(Types.Count(), uint32{0});
	const ComponentType<Position> P = Types.Register<Position>("Position");
	const ComponentType<Health> H = Types.Register<Health>("Health");
	const ComponentType<Wide> W = Types.Register<Wide>("Wide");
	VT_CHECK_EQ(P.Id, ComponentTypeId{0});
	VT_CHECK_EQ(H.Id, ComponentTypeId{1});
	VT_CHECK_EQ(W.Id, ComponentTypeId{2});
	VT_CHECK(P.IsValid() && H.IsValid() && W.IsValid());
	VT_CHECK_EQ(Types.Count(), uint32{3});

	const ComponentTypeInfo& Info = Types.GetInfo(W.Id);
	VT_CHECK_STREQ(Info.Name, "Wide");
	VT_CHECK_EQ(Info.NameHash, HashString("Wide"));
	VT_CHECK_EQ(Info.Size, uint32{sizeof(Wide)});
	VT_CHECK_EQ(Info.Alignment, uint32{16});
	VT_CHECK(Types.IsValid(2));
	VT_CHECK(!Types.IsValid(3));
	VT_CHECK(!Types.IsValid(InvalidComponentTypeId));
	VT_CHECK_STREQ(Types.GetInfo(99).Name, "");
}

VAELEN_TEST(ComponentType, LookupByNameAndHash)
{
	ComponentTypeRegistry Types;
	Types.Register<Position>("Position");
	Types.Register<Health>("Health");
	VT_CHECK_EQ(Types.FindByName("Health"), ComponentTypeId{1});
	VT_CHECK_EQ(Types.FindByHash("Position"_vhash), ComponentTypeId{0});
	VT_CHECK_EQ(Types.FindByName("Missing"), InvalidComponentTypeId);
	VT_CHECK_EQ(Types.FindByName(""), InvalidComponentTypeId);
}

VAELEN_TEST(ComponentType, LayoutDigestIsOrderSensitiveAndDeterministic)
{
	ComponentTypeRegistry A;
	A.Register<Position>("Position");
	A.Register<Health>("Health");
	ComponentTypeRegistry B;
	B.Register<Position>("Position");
	B.Register<Health>("Health");
	ComponentTypeRegistry C;
	C.Register<Health>("Health");
	C.Register<Position>("Position");
	ComponentTypeRegistry D;
	D.Register<Wide>("Position"); // same name, other layout
	D.Register<Health>("Health");
	VT_CHECK_EQ(A.LayoutDigest(), B.LayoutDigest());
	VT_CHECK_NE(A.LayoutDigest(), C.LayoutDigest());
	VT_CHECK_NE(A.LayoutDigest(), D.LayoutDigest());
	VT_CHECK_NE(ComponentTypeRegistry{}.LayoutDigest(), A.LayoutDigest());
}

VAELEN_TEST(ComponentType, InvalidRegistrationsAreRejected)
{
	VaelenTest::ScopedAssertCapture Capture;
	ComponentTypeRegistry Types;
	const ComponentType<Position> P = Types.Register<Position>("Position");
	VT_CHECK(P.IsValid());
	const ComponentType<Health> Duplicate = Types.Register<Health>("Position");
	VT_CHECK(!Duplicate.IsValid());
	const ComponentType<Health> Empty = Types.Register<Health>("");
	VT_CHECK(!Empty.IsValid());
	const ComponentType<Health> Null = Types.Register<Health>(nullptr);
	VT_CHECK(!Null.IsValid());
	VT_CHECK_EQ(Types.Count(), uint32{1});
#if VAELEN_ASSERTS_ENABLED
	VT_CHECK_EQ(Capture.CheckCount, 3);
#else
	VT_CHECK_EQ(Capture.CheckCount, 0);
#endif
}
