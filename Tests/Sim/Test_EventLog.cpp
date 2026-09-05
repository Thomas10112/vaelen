// VAELEN - VaelenSim tests
// EventLog: append-only, running digest, byte round trip, corruption rejected.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/EventBus.h"

#include <vector>

using namespace Vaelen;

namespace
{
	struct Payload
	{
		uint64 Value = 0;
	};
	constexpr EventType<Payload> PayloadEvent = MakeEventType<Payload>("Payload");

	Event Make(uint64 Serial, SimTick Tick, uint64 Value, PersistentId Cause = {})
	{
		Event E{};
		E.Id = PersistentId::Make(IdKind::Event, Serial);
		E.Tick = Tick;
		E.TypeHash = PayloadEvent.TypeHash;
		E.Cause = Cause;
		E.Set(Payload{Value});
		return E;
	}
} // namespace

VAELEN_TEST(EventLog, AppendOnlyWithRunningDigest)
{
	EventLog Log;
	VT_CHECK_EQ(Log.Count(), uint64{0});
	VT_CHECK_EQ(Log.Digest(), EventLog::EmptyDigest);
	Log.Append(Make(1, 0, 10));
	const Hash64 AfterOne = Log.Digest();
	VT_CHECK(AfterOne != EventLog::EmptyDigest);
	Log.Append(Make(2, 0, 20, PersistentId::Make(IdKind::Event, 1)));
	Log.Append(Make(3, 1, 30, PersistentId::Make(IdKind::Event, 1)));
	VT_CHECK_EQ(Log.Count(), uint64{3});
	VT_CHECK(Log.Digest() != AfterOne);
	VT_CHECK_EQ(Log.At(2).Get<Payload>().Value, uint64{30});
	VT_CHECK_EQ(Log.CountCausedBy(PersistentId::Make(IdKind::Event, 1)), uint64{2});
	VT_CHECK_EQ(Log.CountCausedBy(PersistentId::Invalid()), uint64{1});

	// Same events in the same order: same digest. Different order: different.
	EventLog Same;
	Same.Append(Make(1, 0, 10));
	Same.Append(Make(2, 0, 20, PersistentId::Make(IdKind::Event, 1)));
	Same.Append(Make(3, 1, 30, PersistentId::Make(IdKind::Event, 1)));
	VT_CHECK_EQ(Same.Digest(), Log.Digest());
	EventLog Swapped;
	Swapped.Append(Make(2, 0, 20, PersistentId::Make(IdKind::Event, 1)));
	Swapped.Append(Make(1, 0, 10));
	Swapped.Append(Make(3, 1, 30, PersistentId::Make(IdKind::Event, 1)));
	VT_CHECK(Swapped.Digest() != Log.Digest());

	Log.Clear();
	VT_CHECK_EQ(Log.Count(), uint64{0});
	VT_CHECK_EQ(Log.Digest(), EventLog::EmptyDigest);
}

VAELEN_TEST(EventLog, ByteRoundTripAndCorruptionDetection)
{
	EventLog Log;
	for (uint64 i = 1; i <= 100; ++i)
	{
		Log.Append(Make(i, i / 3, i * i));
	}
	std::vector<uint8> Bytes;
	Log.WriteTo(Bytes);
	VT_CHECK_EQ(Bytes.size(), usize{16 + 100 * sizeof(Event)});

	EventLog Restored;
	VT_REQUIRE(Restored.ReadFrom(Bytes.data(), Bytes.size()));
	VT_CHECK_EQ(Restored.Count(), uint64{100});
	VT_CHECK_EQ(Restored.Digest(), Log.Digest());
	VT_CHECK(Restored.All() == Log.All());

	// Appending after a restore continues the same digest chain.
	Restored.Append(Make(101, 40, 1));
	Log.Append(Make(101, 40, 1));
	VT_CHECK_EQ(Restored.Digest(), Log.Digest());

	// Truncated image, flipped byte, wrong count: all rejected, log left empty.
	VT_CHECK(!Restored.ReadFrom(Bytes.data(), Bytes.size() - 1));
	VT_CHECK_EQ(Restored.Count(), uint64{0});
	std::vector<uint8> Flipped = Bytes;
	Flipped[16 + 40] ^= 0x01;
	VT_CHECK(!Restored.ReadFrom(Flipped.data(), Flipped.size()));
	std::vector<uint8> WrongCount = Bytes;
	WrongCount[0] = 99;
	VT_CHECK(!Restored.ReadFrom(WrongCount.data(), WrongCount.size()));
	VT_CHECK(!Restored.ReadFrom(nullptr, 0));
	VT_CHECK(!Restored.ReadFrom(Bytes.data(), 8));

	// An empty log round-trips too.
	EventLog Empty;
	std::vector<uint8> EmptyBytes;
	Empty.WriteTo(EmptyBytes);
	VT_CHECK_EQ(EmptyBytes.size(), usize{16});
	VT_CHECK(Restored.ReadFrom(EmptyBytes.data(), EmptyBytes.size()));
	VT_CHECK_EQ(Restored.Digest(), EventLog::EmptyDigest);
}
