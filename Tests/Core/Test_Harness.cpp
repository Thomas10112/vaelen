// Self-test of the harness itself: registration order, check macros, assert capture.
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

VAELEN_TEST(Harness, AssertCaptureDoesNotAbort)
{
	VaelenTest::ScopedAssertCapture Capture;
	const bool Result = VAELEN_ENSURE(1 == 2);
	VT_CHECK(!Result);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "1 == 2");
}
