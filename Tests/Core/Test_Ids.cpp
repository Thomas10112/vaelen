// VAELEN - VaelenCore tests
// PersistentId layout, validity, ordering and hashing; IdKind names; the
// deterministic IdAllocator (per-kind counters, reservation, state round
// trip, reset, determinism) and its assertion paths.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Ids.h"

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace Vaelen;

namespace
{
	// Every enumerator of IdKind in declaration order. Keep in sync with Ids.h.
	constexpr IdKind AllKinds[] = {
		IdKind::None,
		IdKind::Entity,
		IdKind::Event,
		IdKind::Region,
		IdKind::Tile,
		IdKind::River,
		IdKind::ResourceDeposit,
		IdKind::Lake,
		IdKind::Culture,
		IdKind::Language,
		IdKind::Religion,
		IdKind::Person,
		IdKind::Family,
		IdKind::Organization,
		IdKind::Item,
		IdKind::Building,
		IdKind::Settlement,
		IdKind::Market,
		IdKind::Route,
		IdKind::Polity,
		IdKind::Law,
		IdKind::Army,
		IdKind::War,
		IdKind::Document,
		IdKind::Map,
		IdKind::MaxValue,
	};
	constexpr usize AllKindCount = ArrayCount(AllKinds);

	// Kinds that may be handed to IdAllocator::Allocate (everything but None).
	constexpr IdKind AllocatableKinds[] = {
		IdKind::Entity,	  IdKind::Event,		   IdKind::Region,	   IdKind::Tile,
		IdKind::River,	  IdKind::ResourceDeposit, IdKind::Culture,	   IdKind::Language,
		IdKind::Religion, IdKind::Person,		   IdKind::Family,	   IdKind::Organization,
		IdKind::Item,	  IdKind::Building,		   IdKind::Settlement, IdKind::Market,
		IdKind::Route,	  IdKind::Polity,		   IdKind::Law,		   IdKind::Army,
		IdKind::War,	  IdKind::Document,		   IdKind::Map,		   IdKind::MaxValue,
	};
	constexpr usize AllocatableKindCount = ArrayCount(AllocatableKinds);

	constexpr uint64 Pow2_56 = uint64{1} << 56;

	// ── Compile-time contract ────────────────────────────────────────────────
	static_assert(sizeof(PersistentId) == 8, "PersistentId must be exactly 64-bit");
	static_assert(std::is_trivially_copyable_v<PersistentId>, "PersistentId must be trivially copyable");
	static_assert(std::is_trivially_destructible_v<PersistentId>);
	static_assert(std::is_standard_layout_v<PersistentId>, "PersistentId must be serialisable as raw bytes");
	static_assert(!std::is_convertible_v<PersistentId, bool>, "operator bool must be explicit");
	static_assert(std::is_constructible_v<bool, PersistentId>, "explicit bool conversion must exist");
	static_assert(PersistentId::KindBits == 8);
	static_assert(PersistentId::SerialBits == 56);
	static_assert(PersistentId::KindBits + PersistentId::SerialBits == 64);
	static_assert(PersistentId::SerialMask == 0x00FFFFFFFFFFFFFFull);
	static_assert(PersistentId::MaxSerial == Pow2_56 - 1, "MaxSerial must be 2^56 - 1");
	static_assert(IdAllocator::KindCount == 256, "one counter per possible kind byte");
	static_assert(std::is_trivially_copyable_v<IdAllocator::State>, "State must be serialisable as-is");
	static_assert(sizeof(IdAllocator::State) == 256 * sizeof(uint64));

	// PersistentId is usable in constant expressions.
	constexpr PersistentId ConstexprPerson = PersistentId::Make(IdKind::Person, 7);
	static_assert(ConstexprPerson.Kind() == IdKind::Person);
	static_assert(ConstexprPerson.Serial() == 7);
	static_assert(ConstexprPerson.IsValid());
	static_assert(ConstexprPerson.IsKind(IdKind::Person));
	static_assert(!ConstexprPerson.IsKind(IdKind::Family));
	static_assert(static_cast<bool>(ConstexprPerson));
	static_assert(!PersistentId::Invalid().IsValid());
	static_assert(PersistentId::Invalid().Value == 0);
	static_assert(PersistentId::Make(IdKind::Person, 7) == ConstexprPerson);
	static_assert(PersistentId::Make(IdKind::Person, 6) < ConstexprPerson);
	static_assert(ConstexprPerson.Hash() == HashUInt64(ConstexprPerson.Value));

	uint64 KindBitsOf(IdKind Kind) noexcept
	{
		return static_cast<uint64>(ToUnderlying(Kind)) << PersistentId::SerialBits;
	}
} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// PersistentId
// ═════════════════════════════════════════════════════════════════════════════

VAELEN_TEST(Ids, LayoutRoundTrip)
{
	const uint64 Serials[] = {
		0, 1, 2, 255, 256, 65535, 0x123456789ABCull, PersistentId::MaxSerial - 1, PersistentId::MaxSerial};
	for (usize k = 0; k < AllKindCount; ++k)
	{
		const IdKind Kind = AllKinds[k];
		for (uint64 Serial : Serials)
		{
			const PersistentId Id = PersistentId::Make(Kind, Serial);
			VT_CHECK_EQ(Id.Kind(), Kind);
			VT_CHECK_EQ(Id.Serial(), Serial);
			VT_CHECK(Id.IsKind(Kind));
			// Raw layout: kind in the top 8 bits, serial in the low 56 bits.
			VT_CHECK_EQ(Id.Value, KindBitsOf(Kind) | Serial);
			VT_CHECK_EQ(Id.Value >> 56, static_cast<uint64>(ToUnderlying(Kind)));
			VT_CHECK_EQ(Id.Value & PersistentId::SerialMask, Serial);
			// The raw-value constructor reproduces the same id.
			VT_CHECK(PersistentId(Id.Value) == Id);
		}
	}

	// Spot check with literal bit patterns.
	VT_CHECK_EQ(PersistentId::Make(IdKind::Person, 42).Value, uint64{0x170000000000002A});
	VT_CHECK_EQ(PersistentId::Make(IdKind::Entity, 1).Value, uint64{0x0100000000000001});
	VT_CHECK_EQ(PersistentId::Make(IdKind::MaxValue, PersistentId::MaxSerial).Value, ~uint64{0});
	VT_CHECK_EQ(PersistentId(uint64{0x170000000000002A}).Kind(), IdKind::Person);
	VT_CHECK_EQ(PersistentId(uint64{0x170000000000002A}).Serial(), uint64{42});
}

VAELEN_TEST(Ids, LayoutSerialIsMaskedTo56Bits)
{
	VT_CHECK_EQ(sizeof(PersistentId), usize{8});
	VT_CHECK_EQ(PersistentId::MaxSerial, Pow2_56 - 1);
	VT_CHECK_EQ(PersistentId::MaxSerial, uint64{0x00FFFFFFFFFFFFFF});
	VT_CHECK_EQ(PersistentId::MaxSerial, uint64{72057594037927935});

	// Bits above the 56th are dropped and never leak into the kind field.
	const PersistentId Overflow = PersistentId::Make(IdKind::Person, Pow2_56 + 4);
	VT_CHECK_EQ(Overflow.Kind(), IdKind::Person);
	VT_CHECK_EQ(Overflow.Serial(), uint64{4});
	VT_CHECK_EQ(Overflow.Value, KindBitsOf(IdKind::Person) | uint64{4});

	const PersistentId AllBits = PersistentId::Make(IdKind::Entity, ~uint64{0});
	VT_CHECK_EQ(AllBits.Kind(), IdKind::Entity);
	VT_CHECK_EQ(AllBits.Serial(), PersistentId::MaxSerial);
	VT_CHECK_EQ(AllBits.Value, KindBitsOf(IdKind::Entity) | PersistentId::MaxSerial);

	// A serial of exactly 2^56 masks to 0, which makes the id invalid.
	const PersistentId Wrapped = PersistentId::Make(IdKind::Person, Pow2_56);
	VT_CHECK_EQ(Wrapped.Kind(), IdKind::Person);
	VT_CHECK_EQ(Wrapped.Serial(), uint64{0});
	VT_CHECK(!Wrapped.IsValid());
}

VAELEN_TEST(Ids, InvalidIds)
{
	const PersistentId Default{};
	VT_CHECK_EQ(Default.Value, uint64{0});
	VT_CHECK(!Default.IsValid());
	VT_CHECK(!Default);
	VT_CHECK(!static_cast<bool>(Default));
	VT_CHECK_EQ(Default.Kind(), IdKind::None);
	VT_CHECK_EQ(Default.Serial(), uint64{0});
	VT_CHECK(Default == PersistentId::Invalid());
	VT_CHECK(!PersistentId::Invalid().IsValid());
	VT_CHECK(!PersistentId::Invalid());

	// Kind None is invalid whatever the serial.
	const PersistentId NoneKind = PersistentId::Make(IdKind::None, 5);
	VT_CHECK_EQ(NoneKind.Kind(), IdKind::None);
	VT_CHECK_EQ(NoneKind.Serial(), uint64{5});
	VT_CHECK(!NoneKind.IsValid());
	VT_CHECK(!NoneKind);
	VT_CHECK(NoneKind != PersistentId::Invalid());

	// Serial 0 is invalid whatever the kind.
	const PersistentId ZeroSerial = PersistentId::Make(IdKind::Person, 0);
	VT_CHECK_EQ(ZeroSerial.Kind(), IdKind::Person);
	VT_CHECK_EQ(ZeroSerial.Serial(), uint64{0});
	VT_CHECK(!ZeroSerial.IsValid());
	VT_CHECK(!ZeroSerial);
	VT_CHECK(ZeroSerial.IsKind(IdKind::Person)); // the kind bits are still readable
	VT_CHECK(ZeroSerial != PersistentId::Invalid());

	// Explicit operator bool: usable in boolean contexts, never implicitly.
	const PersistentId Valid = PersistentId::Make(IdKind::Person, 1);
	VT_CHECK(Valid.IsValid());
	VT_CHECK(static_cast<bool>(Valid));
	bool Taken = false;
	if (Valid)
	{
		Taken = true;
	}
	VT_CHECK(Taken);
	VT_CHECK(Valid && !ZeroSerial);
	VT_CHECK(Valid ? true : false);
	VT_CHECK(Valid.IsKind(IdKind::Person));
	VT_CHECK(!Valid.IsKind(IdKind::Family));
	VT_CHECK(!Valid.IsKind(IdKind::None));
}

VAELEN_TEST(Ids, OrderingAndEquality)
{
	const PersistentId A = PersistentId::Make(IdKind::Person, 1);
	const PersistentId B = PersistentId::Make(IdKind::Person, 2);
	const PersistentId B2 = PersistentId::Make(IdKind::Person, 2);
	const PersistentId Big = PersistentId::Make(IdKind::Person, PersistentId::MaxSerial);

	// Same kind: ordered by serial.
	VT_CHECK(A < B);
	VT_CHECK(B > A);
	VT_CHECK(A <= B);
	VT_CHECK(B >= A);
	VT_CHECK(A != B);
	VT_CHECK(!(A == B));
	VT_CHECK(B < Big);
	VT_CHECK(A < Big);
	VT_CHECK((A <=> B) == std::strong_ordering::less);
	VT_CHECK((Big <=> A) == std::strong_ordering::greater);

	// Equality is value equality.
	VT_CHECK(B == B2);
	VT_CHECK(!(B != B2));
	VT_CHECK(B <= B2);
	VT_CHECK(B >= B2);
	VT_CHECK(!(B < B2));
	VT_CHECK(!(B > B2));
	VT_CHECK((B <=> B2) == std::strong_ordering::equal);

	// Different kinds: kind is the major key (it occupies the top bits).
	const PersistentId LastEntity = PersistentId::Make(IdKind::Entity, PersistentId::MaxSerial);
	const PersistentId FirstPerson = A;
	VT_CHECK(LastEntity < FirstPerson);
	VT_CHECK(LastEntity != FirstPerson);
	VT_CHECK(PersistentId::Invalid() < LastEntity);
	VT_CHECK(PersistentId::Make(IdKind::Map, 1) < PersistentId::Make(IdKind::MaxValue, 1));

	// Copies are independent values.
	PersistentId Copy = A;
	VT_CHECK(Copy == A);
	Copy = B;
	VT_CHECK(Copy == B);
	VT_CHECK(Copy != A);
	VT_CHECK(A == PersistentId::Make(IdKind::Person, 1));

	// Sorting orders by serial within a kind.
	std::vector<PersistentId> Ids;
	for (uint64 Serial = 50; Serial >= 1; --Serial)
	{
		Ids.push_back(PersistentId::Make(IdKind::Family, Serial));
	}
	std::sort(Ids.begin(), Ids.end());
	VT_REQUIRE_EQ(Ids.size(), usize{50});
	for (usize i = 0; i < Ids.size(); ++i)
	{
		VT_CHECK_EQ(Ids[i].Serial(), static_cast<uint64>(i + 1));
		VT_CHECK_EQ(Ids[i].Kind(), IdKind::Family);
	}
}

VAELEN_TEST(Ids, HashAndUnorderedMapKey)
{
	// std::hash<PersistentId> is consistent with PersistentId::Hash and pure.
	const PersistentId Sample = PersistentId::Make(IdKind::Person, 12345);
	VT_CHECK_EQ(std::hash<PersistentId>{}(Sample), static_cast<std::size_t>(Sample.Hash()));
	VT_CHECK_EQ(Sample.Hash(), HashUInt64(Sample.Value));
	VT_CHECK_EQ(Sample.Hash(), PersistentId::Make(IdKind::Person, 12345).Hash());
	VT_CHECK_EQ(PersistentId().Hash(), PersistentId::Invalid().Hash());
	VT_CHECK_NE(Sample.Hash(), Sample.Value); // it is mixed, not the identity

	// 10k distinct ids across every allocatable kind: same serials with
	// different kinds and same kinds with different serials.
	constexpr usize SampleCount = 10000;
	std::vector<PersistentId> Ids;
	Ids.reserve(SampleCount);
	for (usize i = 0; i < SampleCount; ++i)
	{
		const IdKind Kind = AllocatableKinds[i % AllocatableKindCount];
		const uint64 Serial = 1 + static_cast<uint64>(i / AllocatableKindCount);
		Ids.push_back(PersistentId::Make(Kind, Serial));
	}

	std::unordered_set<uint64> DistinctValues;
	std::unordered_set<std::size_t> DistinctHashes;
	std::unordered_map<PersistentId, uint32> Map;
	for (usize i = 0; i < Ids.size(); ++i)
	{
		DistinctValues.insert(Ids[i].Value);
		DistinctHashes.insert(std::hash<PersistentId>{}(Ids[i]));
		const bool Inserted = Map.emplace(Ids[i], static_cast<uint32>(i)).second;
		VT_CHECK(Inserted);
	}
	VT_CHECK_EQ(DistinctValues.size(), SampleCount); // the sample itself is collision-free
	VT_CHECK_EQ(DistinctHashes.size(), SampleCount); // zero hash collisions over the sample
	VT_CHECK_EQ(Map.size(), SampleCount);

	// Every id finds its own slot and nothing else.
	usize Mismatches = 0;
	for (usize i = 0; i < Ids.size(); ++i)
	{
		const auto Found = Map.find(Ids[i]);
		if (Found == Map.end() || Found->second != static_cast<uint32>(i))
		{
			++Mismatches;
		}
	}
	VT_CHECK_EQ(Mismatches, usize{0});
	VT_CHECK(Map.find(PersistentId::Invalid()) == Map.end());
	VT_CHECK(Map.find(PersistentId::Make(IdKind::Entity, 999999)) == Map.end());

	// Keys behave as values: an equal id built independently hits the same slot.
	Map[PersistentId::Make(IdKind::Entity, 1)] = 4242;
	VT_CHECK_EQ(Map.size(), SampleCount);
	VT_CHECK_EQ(Map.at(Ids[0]), uint32{4242});
	VT_CHECK_EQ(Map.erase(PersistentId::Make(IdKind::Entity, 1)), usize{1});
	VT_CHECK(Map.find(Ids[0]) == Map.end());
	VT_CHECK_EQ(Map.size(), SampleCount - 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// IdKind
// ═════════════════════════════════════════════════════════════════════════════

VAELEN_TEST(Ids, KindNamesAreKnownAndUnique)
{
	for (usize i = 0; i < AllKindCount; ++i)
	{
		const unsigned KindValue = static_cast<unsigned>(ToUnderlying(AllKinds[i]));
		const char* Name = IdKindToString(AllKinds[i]);
		VT_REQUIRE(Name != nullptr);
		VT_CHECK_MSG(Name[0] != '\0', "kind %u has an empty name", KindValue);
		VT_CHECK_MSG(std::strcmp(Name, "Unknown") != 0, "kind %u has no name", KindValue);
		for (usize j = 0; j < i; ++j)
		{
			VT_CHECK_MSG(std::strcmp(Name, IdKindToString(AllKinds[j])) != 0, "kinds %u and %u share the name \"%s\"",
						 KindValue, static_cast<unsigned>(ToUnderlying(AllKinds[j])), Name);
		}
	}

	// Names are stable identifiers used by logs and tooling.
	VT_CHECK_STREQ(IdKindToString(IdKind::None), "None");
	VT_CHECK_STREQ(IdKindToString(IdKind::Entity), "Entity");
	VT_CHECK_STREQ(IdKindToString(IdKind::Person), "Person");
	VT_CHECK_STREQ(IdKindToString(IdKind::ResourceDeposit), "ResourceDeposit");
	VT_CHECK_STREQ(IdKindToString(IdKind::Map), "Map");
	VT_CHECK_STREQ(IdKindToString(IdKind::MaxValue), "MaxValue");

	// Gaps reserved for future kinds are reported as Unknown, never as garbage.
	VT_CHECK_STREQ(IdKindToString(static_cast<IdKind>(3)), "Unknown");
	VT_CHECK_STREQ(IdKindToString(static_cast<IdKind>(9)), "Unknown");
	VT_CHECK_STREQ(IdKindToString(static_cast<IdKind>(60)), "Unknown");
	VT_CHECK_STREQ(IdKindToString(static_cast<IdKind>(254)), "Unknown");

	// Enumerator values are part of the save format: pin them.
	VT_CHECK_EQ(ToUnderlying(IdKind::None), uint8{0});
	VT_CHECK_EQ(ToUnderlying(IdKind::Entity), uint8{1});
	VT_CHECK_EQ(ToUnderlying(IdKind::Event), uint8{2});
	VT_CHECK_EQ(ToUnderlying(IdKind::Region), uint8{10});
	VT_CHECK_EQ(ToUnderlying(IdKind::Culture), uint8{20});
	VT_CHECK_EQ(ToUnderlying(IdKind::Person), uint8{23});
	VT_CHECK_EQ(ToUnderlying(IdKind::Item), uint8{30});
	VT_CHECK_EQ(ToUnderlying(IdKind::Polity), uint8{40});
	VT_CHECK_EQ(ToUnderlying(IdKind::Document), uint8{50});
	VT_CHECK_EQ(ToUnderlying(IdKind::Map), uint8{51});
	VT_CHECK_EQ(ToUnderlying(IdKind::MaxValue), uint8{255});
	static_assert(std::is_same_v<std::underlying_type_t<IdKind>, uint8>, "IdKind must fit the 8 kind bits");
}

// ═════════════════════════════════════════════════════════════════════════════
// IdAllocator
// ═════════════════════════════════════════════════════════════════════════════

VAELEN_TEST(Ids, AllocatorFirstSerialIsOne)
{
	const IdAllocator Fresh;
	for (uint64 Next : Fresh.GetState().NextSerial)
	{
		VT_CHECK_EQ(Next, uint64{1}); // every slot, including the unused None slot
	}

	IdAllocator Allocator;
	for (usize i = 0; i < AllocatableKindCount; ++i)
	{
		const IdKind Kind = AllocatableKinds[i];
		VT_CHECK_EQ(Allocator.GetAllocatedCount(Kind), uint64{0});
		const PersistentId Peeked = Allocator.PeekNext(Kind);
		VT_CHECK_EQ(Peeked.Kind(), Kind);
		VT_CHECK_EQ(Peeked.Serial(), uint64{1});

		const PersistentId Id = Allocator.Allocate(Kind);
		VT_CHECK(Id.IsValid());
		VT_CHECK_EQ(Id.Kind(), Kind);
		VT_CHECK_EQ(Id.Serial(), uint64{1});
		VT_CHECK_EQ(Id.Value, Peeked.Value);
		VT_CHECK_EQ(Allocator.GetAllocatedCount(Kind), uint64{1});
	}
}

VAELEN_TEST(Ids, AllocatorSequentialSerials)
{
	IdAllocator Allocator;
	PersistentId Previous;
	for (uint64 i = 1; i <= 1000; ++i)
	{
		const PersistentId Id = Allocator.Allocate(IdKind::Person);
		VT_CHECK_EQ(Id.Serial(), i);
		VT_CHECK_EQ(Id.Kind(), IdKind::Person);
		VT_CHECK(Previous < Id);
		VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Person), i);
		Previous = Id;
	}
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Person).Serial(), uint64{1001});
	VT_CHECK_EQ(Allocator.GetState().NextSerial[ToUnderlying(IdKind::Person)], uint64{1001});
}

VAELEN_TEST(Ids, AllocatorKindsAreIndependent)
{
	IdAllocator Allocator;
	for (int i = 0; i < 5; ++i)
	{
		(void)Allocator.Allocate(IdKind::Person);
	}
	for (int i = 0; i < 3; ++i)
	{
		(void)Allocator.Allocate(IdKind::Family);
	}
	(void)Allocator.Allocate(IdKind::MaxValue); // last slot of the counter table

	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Person), uint64{5});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Family), uint64{3});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::MaxValue), uint64{1});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Item), uint64{0});
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Person).Serial(), uint64{6});
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Family).Serial(), uint64{4});
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{1});

	// Interleaving kinds does not disturb any per-kind sequence.
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Item).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Person).Serial(), uint64{6});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Family).Serial(), uint64{4});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Item).Serial(), uint64{2});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Person).Serial(), uint64{7});

	// Everything else is untouched.
	for (usize i = 0; i < AllocatableKindCount; ++i)
	{
		const IdKind Kind = AllocatableKinds[i];
		if (Kind == IdKind::Person || Kind == IdKind::Family || Kind == IdKind::Item || Kind == IdKind::MaxValue)
		{
			continue;
		}
		VT_CHECK_EQ(Allocator.GetAllocatedCount(Kind), uint64{0});
		VT_CHECK_EQ(Allocator.PeekNext(Kind).Serial(), uint64{1});
	}
}

VAELEN_TEST(Ids, AllocatorPeekDoesNotConsume)
{
	IdAllocator Allocator;
	(void)Allocator.Allocate(IdKind::Settlement);
	const IdAllocator::State Before = Allocator.GetState();

	const PersistentId First = Allocator.PeekNext(IdKind::Settlement);
	const PersistentId Second = Allocator.PeekNext(IdKind::Settlement);
	VT_CHECK_EQ(First.Value, Second.Value);
	VT_CHECK_EQ(First.Kind(), IdKind::Settlement);
	VT_CHECK_EQ(First.Serial(), uint64{2});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Settlement), uint64{1});
	VT_CHECK(Allocator.GetState().NextSerial == Before.NextSerial);

	const PersistentId Allocated = Allocator.Allocate(IdKind::Settlement);
	VT_CHECK_EQ(Allocated.Value, First.Value);
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Settlement).Serial(), uint64{3});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Settlement), uint64{2});

	// Peeking a kind that was never allocated creates no state either.
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Map).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Map).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Map), uint64{0});
}

VAELEN_TEST(Ids, AllocatorReserveUpTo)
{
	IdAllocator Allocator;

	// Serial 0 is never allocated: reserving it on a fresh kind changes nothing.
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Item, 0));
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Item), uint64{0});

	// Advancing: true, and the next allocation is Serial + 1.
	VT_CHECK(Allocator.ReserveUpTo(IdKind::Item, 100));
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{101});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Item), uint64{100});

	// At or below the high-water mark: false, no change.
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Item, 100));
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Item, 50));
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Item, 1));
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Item, 0));
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{101});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Item), uint64{100});

	// Reserving exactly the next serial advances by one.
	VT_CHECK(Allocator.ReserveUpTo(IdKind::Item, 101));
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{102});
	const PersistentId Id = Allocator.Allocate(IdKind::Item);
	VT_CHECK_EQ(Id.Kind(), IdKind::Item);
	VT_CHECK_EQ(Id.Serial(), uint64{102});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Item), uint64{102});

	// The id just handed out is already used: reserving it is a no-op.
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Item, 102));
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{103});

	// Other kinds are unaffected.
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Building).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Building), uint64{0});

	// Importing a batch of ids in any order: the highest one wins.
	VT_CHECK(Allocator.ReserveUpTo(IdKind::Building, 7));
	VT_CHECK(Allocator.ReserveUpTo(IdKind::Building, 9));
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Building, 8));
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Building, 9));
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Building).Serial(), uint64{10});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Building), uint64{10});

	// The reservation is visible in the serialisable state.
	VT_CHECK_EQ(Allocator.GetState().NextSerial[ToUnderlying(IdKind::Item)], uint64{103});
	VT_CHECK_EQ(Allocator.GetState().NextSerial[ToUnderlying(IdKind::Building)], uint64{11});
}

VAELEN_TEST(Ids, AllocatorReset)
{
	const IdAllocator Fresh;
	IdAllocator Allocator;
	for (usize i = 0; i < 300; ++i)
	{
		(void)Allocator.Allocate(AllocatableKinds[i % AllocatableKindCount]);
	}
	VT_CHECK(Allocator.ReserveUpTo(IdKind::Route, 12345));
	VT_CHECK(Allocator.GetState().NextSerial != Fresh.GetState().NextSerial);

	Allocator.Reset();
	VT_CHECK(Allocator.GetState().NextSerial == Fresh.GetState().NextSerial);
	for (usize i = 0; i < AllocatableKindCount; ++i)
	{
		VT_CHECK_EQ(Allocator.GetAllocatedCount(AllocatableKinds[i]), uint64{0});
		VT_CHECK_EQ(Allocator.PeekNext(AllocatableKinds[i]).Serial(), uint64{1});
	}
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Route), uint64{0});

	// After Reset the allocator restarts the same sequence from scratch.
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Person).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Route).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Person).Serial(), uint64{2});

	// Reset is idempotent.
	Allocator.Reset();
	Allocator.Reset();
	VT_CHECK(Allocator.GetState().NextSerial == Fresh.GetState().NextSerial);
}

VAELEN_TEST(Ids, AllocatorStateRoundTrip)
{
	IdAllocator Original;
	for (usize i = 0; i < 777; ++i)
	{
		(void)Original.Allocate(AllocatableKinds[(i * 11) % AllocatableKindCount]);
	}
	VT_CHECK(Original.ReserveUpTo(IdKind::War, 5000));

	// Snapshot, and record the id every kind would hand out next.
	const IdAllocator::State Snapshot = Original.GetState();
	std::array<PersistentId, IdAllocator::KindCount> ExpectedNext;
	for (usize k = 0; k < IdAllocator::KindCount; ++k)
	{
		ExpectedNext[k] = Original.PeekNext(static_cast<IdKind>(k));
	}
	VT_CHECK_EQ(ExpectedNext[ToUnderlying(IdKind::War)].Serial(), uint64{5001});

	// The original keeps going: the snapshot must be a copy, not an alias.
	(void)Original.Allocate(IdKind::Person);
	(void)Original.Allocate(IdKind::War);
	VT_CHECK(Original.GetState().NextSerial != Snapshot.NextSerial);
	VT_CHECK_EQ(Snapshot.NextSerial[ToUnderlying(IdKind::War)], uint64{5001});

	// Restore into a fresh allocator: exact same next ids for all 256 kinds.
	IdAllocator Restored;
	Restored.SetState(Snapshot);
	VT_CHECK(Restored.GetState().NextSerial == Snapshot.NextSerial);
	for (usize k = 0; k < IdAllocator::KindCount; ++k)
	{
		const IdKind Kind = static_cast<IdKind>(k);
		VT_CHECK_EQ(Restored.PeekNext(Kind).Value, ExpectedNext[k].Value);
		VT_CHECK_EQ(Restored.GetAllocatedCount(Kind), Snapshot.NextSerial[k] - 1);
	}
	for (usize i = 0; i < AllocatableKindCount; ++i)
	{
		const IdKind Kind = AllocatableKinds[i];
		VT_CHECK_EQ(Restored.Allocate(Kind).Value, ExpectedNext[ToUnderlying(Kind)].Value);
	}

	// Restoring the snapshot into the original allocator rewinds it as well.
	Original.SetState(Snapshot);
	for (usize i = 0; i < AllocatableKindCount; ++i)
	{
		const IdKind Kind = AllocatableKinds[i];
		VT_CHECK_EQ(Original.Allocate(Kind).Value, ExpectedNext[ToUnderlying(Kind)].Value);
	}
	VT_CHECK(Original.GetState().NextSerial == Restored.GetState().NextSerial);

	// A save/load cycle of the state bytes is the identity.
	IdAllocator::State Copy = Snapshot;
	IdAllocator Loaded;
	Loaded.SetState(Copy);
	VT_CHECK(Loaded.GetState().NextSerial == Snapshot.NextSerial);
}

VAELEN_TEST(Ids, AllocatorSetStateSanitisesZeroCounters)
{
	VaelenTest::ScopedAssertCapture Capture;

	IdAllocator::State Zeroed{}; // e.g. a zero-filled legacy save
	for (uint64 Next : Zeroed.NextSerial)
	{
		VT_CHECK_EQ(Next, uint64{0});
	}

	IdAllocator Allocator;
	(void)Allocator.Allocate(IdKind::Person);
	Allocator.SetState(Zeroed);
	for (uint64 Next : Allocator.GetState().NextSerial)
	{
		VT_CHECK_EQ(Next, uint64{1});
	}
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Person), uint64{0});
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Person).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Person).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Law).Serial(), uint64{1});

	// Mixed state: zero slots become 1, non-zero slots are kept verbatim.
	IdAllocator::State Mixed{};
	Mixed.NextSerial[ToUnderlying(IdKind::Person)] = 42;
	Mixed.NextSerial[ToUnderlying(IdKind::Family)] = PersistentId::MaxSerial;
	Allocator.SetState(Mixed);
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Person).Serial(), uint64{42});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Person), uint64{41});
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Family).Serial(), PersistentId::MaxSerial);
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Item), uint64{0});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Person).Serial(), uint64{42});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Item).Serial(), uint64{1});

	// The sanitised counters are what GetState reports, so a re-save is clean.
	VT_CHECK_EQ(Allocator.GetState().NextSerial[0], uint64{1});
	VT_CHECK_EQ(Allocator.GetState().NextSerial[ToUnderlying(IdKind::Item)], uint64{2});
	VT_CHECK_EQ(Allocator.GetState().NextSerial[ToUnderlying(IdKind::Person)], uint64{43});
	VT_CHECK_EQ(Allocator.GetState().NextSerial[ToUnderlying(IdKind::Map)], uint64{1});

	// Sanitising is silent: no assertion is raised for zero counters.
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
}

VAELEN_TEST(Ids, AllocatorDeterminism)
{
	IdAllocator A;
	IdAllocator B;
	constexpr usize Steps = 5000;
	std::vector<PersistentId> Recorded;
	Recorded.reserve(Steps);
	usize Mismatches = 0;
	for (usize i = 0; i < Steps; ++i)
	{
		const IdKind Kind = AllocatableKinds[(i * 7 + (i >> 3)) % AllocatableKindCount];
		const PersistentId IdA = A.Allocate(Kind);
		const PersistentId IdB = B.Allocate(Kind);
		if (IdA != IdB)
		{
			++Mismatches;
		}
		Recorded.push_back(IdA);
		if (i % 97 == 0)
		{
			const bool ReservedA = A.ReserveUpTo(Kind, static_cast<uint64>(i) + 3);
			const bool ReservedB = B.ReserveUpTo(Kind, static_cast<uint64>(i) + 3);
			VT_CHECK_EQ(ReservedA, ReservedB);
		}
	}
	VT_CHECK_EQ(Mismatches, usize{0});
	VT_CHECK(A.GetState().NextSerial == B.GetState().NextSerial);
	for (usize i = 0; i < AllocatableKindCount; ++i)
	{
		VT_CHECK_EQ(A.PeekNext(AllocatableKinds[i]).Value, B.PeekNext(AllocatableKinds[i]).Value);
		VT_CHECK_EQ(A.GetAllocatedCount(AllocatableKinds[i]), B.GetAllocatedCount(AllocatableKinds[i]));
	}

	// Every recorded id is valid and unique: ids are never reused.
	std::unordered_set<PersistentId> Seen;
	for (const PersistentId& Id : Recorded)
	{
		if (!Id.IsValid() || !Seen.insert(Id).second)
		{
			++Mismatches;
		}
	}
	VT_CHECK_EQ(Mismatches, usize{0});
	VT_CHECK_EQ(Seen.size(), Steps);

	// A third allocator replaying the same kinds from a fresh state matches
	// the recording exactly (allocation depends on nothing but the state).
	IdAllocator C;
	for (usize i = 0; i < Steps; ++i)
	{
		const IdKind Kind = AllocatableKinds[(i * 7 + (i >> 3)) % AllocatableKindCount];
		if (C.Allocate(Kind) != Recorded[i])
		{
			++Mismatches;
		}
		if (i % 97 == 0)
		{
			(void)C.ReserveUpTo(Kind, static_cast<uint64>(i) + 3);
		}
	}
	VT_CHECK_EQ(Mismatches, usize{0});
	VT_CHECK(C.GetState().NextSerial == A.GetState().NextSerial);
}

// ═════════════════════════════════════════════════════════════════════════════
// Assertion paths (the handler returns instead of aborting). The Check counts
// exist only when assertions are compiled in; the behaviour that follows a
// failed precondition (Invalid() returned, counters untouched) is unconditional
// and is exactly what a build without assertions relies on.
// ═════════════════════════════════════════════════════════════════════════════
#if VAELEN_ASSERTS_ENABLED
#	define VT_CHECK_ASSERTS(Expr) VT_CHECK(Expr)
#	define VT_CHECK_EQ_ASSERTS(Actual, Expected) VT_CHECK_EQ(Actual, Expected)
#	define VT_CHECK_STREQ_ASSERTS(Actual, Expected) VT_CHECK_STREQ(Actual, Expected)
#else
#	define VT_CHECK_ASSERTS(Expr) ((void)0)
#	define VT_CHECK_EQ_ASSERTS(Actual, Expected) ((void)0)
#	define VT_CHECK_STREQ_ASSERTS(Actual, Expected) ((void)0)
#endif

VAELEN_TEST(Ids, AllocateNoneTriggersCheck)
{
	IdAllocator Allocator;
	(void)Allocator.Allocate(IdKind::Person);
	const IdAllocator::State Before = Allocator.GetState();

	PersistentId Id;
	{
		VaelenTest::ScopedAssertCapture Capture;
		Id = Allocator.Allocate(IdKind::None);
		VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 1);
		VT_CHECK_EQ(Capture.EnsureCount, 0);
		VT_CHECK_STREQ_ASSERTS(Capture.LastExpression, "Kind != IdKind::None");
		VT_CHECK_ASSERTS(std::strstr(Capture.LastMessage, "None") != nullptr);
	}

	// With a returning handler the call yields PersistentId::Invalid() and
	// leaves the allocator untouched (the None slot is never consumed).
	VT_CHECK(!Id.IsValid());
	VT_CHECK_EQ(Id.Kind(), IdKind::None);
	VT_CHECK_EQ(Id.Value, PersistentId::Invalid().Value);
	VT_CHECK(Allocator.GetState().NextSerial == Before.NextSerial);
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::None), uint64{0});
	VT_CHECK_EQ(Allocator.GetState().NextSerial[0], uint64{1});

	// The allocator is still usable for real kinds afterwards.
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Person).Serial(), uint64{2});
}

VAELEN_TEST(Ids, AllocatorExhaustionTriggersCheck)
{
	IdAllocator Allocator;
	VaelenTest::ScopedAssertCapture Capture;

	// Reserve everything but the last serial: MaxSerial itself is still free.
	VT_CHECK(Allocator.ReserveUpTo(IdKind::Army, PersistentId::MaxSerial - 1));
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 0);
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Army).Serial(), PersistentId::MaxSerial);
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Army), PersistentId::MaxSerial - 1);

	// The last serial is a normal, valid allocation.
	const PersistentId Last = Allocator.Allocate(IdKind::Army);
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 0);
	VT_CHECK(Last.IsValid());
	VT_CHECK_EQ(Last.Kind(), IdKind::Army);
	VT_CHECK_EQ(Last.Serial(), PersistentId::MaxSerial);
	VT_CHECK_EQ(Last.Value, KindBitsOf(IdKind::Army) | PersistentId::MaxSerial);
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Army), PersistentId::MaxSerial);

	// The serial space is now exhausted: allocating again is a Check failure.
	// With a returning handler the allocator hands back PersistentId::Invalid()
	// and leaves its counter alone, so serials never wrap around to 1.
	const IdAllocator::State Exhausted = Allocator.GetState();
	const PersistentId Overflow = Allocator.Allocate(IdKind::Army);
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 1);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_STREQ_ASSERTS(Capture.LastExpression, "Next <= PersistentId::MaxSerial");
	VT_CHECK_ASSERTS(std::strstr(Capture.LastMessage, "Army") != nullptr);
	VT_CHECK(!Overflow.IsValid());
	VT_CHECK_EQ(Overflow.Value, PersistentId::Invalid().Value);
	VT_CHECK(Allocator.GetState().NextSerial == Exhausted.NextSerial);
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Army), PersistentId::MaxSerial);

	const PersistentId Overflow2 = Allocator.Allocate(IdKind::Army);
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 2);
	VT_CHECK(!Overflow2.IsValid());
	VT_CHECK_NE(Overflow2.Serial(), uint64{1}); // no wrap-around, no reuse
	VT_CHECK_EQ(Overflow2.Value, PersistentId::Invalid().Value);
	VT_CHECK(Allocator.GetState().NextSerial == Exhausted.NextSerial);

	// Other kinds are unaffected by one kind's exhaustion.
	VT_CHECK_EQ(Allocator.Allocate(IdKind::War).Serial(), uint64{1});
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 2);

	// Reserving MaxSerial itself marks the whole space as used: the very next
	// allocation already fails, and PeekNext shows an invalid id.
	VT_CHECK(Allocator.ReserveUpTo(IdKind::Polity, PersistentId::MaxSerial));
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 2);
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Polity), PersistentId::MaxSerial);
	VT_CHECK(!Allocator.PeekNext(IdKind::Polity).IsValid());
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Polity, PersistentId::MaxSerial));
	const PersistentId Overflow3 = Allocator.Allocate(IdKind::Polity);
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 3);
	VT_CHECK(!Overflow3.IsValid());
	VT_CHECK_EQ(Overflow3.Value, PersistentId::Invalid().Value);

	// Reserving past MaxSerial is a Check failure; the counter is left alone
	// and false is returned (nothing changed).
	const IdAllocator::State BeforeBad = Allocator.GetState();
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Law, PersistentId::MaxSerial + 1));
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 4);
	VT_CHECK_STREQ_ASSERTS(Capture.LastExpression, "Serial <= PersistentId::MaxSerial");
	VT_CHECK(Allocator.GetState().NextSerial == BeforeBad.NextSerial);
	VT_CHECK(!Allocator.ReserveUpTo(IdKind::Law, ~uint64{0}));
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 5);
	VT_CHECK(Allocator.GetState().NextSerial == BeforeBad.NextSerial);
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Law).Serial(), uint64{1});
	VT_CHECK_EQ(Allocator.Allocate(IdKind::Law).Serial(), uint64{1});
	VT_CHECK_EQ_ASSERTS(Capture.CheckCount, 5);
}

VAELEN_TEST(Ids, AllocatorSetStateClampsCountersBeyondSerialSpace)
{
	// A counter beyond the 56-bit serial space can only come from a corrupt
	// save: it is clamped to the exhausted sentinel and reported, so that
	// PeekNext can never mask it into a valid-looking, already-used id.
	VaelenTest::ScopedAssertCapture Capture;
	IdAllocator::State Bad{};
	Bad.NextSerial[ToUnderlying(IdKind::Person)] = PersistentId::MaxSerial + 6;
	Bad.NextSerial[ToUnderlying(IdKind::Family)] = PersistentId::MaxSerial + 1; // exhausted, legal
	Bad.NextSerial[ToUnderlying(IdKind::Item)] = 12;
	IdAllocator Allocator;
	Allocator.SetState(Bad);
	VT_CHECK_EQ_ASSERTS(Capture.EnsureCount, 1);

	VT_CHECK_EQ(Allocator.GetState().NextSerial[ToUnderlying(IdKind::Person)], PersistentId::MaxSerial + 1);
	VT_CHECK(!Allocator.PeekNext(IdKind::Person).IsValid());
	VT_CHECK(!Allocator.PeekNext(IdKind::Family).IsValid());
	VT_CHECK(Allocator.PeekNext(IdKind::Person) == Allocator.Allocate(IdKind::Person));
	VT_CHECK_EQ(Allocator.GetAllocatedCount(IdKind::Person), PersistentId::MaxSerial);
	VT_CHECK_EQ(Allocator.PeekNext(IdKind::Item).Serial(), uint64{12});
	VT_CHECK(!Allocator.PeekNext(IdKind::None).IsValid());
}

VAELEN_TEST(Ids, GetTypeHashIsUsableForUnrealContainers)
{
	// Found by ADL, constexpr, derived from Hash(), differs between ids.
	static_assert(GetTypeHash(PersistentId::Make(IdKind::Person, 1)) !=
				  GetTypeHash(PersistentId::Make(IdKind::Person, 2)));
	const PersistentId Id = PersistentId::Make(IdKind::Settlement, 77);
	const uint64 H = Id.Hash();
	VT_CHECK_EQ(GetTypeHash(Id), static_cast<uint32>(H ^ (H >> 32)));
	std::unordered_set<uint32> Seen;
	for (uint64 i = 1; i <= 10000; ++i)
	{
		Seen.insert(GetTypeHash(PersistentId::Make(IdKind::Item, i)));
	}
	VT_CHECK(Seen.size() > 9990); // 32-bit hash: a handful of collisions is possible
}
