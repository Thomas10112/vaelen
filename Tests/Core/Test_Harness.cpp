// VAELEN - harness self-test: check macros, assert capture, nesting.
//
// STATUS: VALIDATED (Phase 00)
#include "VaelenTest.h"

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
		VT_CHECK_NEAR(0.0 / 0.0, 0.0, 1.0); // NaN is never near
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
