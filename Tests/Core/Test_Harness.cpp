// VAELEN - harness self-test: check macros, assert capture, nesting.
//
// STATUS: VALIDATED (Phase 00)
#include "VaelenTest.h"

#include <cstring>
#include <limits>
#include <type_traits>

VAELEN_TEST(Harness, ChecksPass)
{
	VT_CHECK(true);
	VT_CHECK_EQ(2 + 2, 4);
	VT_CHECK_NE(1, 2);
	VT_CHECK_STREQ("vaelen", "vaelen");
	VT_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-12);
	VT_REQUIRE(Ctx.Failures == 0);
}

namespace
{
	/// Every failing form of every macro, run against a silent scratch context.
	void RunNegativeChecks(::VaelenTest::Context& Ctx)
	{
		VT_CHECK(false);
		VT_CHECK_MSG(false, "detail %d", 1);
		VT_CHECK_EQ(1, 2);
		VT_CHECK_EQ(int{-1}, ~Vaelen::uint64{0}); // mixed signedness must NOT compare equal
		VT_CHECK_NE(3, 3);
		VT_CHECK_STREQ("a", "b");
		VT_CHECK_STREQ(nullptr, "");
		VT_CHECK_NEAR(std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0); // NaN is never near
		VT_REQUIRE_EQ(1, 2);
		VT_CHECK(true); // must not be reached: VT_REQUIRE_EQ returned
	}

	void RunRequireStops(::VaelenTest::Context& Ctx)
	{
		VT_REQUIRE(false);
		VT_CHECK(false); // must not be reached
	}
} // namespace

VAELEN_TEST(Harness, NegativeChecksAreRecorded)
{
	::VaelenTest::Context Scratch;
	Scratch.Silent = true;
	RunNegativeChecks(Scratch);
	VT_CHECK_EQ(Scratch.Failures, 9);
	VT_CHECK_EQ(Scratch.Checks, 9);

	::VaelenTest::Context Stops;
	Stops.Silent = true;
	RunRequireStops(Stops);
	VT_CHECK_EQ(Stops.Failures, 1);
	VT_CHECK_EQ(Stops.Checks, 1);

	// Mixed signedness compares mathematically, not after promotion.
	VT_CHECK_NE(int{-1}, ~Vaelen::uint64{0});
	VT_CHECK_EQ(Vaelen::uint64{5}, int{5});
	VT_CHECK_EQ(Vaelen::int64{-5}, int{-5});
}

VAELEN_TEST(Harness, EnsureYieldsItsBooleanInEveryBuild)
{
	VaelenTest::ScopedAssertCapture Capture;
	VT_CHECK(!VAELEN_ENSURE(1 == 2));
	VT_CHECK(VAELEN_ENSURE(2 == 2));
}

#if VAELEN_ASSERTS_ENABLED
VAELEN_TEST(Harness, AssertCaptureDoesNotAbort)
{
	VaelenTest::ScopedAssertCapture Capture;
	const bool Result = VAELEN_ENSURE(1 == 2);
	VT_CHECK(!Result);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "1 == 2");
}

VAELEN_TEST(Harness, NestedCapturesRestoreTheOuterHandler)
{
	VaelenTest::ScopedAssertCapture Outer;
	{
		VaelenTest::ScopedAssertCapture Inner;
		(void)VAELEN_ENSURE(false);
		VT_CHECK_EQ(Inner.EnsureCount, 1);
		VT_CHECK_EQ(Outer.EnsureCount, 0);
	}
	// Inner is gone: failures must reach Outer again instead of the default handler.
	(void)VAELEN_ENSURE(false);
	VT_CHECK_EQ(Outer.EnsureCount, 1);
	void* UserData = nullptr;
	VT_CHECK(Vaelen::GetAssertHandler(&UserData) != nullptr);
	VT_CHECK(UserData == static_cast<void*>(&Outer));
}
#endif

// ── 17.07: a harness that can see a vacuous assertion ────────────────────────
namespace
{
	/// `Player::StartRules`, byte for byte - four uint32 with these defaults -
	/// reconstructed here because the harness self-test sits below VaelenPlayer
	/// and may not include it. What matters to the macro is the shape and the
	/// defaults, and both are pinned by the static_asserts below.
	struct StartRulesAsWritten
	{
		Vaelen::uint32 FromAge = 16;
		Vaelen::uint32 ToAge = 40;
		Vaelen::uint32 WantBound = 1;
		Vaelen::uint32 PreferOre = 1;
	};
	static_assert(sizeof(StartRulesAsWritten) == 16, "four uint32, no padding");
	static_assert(std::has_unique_object_representations_v<StartRulesAsWritten>, "byte equality is value equality");

	/// THE ASSERTION PHASE 16 COMMITTED, reconstructed: two default-constructed
	/// values, "written" and "read", compared with the plain macro.
	void TheVacuousRoundTripUnderPlainCheck(::VaelenTest::Context& Ctx)
	{
		const StartRulesAsWritten Written;
		const StartRulesAsWritten Read; // a reader that wrote nothing leaves this at its defaults
		VT_CHECK(std::memcmp(&Written, &Read, sizeof(Written)) == 0);
	}

	/// The same assertion under the witness macro.
	void TheVacuousRoundTripUnderTheWitness(::VaelenTest::Context& Ctx)
	{
		const StartRulesAsWritten Written;
		const StartRulesAsWritten Read;
		VT_CHECK_ROUNDTRIP(Written, Read);
	}

	/// A GENUINE round trip: something non-default was written and the same
	/// thing came back. Must pass under both macros, or the witness is just a
	/// macro that fails.
	void TheGenuineRoundTrip(::VaelenTest::Context& Ctx)
	{
		StartRulesAsWritten Written;
		Written.FromAge = 0;
		Written.ToAge = 45;
		Written.WantBound = 0;
		Written.PreferOre = 0;
		const StartRulesAsWritten Read = Written; // what a working reader hands back
		VT_CHECK(std::memcmp(&Written, &Read, sizeof(Written)) == 0);
		VT_CHECK_ROUNDTRIP(Written, Read);
	}

	/// A round trip that genuinely LOST something: non-default written, a
	/// field dropped on the way back. Must fail under the witness for the
	/// second reason, not the first.
	void TheLossyRoundTrip(::VaelenTest::Context& Ctx)
	{
		StartRulesAsWritten Written;
		Written.ToAge = 45;
		StartRulesAsWritten Read = Written;
		Read.ToAge = 40; // the reader restored the default instead of the value
		VT_CHECK_ROUNDTRIP(Written, Read);
	}

	void TheDigestChecks(::VaelenTest::Context& Ctx)
	{
		VT_CHECK_DIGEST_EQ(0x609253a29361ec5full, 0x609253a29361ec5full); // passes: real, equal
		VT_CHECK_DIGEST_EQ(0x609253a29361ec5full, 0x18aec14e39a68a8cull); // fails: differ
		VT_CHECK_DIGEST_EQ(0ull, 0ull);									  // fails: unset against unset
		VT_CHECK_DIGEST_EQ(0x609253a29361ec5full, 0ull);				  // fails: one side unset
		VT_CHECK_DIGEST_EQ(0x5641454c454e2d45ull, 0x5641454c454e2d45ull); // fails: an empty log against itself
	}
} // namespace

VAELEN_TEST(Harness, VacuityIsSeen)
{
	// BOTH ARMS IN ONE RUN. The reconstructed default-vs-default assertion
	// PASSES under VT_CHECK - that is the defect, and it stays visible here -
	// and FAILS under VT_CHECK_ROUNDTRIP, in the same test, so that a witness
	// which had stopped witnessing could not pass this file.
	::VaelenTest::Context Plain;
	Plain.Silent = true;
	TheVacuousRoundTripUnderPlainCheck(Plain);
	VT_CHECK_MSG(Plain.Failures == 0,
				 "the plain check no longer passes the vacuous round trip (%d failures); the "
				 "arm that shows the defect has stopped showing it",
				 Plain.Failures);
	VT_CHECK_EQ(Plain.Checks, 1);

	::VaelenTest::Context Witness;
	Witness.Silent = true;
	TheVacuousRoundTripUnderTheWitness(Witness);
	VT_CHECK_MSG(Witness.Failures == 1, "the witness let the vacuous round trip through (%d failures)",
				 Witness.Failures);
	VT_CHECK_EQ(Witness.Checks, 1);

	// THE CONTROL: a genuine round trip passes under both, so the witness is
	// not simply a macro that fails.
	::VaelenTest::Context Genuine;
	Genuine.Silent = true;
	TheGenuineRoundTrip(Genuine);
	VT_CHECK_MSG(Genuine.Failures == 0, "a genuine round trip failed (%d failures)", Genuine.Failures);
	VT_CHECK_EQ(Genuine.Checks, 2);

	// And a round trip that lost a field fails - for the loss, which is the
	// second check in the helper, and is reached only because the write was
	// not vacuous.
	::VaelenTest::Context Lossy;
	Lossy.Silent = true;
	TheLossyRoundTrip(Lossy);
	VT_CHECK_EQ(Lossy.Failures, 1);
}

VAELEN_TEST(Harness, DigestsThatCompareNothingAreRefused)
{
	::VaelenTest::Context Scratch;
	Scratch.Silent = true;
	TheDigestChecks(Scratch);
	VT_CHECK_EQ(Scratch.Checks, 5);
	// One genuine pass, four refusals: two of them would have been GREEN under
	// VT_CHECK_EQ, which is why nine cells of a matrix once compared an empty
	// life against itself.
	VT_CHECK_EQ(Scratch.Failures, 4);
}
