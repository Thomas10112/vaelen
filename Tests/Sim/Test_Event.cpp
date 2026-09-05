// VAELEN - VaelenSim tests
// Event layout, typed payloads, zero-filled bytes, hashing.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/Event.h"

#include <cstring>
#include <type_traits>

using namespace Vaelen;

namespace
{
	struct Harvest
	{
		int64 Grain = 0;
		uint32 Region = 0;
	};
	struct Big
	{
		uint8 Bytes[Event::MaxPayloadBytes] = {};
	};
	constexpr EventType<Harvest> HarvestEvent = MakeEventType<Harvest>("Harvest");
	constexpr EventType<NoPayload> DawnEvent = MakeEventType<NoPayload>("Dawn");
} // namespace

static_assert(sizeof(Event) == 112);
static_assert(std::is_trivially_copyable_v<Event>);
static_assert(HarvestEvent.TypeHash == HashString("Harvest"));
static_assert(HarvestEvent.IsValid());
static_assert(!EventType<Harvest>{}.IsValid());
static_assert(Event::PayloadBytesOf<NoPayload>() == 0);
static_assert(Event::PayloadBytesOf<Harvest>() == sizeof(Harvest));

VAELEN_TEST(Event, TypedPayloadRoundTrip)
{
	Event E{};
	E.Tick = 7;
	E.TypeHash = HarvestEvent.TypeHash;
	E.Set(Harvest{1200, 3});
	VT_CHECK_EQ(E.PayloadSize, uint32{sizeof(Harvest)});
	VT_CHECK(E.Is(HarvestEvent));
	VT_CHECK(!E.Is(DawnEvent));
	const Harvest H = E.Get<Harvest>();
	VT_CHECK_EQ(H.Grain, int64{1200});
	VT_CHECK_EQ(H.Region, uint32{3});

	// Bytes after the payload are zero, so the raw image is fully defined.
	bool TailIsZero = true;
	for (uint32 i = sizeof(Harvest); i < Event::MaxPayloadBytes; ++i)
	{
		TailIsZero = TailIsZero && E.Payload[i] == 0;
	}
	VT_CHECK(TailIsZero);
	VT_CHECK_EQ(E.Reserved, uint32{0});

	Event Empty{};
	Empty.TypeHash = DawnEvent.TypeHash;
	Empty.Set(NoPayload{});
	VT_CHECK_EQ(Empty.PayloadSize, uint32{0});
	VT_CHECK(Empty.Is(DawnEvent));

	Big Full;
	for (uint32 i = 0; i < Event::MaxPayloadBytes; ++i)
	{
		Full.Bytes[i] = static_cast<uint8>(i);
	}
	Event Max{};
	Max.Set(Full);
	VT_CHECK_EQ(Max.PayloadSize, Event::MaxPayloadBytes);
	VT_CHECK(std::memcmp(Max.Get<Big>().Bytes, Full.Bytes, Event::MaxPayloadBytes) == 0);
}

VAELEN_TEST(Event, HashCoversEveryField)
{
	Event A{};
	A.Set(Harvest{1, 1});
	Event B = A;
	VT_CHECK(A == B);
	VT_CHECK_EQ(A.Hash(), B.Hash());
	B.Tick = 1;
	VT_CHECK(A.Hash() != B.Hash());
	B = A;
	B.Cause = PersistentId::Make(IdKind::Event, 9);
	VT_CHECK(A.Hash() != B.Hash());
	B = A;
	B.Set(Harvest{1, 2});
	VT_CHECK(A.Hash() != B.Hash());
	B = A;
	B.Payload[Event::MaxPayloadBytes - 1] = 1; // beyond PayloadSize but still hashed
	VT_CHECK(A.Hash() != B.Hash());
}
