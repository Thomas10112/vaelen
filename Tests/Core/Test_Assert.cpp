// VAELEN - VaelenCore tests
// Assertions: VAELEN_CHECK / VAELEN_CHECKF / VAELEN_VERIFY / VAELEN_ENSURE
// reporting (kind, expression text, formatted message, location), single
// evaluation of expressions, the failure counter, handler installation with
// user data, safe truncation of long messages, and statement-context safety
// of every macro. VAELEN_UNREACHABLE() is only compiled on a path that is
// never taken: it aborts even with a custom handler and must not be executed.
//
// Every test that expects a report is guarded by VAELEN_ASSERTS_ENABLED; the
// #else branch checks the compiled-out contracts (CHECK not evaluated, VERIFY
// still evaluated, ENSURE still yields its boolean).
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "Vaelen/Core/Assert.h"

#include <cstring>
#include <type_traits>

using Vaelen::AssertHandler;
using Vaelen::AssertInfo;
using Vaelen::AssertKind;
using Vaelen::GetAssertFailureCount;
using Vaelen::int32;
using Vaelen::SetAssertHandler;
using Vaelen::uint64;
using Vaelen::uint8;
using Vaelen::usize;
using VaelenTest::ScopedAssertCapture;

// ── Compile-time facts ──────────────────────────────────────────────────────

static_assert(std::is_same_v<std::underlying_type_t<AssertKind>, uint8>, "AssertKind must be a uint8 enum");
static_assert(std::is_same_v<AssertHandler, void (*)(const AssertInfo&, void*)>, "handler signature");
static_assert(std::is_trivially_copyable_v<AssertInfo>);
static_assert(std::is_same_v<decltype(GetAssertFailureCount()), uint64>);
static_assert(noexcept(GetAssertFailureCount()));
static_assert(noexcept(SetAssertHandler(nullptr, nullptr)));

namespace
{
	// VAELEN_UNREACHABLE() aborts even with a custom handler installed, so it
	// is only ever placed on a path that cannot execute. This function proves
	// at compile time that the macro terminates control flow (no -Wreturn-type
	// diagnostic for the missing return) and is a single statement.
	int32 SignOf(int32 Value)
	{
		if (Value >= 0)
		{
			return 1;
		}
		if (Value < 0)
		{
			return -1;
		}
		VAELEN_UNREACHABLE();
	}

	int32 CountElseBranches(bool Condition)
	{
		int32 ElseCount = 0;
		if (Condition)
			VAELEN_CHECK(true);
		else
			++ElseCount;

		if (Condition)
			VAELEN_CHECKF(true, "never shown %d", 0);
		else
			++ElseCount;

		if (Condition)
			VAELEN_VERIFY(true);
		else
			++ElseCount;

		if (Condition)
			VAELEN_ENSURE(true);
		else
			++ElseCount;

		// ElseCount is never negative: the unreachable branch is NEVER taken
		// (it would abort the process), the else branch always is.
		if (ElseCount < 0)
			VAELEN_UNREACHABLE();
		else
			++ElseCount;

		return ElseCount;
	}

	// Helpers used by the tests that expect reports (in every build:
	// Detail::ReportAssert is compiled in even when the macros are not).

	// Bounded copy that never emits format diagnostics; always NUL-terminates.
	void CopyString(char* Dst, usize DstSize, const char* Src) noexcept
	{
		usize i = 0;
		if (Src != nullptr)
		{
			for (; i + 1 < DstSize && Src[i] != '\0'; ++i)
			{
				Dst[i] = Src[i];
			}
		}
		Dst[i] = '\0';
	}

	/// Records every field of the last AssertInfo. Fields are COPIED: the
	/// kernel's formatted message lives in a stack buffer that is only valid
	/// for the duration of the handler call. Installs itself with `this` as
	/// user data and restores the default handler on destruction.
	struct RecordingHandler
	{
		RecordingHandler() noexcept { SetAssertHandler(&OnAssert, this); }
		~RecordingHandler() noexcept { SetAssertHandler(nullptr); }
		RecordingHandler(const RecordingHandler&) = delete;
		RecordingHandler& operator=(const RecordingHandler&) = delete;

		int32 CallCount = 0;
		int32 CheckCount = 0;
		int32 EnsureCount = 0;
		void* LastUserData = nullptr;
		AssertKind LastKind = AssertKind::Ensure;
		int32 LastLine = -1;
		bool LastExpressionWasNull = false;
		bool LastFileWasNull = false;
		bool LastFunctionWasNull = false;
		bool LastMessageWasNull = false;
		usize LastMessageLength = 0;
		char LastExpression[256] = {};
		char LastFile[1024] = {};
		char LastFunction[1024] = {};
		char LastMessage[4096] = {};

		static void OnAssert(const AssertInfo& Info, void* UserData)
		{
			auto* Self = static_cast<RecordingHandler*>(UserData);
			++Self->CallCount;
			if (Info.Kind == AssertKind::Check)
			{
				++Self->CheckCount;
			}
			else
			{
				++Self->EnsureCount;
			}
			Self->LastUserData = UserData;
			Self->LastKind = Info.Kind;
			Self->LastLine = Info.Line;
			Self->LastExpressionWasNull = Info.Expression == nullptr;
			Self->LastFileWasNull = Info.File == nullptr;
			Self->LastFunctionWasNull = Info.Function == nullptr;
			Self->LastMessageWasNull = Info.Message == nullptr;
			Self->LastMessageLength = Info.Message != nullptr ? std::strlen(Info.Message) : 0;
			CopyString(Self->LastExpression, sizeof(Self->LastExpression), Info.Expression);
			CopyString(Self->LastFile, sizeof(Self->LastFile), Info.File);
			CopyString(Self->LastFunction, sizeof(Self->LastFunction), Info.Function);
			CopyString(Self->LastMessage, sizeof(Self->LastMessage), Info.Message);
		}
	};

	/// Restores the default handler when a test installs one by hand and
	/// leaves early through VT_REQUIRE.
	struct DefaultHandlerRestorer
	{
		DefaultHandlerRestorer() noexcept = default;
		~DefaultHandlerRestorer() noexcept { SetAssertHandler(nullptr); }
		DefaultHandlerRestorer(const DefaultHandlerRestorer&) = delete;
		DefaultHandlerRestorer& operator=(const DefaultHandlerRestorer&) = delete;
	};

	// Plain-function handler used to observe the UserData argument, including
	// the nullptr default of SetAssertHandler(Handler).
	void* GProbeLastUserData = nullptr;
	int32 GProbeCalls = 0;

	[[maybe_unused]] void ProbeHandler(const AssertInfo& /*Info*/, void* UserData)
	{
		++GProbeCalls;
		GProbeLastUserData = UserData;
	}

	struct UserDataPayload
	{
		int32 Tag = 0;
	};

} // namespace

// ── AssertInfo ──────────────────────────────────────────────────────────────

VAELEN_TEST(Assert, InfoDefaultsAreEmptyStrings)
{
	const AssertInfo Info;
	VT_CHECK(Info.Kind == AssertKind::Check);
	VT_REQUIRE(Info.Expression != nullptr);
	VT_REQUIRE(Info.File != nullptr);
	VT_REQUIRE(Info.Function != nullptr);
	VT_REQUIRE(Info.Message != nullptr);
	VT_CHECK_STREQ(Info.Expression, "");
	VT_CHECK_STREQ(Info.File, "");
	VT_CHECK_STREQ(Info.Function, "");
	VT_CHECK_STREQ(Info.Message, "");
	VT_CHECK_EQ(Info.Line, int32{0});
	VT_CHECK_EQ(Vaelen::ToUnderlying(AssertKind::Check), uint8{0});
	VT_CHECK_EQ(Vaelen::ToUnderlying(AssertKind::Ensure), uint8{1});
}

VAELEN_TEST(Assert, EnsureYieldsBool)
{
	// The macro is an expression usable in conditions and initialisers.
	static_assert(std::is_same_v<decltype(VAELEN_ENSURE(true)), bool>, "VAELEN_ENSURE must yield bool");
	static_assert(std::is_same_v<decltype(VAELEN_ENSURE(1 + 1)), bool>,
				  "integral expressions are contextually converted");
	const int32* Null = nullptr;
	static_assert(std::is_same_v<decltype(VAELEN_ENSURE(Null)), bool>, "pointers are contextually converted");
	(void)Null;
	VT_CHECK(SignOf(5) == 1);
	VT_CHECK(SignOf(-5) == -1);
	VT_CHECK(SignOf(0) == 1);
}

#if VAELEN_ASSERTS_ENABLED

// ── VAELEN_CHECK ────────────────────────────────────────────────────────────

VAELEN_TEST(Assert, CheckFalseReportsOneCheck)
{
	ScopedAssertCapture Capture;
	VAELEN_CHECK(false);
	// Execution continues: returning from a Check handler is allowed.
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "false");
	VT_CHECK_STREQ(Capture.LastMessage, "");
}

VAELEN_TEST(Assert, CheckTrueReportsNothing)
{
	ScopedAssertCapture Capture;
	VAELEN_CHECK(true);
	VAELEN_CHECK(1 + 1 == 2);
	VAELEN_CHECKF(true, "never formatted %d", 1);
	VAELEN_VERIFY(true);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "");
	VT_CHECK_STREQ(Capture.LastMessage, "");
}

VAELEN_TEST(Assert, CheckExpressionTextIsStringified)
{
	ScopedAssertCapture Capture;
	const int32 Value = 2;
	VAELEN_CHECK(Value + 1 == 4);
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "Value + 1 == 4");

	// Each failure overwrites the last expression.
	VAELEN_CHECK(Value < 0);
	VT_CHECK_EQ(Capture.CheckCount, 2);
	VT_CHECK_STREQ(Capture.LastExpression, "Value < 0");
}

VAELEN_TEST(Assert, CheckAcceptsNonBoolExpressions)
{
	ScopedAssertCapture Capture;
	const int32* NullPointer = nullptr;
	const int32 Zero = 0;
	const int32 One = 1;
	VAELEN_CHECK(NullPointer);
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "NullPointer");
	VAELEN_CHECK(Zero);
	VT_CHECK_EQ(Capture.CheckCount, 2);
	VAELEN_CHECK(One);
	VAELEN_CHECK(&One);
	VT_CHECK_EQ(Capture.CheckCount, 2);
}

VAELEN_TEST(Assert, CheckEvaluatesExpressionOnce)
{
	ScopedAssertCapture Capture;
	int32 Counter = 0;
	VAELEN_CHECK(++Counter == 1); // passes
	VT_CHECK_EQ(Counter, 1);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VAELEN_CHECK(++Counter == 100); // fails
	VT_CHECK_EQ(Counter, 2);
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "++Counter == 100");
}

// ── VAELEN_CHECKF ───────────────────────────────────────────────────────────

VAELEN_TEST(Assert, CheckFormatsMessage)
{
	ScopedAssertCapture Capture;
	VAELEN_CHECKF(false, "v=%d", 7);
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "false");
	VT_CHECK_STREQ(Capture.LastMessage, "v=7");
}

VAELEN_TEST(Assert, CheckFormatsSeveralArguments)
{
	ScopedAssertCapture Capture;
	const char* Name = "settlement";
	const uint64 Big = 0xFFFFFFFFFFFFFFFFull;
	VAELEN_CHECKF(1 == 2, "%s #%u: %llu %c", Name, 42u, static_cast<unsigned long long>(Big), 'x');
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "1 == 2");
	VT_CHECK_STREQ(Capture.LastMessage, "settlement #42: 18446744073709551615 x");
}

VAELEN_TEST(Assert, CheckFormatWithoutArguments)
{
	ScopedAssertCapture Capture;
	VAELEN_CHECKF(false, "plain text");
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastMessage, "plain text");

	// A message that is itself empty stays empty (no "(null)" or garbage).
	VAELEN_CHECKF(false, "%s", "");
	VT_CHECK_EQ(Capture.CheckCount, 2);
	VT_CHECK_STREQ(Capture.LastMessage, "");
}

VAELEN_TEST(Assert, CheckFormatEvaluatesExpressionOnceAndArgumentsOnlyOnFailure)
{
	ScopedAssertCapture Capture;
	int32 ExprCounter = 0;
	int32 ArgCounter = 0;

	// Passing: the expression is evaluated once, the format arguments never.
	VAELEN_CHECKF(++ExprCounter == 1, "arg=%d", ++ArgCounter);
	VT_CHECK_EQ(ExprCounter, 1);
	VT_CHECK_EQ(ArgCounter, 0);
	VT_CHECK_EQ(Capture.CheckCount, 0);

	// Failing: the expression is still evaluated once, the arguments once.
	VAELEN_CHECKF(++ExprCounter == 100, "arg=%d", ++ArgCounter);
	VT_CHECK_EQ(ExprCounter, 2);
	VT_CHECK_EQ(ArgCounter, 1);
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "++ExprCounter == 100");
	VT_CHECK_STREQ(Capture.LastMessage, "arg=1");
}

// ── VAELEN_VERIFY ───────────────────────────────────────────────────────────

VAELEN_TEST(Assert, VerifyEvaluatesExpressionOnce)
{
	ScopedAssertCapture Capture;
	int32 Counter = 0;
	VAELEN_VERIFY(++Counter == 1); // passes
	VT_CHECK_EQ(Counter, 1);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_EQ(Capture.EnsureCount, 0);

	VAELEN_VERIFY(++Counter == 100); // fails
	VT_CHECK_EQ(Counter, 2);
	VT_CHECK_EQ(Capture.CheckCount, 1);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "++Counter == 100");
	VT_CHECK_STREQ(Capture.LastMessage, "");
}

// ── VAELEN_ENSURE ───────────────────────────────────────────────────────────

VAELEN_TEST(Assert, EnsureReturnsResultAndDoesNotAbort)
{
	ScopedAssertCapture Capture;
	const bool Failed = VAELEN_ENSURE(false);
	// Still running: Ensure is non-fatal.
	VT_CHECK(!Failed);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "false");
	VT_CHECK_STREQ(Capture.LastMessage, "");

	const bool Passed = VAELEN_ENSURE(true);
	VT_CHECK(Passed);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
	VT_CHECK_EQ(Capture.CheckCount, 0);
}

VAELEN_TEST(Assert, EnsureTrueReportsNothing)
{
	ScopedAssertCapture Capture;
	VT_CHECK(VAELEN_ENSURE(true));
	VT_CHECK(VAELEN_ENSURE(2 > 1));
	const int32 Value = 3;
	VT_CHECK(VAELEN_ENSURE(&Value));
	VT_CHECK(VAELEN_ENSURE(Value));
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_STREQ(Capture.LastExpression, "");
}

VAELEN_TEST(Assert, EnsureEvaluatesExpressionOnce)
{
	ScopedAssertCapture Capture;
	int32 Counter = 0;
	VT_CHECK(VAELEN_ENSURE(++Counter == 1)); // passes
	VT_CHECK_EQ(Counter, 1);
	VT_CHECK(!VAELEN_ENSURE(++Counter == 100)); // fails
	VT_CHECK_EQ(Counter, 2);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "++Counter == 100");
}

VAELEN_TEST(Assert, EnsureReportsEveryFailure)
{
	// Actual kernel behaviour: every failing evaluation is reported, including
	// repeated failures of the same call site (there is no once-per-site latch).
	ScopedAssertCapture Capture;
	int32 FalseResults = 0;
	for (int32 i = 0; i < 3; ++i)
	{
		if (!VAELEN_ENSURE(i < 0))
		{
			++FalseResults;
		}
	}
	VT_CHECK_EQ(FalseResults, 3);
	VT_CHECK_EQ(Capture.EnsureCount, 3);
	VT_CHECK_EQ(Capture.CheckCount, 0);
}

VAELEN_TEST(Assert, EnsureGuardsEarlyReturn)
{
	ScopedAssertCapture Capture;
	int32 BodyRuns = 0;
	const auto Guarded = [&BodyRuns](const int32* Pointer) -> int32
	{
		if (!VAELEN_ENSURE(Pointer != nullptr))
		{
			return -1;
		}
		++BodyRuns;
		return *Pointer;
	};
	const int32 Value = 9;
	VT_CHECK_EQ(Guarded(&Value), 9);
	VT_CHECK_EQ(BodyRuns, 1);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_EQ(Guarded(nullptr), -1);
	VT_CHECK_EQ(BodyRuns, 1);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "Pointer != nullptr");
}

// ── Failure counter ─────────────────────────────────────────────────────────

VAELEN_TEST(Assert, FailureCountIncreasesPerReport)
{
	ScopedAssertCapture Capture;
	const uint64 Before = GetAssertFailureCount();

	// Passing assertions do not count.
	VAELEN_CHECK(true);
	VAELEN_CHECKF(true, "%d", 1);
	VAELEN_VERIFY(true);
	VT_CHECK(VAELEN_ENSURE(true));
	VT_CHECK_EQ(GetAssertFailureCount(), Before);

	VAELEN_CHECK(false);
	VT_CHECK_EQ(GetAssertFailureCount(), Before + 1);
	VAELEN_CHECKF(false, "%d", 2);
	VT_CHECK_EQ(GetAssertFailureCount(), Before + 2);
	VAELEN_VERIFY(false);
	VT_CHECK_EQ(GetAssertFailureCount(), Before + 3);
	VT_CHECK(!VAELEN_ENSURE(false));
	VT_CHECK_EQ(GetAssertFailureCount(), Before + 4);

	VT_CHECK_EQ(Capture.CheckCount + Capture.EnsureCount, 4);
	VT_CHECK_EQ(Capture.CheckCount, 3);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
}

VAELEN_TEST(Assert, FailureCountIsMonotonic)
{
	ScopedAssertCapture Capture;
	uint64 Previous = GetAssertFailureCount();
	for (int32 i = 0; i < 10; ++i)
	{
		VAELEN_CHECK(i < 0);
		const uint64 Now = GetAssertFailureCount();
		VT_CHECK_EQ(Now, Previous + 1);
		Previous = Now;
	}
	VT_CHECK_EQ(Capture.CheckCount, 10);
}

// ── Handler installation ────────────────────────────────────────────────────

VAELEN_TEST(Assert, HandlerReceivesUserData)
{
	DefaultHandlerRestorer Restorer;
	UserDataPayload Payload;
	Payload.Tag = 1234;
	GProbeCalls = 0;
	GProbeLastUserData = nullptr;

	SetAssertHandler(&ProbeHandler, &Payload);
	VT_CHECK(!VAELEN_ENSURE(false));
	VT_CHECK_EQ(GProbeCalls, 1);
	VT_CHECK(GProbeLastUserData == static_cast<void*>(&Payload));
	VT_CHECK_EQ(static_cast<UserDataPayload*>(GProbeLastUserData)->Tag, 1234);

	// A Check reaching a custom handler that returns does not abort either.
	VAELEN_CHECK(false);
	VT_CHECK_EQ(GProbeCalls, 2);
	VT_CHECK(GProbeLastUserData == static_cast<void*>(&Payload));

	// Re-installing with different user data takes effect immediately.
	UserDataPayload Other;
	SetAssertHandler(&ProbeHandler, &Other);
	VT_CHECK(!VAELEN_ENSURE(false));
	VT_CHECK_EQ(GProbeCalls, 3);
	VT_CHECK(GProbeLastUserData == static_cast<void*>(&Other));

	// The default argument is nullptr.
	SetAssertHandler(&ProbeHandler);
	VT_CHECK(!VAELEN_ENSURE(false));
	VT_CHECK_EQ(GProbeCalls, 4);
	VT_CHECK(GProbeLastUserData == nullptr);
}

VAELEN_TEST(Assert, ResetToDefaultThenCaptureInstallsCorrectly)
{
	// SetAssertHandler(nullptr) restores the default (log + abort) handler. It
	// is never triggered here; we only prove that a capture installed AFTER the
	// reset receives the reports (i.e. the reset did not leave a stale handler).
	GProbeCalls = 0;
	{
		DefaultHandlerRestorer Restorer;
		SetAssertHandler(&ProbeHandler, nullptr);
		VT_CHECK(!VAELEN_ENSURE(false));
		VT_CHECK_EQ(GProbeCalls, 1);
		SetAssertHandler(nullptr);
	}

	{
		ScopedAssertCapture Capture;
		VAELEN_CHECK(false);
		VT_CHECK(!VAELEN_ENSURE(false));
		VT_CHECK_EQ(Capture.CheckCount, 1);
		VT_CHECK_EQ(Capture.EnsureCount, 1);
	}
	// The probe handler was not called again: the capture replaced it.
	VT_CHECK_EQ(GProbeCalls, 1);

	// Sequential captures start from clean counters.
	{
		ScopedAssertCapture Second;
		VT_CHECK_EQ(Second.CheckCount, 0);
		VT_CHECK_EQ(Second.EnsureCount, 0);
		VAELEN_CHECK(false);
		VT_CHECK_EQ(Second.CheckCount, 1);
	}
	VT_CHECK_EQ(GProbeCalls, 1);
}

VAELEN_TEST(Assert, RecordingHandlerGetsItsOwnPointer)
{
	RecordingHandler Handler;
	VAELEN_CHECK(false);
	VT_CHECK_EQ(Handler.CallCount, 1);
	VT_CHECK(Handler.LastUserData == static_cast<void*>(&Handler));
	VT_CHECK(Handler.LastKind == AssertKind::Check);
}

// ── AssertInfo contents ─────────────────────────────────────────────────────

VAELEN_TEST(Assert, InfoLocationFieldsAreFilled)
{
	RecordingHandler Handler;
	const int32 LineBefore = __LINE__;
	VAELEN_CHECK(false);
	VT_REQUIRE_EQ(Handler.CallCount, 1);
	VT_CHECK(Handler.LastKind == AssertKind::Check);
	VT_CHECK(!Handler.LastExpressionWasNull);
	VT_CHECK(!Handler.LastFileWasNull);
	VT_CHECK(!Handler.LastFunctionWasNull);
	VT_CHECK(!Handler.LastMessageWasNull);
	VT_CHECK_STREQ(Handler.LastExpression, "false");
	VT_CHECK(std::strstr(Handler.LastFile, "Test_Assert.cpp") != nullptr);
	VT_CHECK(Handler.LastLine > 0);
	VT_CHECK_EQ(Handler.LastLine, LineBefore + 1);
	VT_CHECK(Handler.LastFunction[0] != '\0');
	VT_CHECK(std::strstr(Handler.LastFunction, "InfoLocationFieldsAreFilled") != nullptr);
	VT_CHECK_STREQ(Handler.LastMessage, "");
	VT_CHECK_EQ(Handler.LastMessageLength, usize{0});
}

VAELEN_TEST(Assert, InfoLocationFieldsForEnsureAndCheckF)
{
	RecordingHandler Handler;

	const int32 EnsureLineBefore = __LINE__;
	VT_CHECK(!VAELEN_ENSURE(1 > 2));
	VT_REQUIRE_EQ(Handler.CallCount, 1);
	VT_CHECK(Handler.LastKind == AssertKind::Ensure);
	VT_CHECK_STREQ(Handler.LastExpression, "1 > 2");
	VT_CHECK(std::strstr(Handler.LastFile, "Test_Assert.cpp") != nullptr);
	VT_CHECK_EQ(Handler.LastLine, EnsureLineBefore + 1);
	VT_CHECK(std::strstr(Handler.LastFunction, "InfoLocationFieldsForEnsureAndCheckF") != nullptr);
	VT_CHECK_STREQ(Handler.LastMessage, "");

	const int32 CheckFLineBefore = __LINE__;
	VAELEN_CHECKF(2 > 3, "a=%d b=%s", 1, "two");
	VT_REQUIRE_EQ(Handler.CallCount, 2);
	VT_CHECK(Handler.LastKind == AssertKind::Check);
	VT_CHECK_STREQ(Handler.LastExpression, "2 > 3");
	VT_CHECK(std::strstr(Handler.LastFile, "Test_Assert.cpp") != nullptr);
	VT_CHECK_EQ(Handler.LastLine, CheckFLineBefore + 1);
	VT_CHECK(std::strstr(Handler.LastFunction, "InfoLocationFieldsForEnsureAndCheckF") != nullptr);
	VT_CHECK_STREQ(Handler.LastMessage, "a=1 b=two");
	VT_CHECK_EQ(Handler.LastMessageLength, usize{9});

	const int32 VerifyLineBefore = __LINE__;
	VAELEN_VERIFY(3 > 4);
	VT_REQUIRE_EQ(Handler.CallCount, 3);
	VT_CHECK(Handler.LastKind == AssertKind::Check);
	VT_CHECK_STREQ(Handler.LastExpression, "3 > 4");
	VT_CHECK_EQ(Handler.LastLine, VerifyLineBefore + 1);
}

// ── Long messages ───────────────────────────────────────────────────────────

VAELEN_TEST(Assert, LongMessageIsTruncatedSafely)
{
	RecordingHandler Handler;
	constexpr usize LongLength = 2000;
	char Long[LongLength + 1];
	std::memset(Long, 'x', LongLength);
	Long[LongLength] = '\0';
	VT_REQUIRE_EQ(std::strlen(Long), LongLength);

	VAELEN_CHECKF(false, "%s", Long);
	VT_REQUIRE_EQ(Handler.CallCount, 1);
	VT_CHECK(!Handler.LastMessageWasNull);
	// The kernel buffer is exactly 1024 bytes (Assert.cpp MessageBufferSize):
	// 1023 characters survive, the last three of which are the "..." marker.
	constexpr usize AssertMessageBufferSize = 1024;
	VT_CHECK_EQ(Handler.LastMessageLength, AssertMessageBufferSize - 1);
	VT_CHECK_EQ(std::strlen(Handler.LastMessage), Handler.LastMessageLength);
	VT_CHECK(std::strcmp(Handler.LastMessage + Handler.LastMessageLength - 3, "...") == 0);

	// What survived before the marker is a prefix of the original.
	bool OnlyFill = true;
	for (usize i = 0; i + 3 < Handler.LastMessageLength; ++i)
	{
		if (Handler.LastMessage[i] != 'x')
		{
			OnlyFill = false;
			break;
		}
	}
	VT_CHECK(OnlyFill);

	// A long format string with a trailing argument truncates the same way.
	char LongFormat[LongLength + 4];
	std::memset(LongFormat, 'y', LongLength);
	LongFormat[LongLength] = '%';
	LongFormat[LongLength + 1] = 'd';
	LongFormat[LongLength + 2] = '\0';
	VAELEN_CHECKF(false, "%s%d", LongFormat, 5);
	VT_REQUIRE_EQ(Handler.CallCount, 2);
	VT_CHECK_EQ(Handler.LastMessageLength, AssertMessageBufferSize - 1);
	VT_CHECK_EQ(Handler.LastMessage[0], 'y');
	VT_CHECK(std::strcmp(Handler.LastMessage + Handler.LastMessageLength - 3, "...") == 0);
	VT_CHECK_EQ(std::strlen(Handler.LastMessage), Handler.LastMessageLength);

	// The capture in the harness copes with the long message as well.
	{
		ScopedAssertCapture Capture;
		VAELEN_CHECKF(false, "%s", Long);
		VT_CHECK_EQ(Capture.CheckCount, 1);
		VT_CHECK(std::strlen(Capture.LastMessage) < sizeof(Capture.LastMessage));
		VT_CHECK_EQ(Capture.LastMessage[0], 'x');
	}
}

VAELEN_TEST(Assert, MessageJustBelowLimitIsIntact)
{
	RecordingHandler Handler;
	// Exactly 1023 characters fit untouched; 1024 lose one character and get the marker.
	constexpr usize Length = 1023;
	char Text[Length + 2];
	std::memset(Text, 'z', Length + 1);
	Text[Length] = '\0';
	VAELEN_CHECKF(false, "%s", Text);
	VT_REQUIRE_EQ(Handler.CallCount, 1);
	VT_CHECK_EQ(Handler.LastMessageLength, Length);
	VT_CHECK_STREQ(Handler.LastMessage, Text);

	Text[Length] = 'z';
	Text[Length + 1] = '\0';
	VAELEN_CHECKF(false, "%s", Text);
	VT_REQUIRE_EQ(Handler.CallCount, 2);
	VT_CHECK_EQ(Handler.LastMessageLength, Length);
	VT_CHECK(std::strncmp(Handler.LastMessage, Text, Length - 3) == 0);
	VT_CHECK(std::strcmp(Handler.LastMessage + Length - 3, "...") == 0);
}

// ── Statement-context safety ────────────────────────────────────────────────

VAELEN_TEST(Assert, MacrosAreSingleStatements)
{
	// Brace-less if/else around every macro: a macro that expanded to more
	// than one statement (or to a dangling `if`) would bind the `else` wrongly
	// or fail to compile. See CountElseBranches for the actual forms: the
	// unreachable one always takes its else branch (1), the four others take
	// theirs only when the condition is false (4 more).
	ScopedAssertCapture Capture;
	VT_CHECK_EQ(CountElseBranches(true), 1);
	VT_CHECK_EQ(CountElseBranches(false), 5);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_EQ(Capture.EnsureCount, 0);

	// Same forms with failing expressions still report exactly once each.
	int32 ElseCount = 0;
	const bool Condition = Capture.CheckCount == 0;
	if (Condition)
		VAELEN_CHECK(Capture.CheckCount == 99);
	else
		++ElseCount;
	VT_CHECK_EQ(Capture.CheckCount, 1);

	if (Condition)
		VAELEN_CHECKF(Capture.CheckCount == 99, "count=%d", Capture.CheckCount);
	else
		++ElseCount;
	VT_CHECK_EQ(Capture.CheckCount, 2);
	VT_CHECK_STREQ(Capture.LastMessage, "count=1");

	if (Condition)
		VAELEN_VERIFY(Capture.CheckCount == 99);
	else
		++ElseCount;
	VT_CHECK_EQ(Capture.CheckCount, 3);

	if (Condition)
		VAELEN_ENSURE(Capture.CheckCount == 99);
	else
		++ElseCount;
	VT_CHECK_EQ(Capture.EnsureCount, 1);

	if (!Condition)
		VAELEN_UNREACHABLE();
	else
		++ElseCount;

	VT_CHECK_EQ(ElseCount, 1);
	VT_CHECK_EQ(Capture.CheckCount, 3);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
}

VAELEN_TEST(Assert, MacrosWorkInsideLoopsAndSwitches)
{
	ScopedAssertCapture Capture;
	int32 Reported = 0;
	for (int32 i = 0; i < 4; ++i)
		VAELEN_CHECK(i != 2);
	VT_CHECK_EQ(Capture.CheckCount, 1);

	for (int32 i = 0; i < 4; ++i)
	{
		switch (i)
		{
		case 0:
			VAELEN_CHECK(true);
			break;
		case 1:
			VAELEN_CHECKF(false, "case %d", i);
			++Reported;
			break;
		case 2:
			if (!VAELEN_ENSURE(false))
			{
				++Reported;
			}
			break;
		default:
			VAELEN_VERIFY(i == 3);
			break;
		}
	}
	VT_CHECK_EQ(Reported, 2);
	VT_CHECK_EQ(Capture.CheckCount, 2);
	VT_CHECK_EQ(Capture.EnsureCount, 1);
	VT_CHECK_STREQ(Capture.LastExpression, "false");
}

#else // VAELEN_ASSERTS_ENABLED == 0

VAELEN_TEST(Assert, DisabledCheckIsNotEvaluated)
{
	ScopedAssertCapture Capture;
	int32 Counter = 0;
	VAELEN_CHECK(++Counter == 1);
	VAELEN_CHECKF(++Counter == 1, "%d", ++Counter);
	VAELEN_CHECK(false);
	VAELEN_CHECKF(false, "v=%d", 7);
	VT_CHECK_EQ(Counter, 0);
	VT_CHECK_EQ(Capture.CheckCount, 0);
	VT_CHECK_EQ(Capture.EnsureCount, 0);
}

VAELEN_TEST(Assert, DisabledVerifyStillEvaluatesOnce)
{
	ScopedAssertCapture Capture;
	int32 Counter = 0;
	VAELEN_VERIFY(++Counter == 1);
	VAELEN_VERIFY(++Counter == 100);
	VT_CHECK_EQ(Counter, 2);
	VT_CHECK_EQ(Capture.CheckCount, 0);
}

VAELEN_TEST(Assert, DisabledEnsureYieldsResultWithoutReport)
{
	ScopedAssertCapture Capture;
	const uint64 Before = GetAssertFailureCount();
	int32 Counter = 0;
	VT_CHECK(VAELEN_ENSURE(++Counter == 1));
	VT_CHECK(!VAELEN_ENSURE(++Counter == 100));
	VT_CHECK_EQ(Counter, 2);
	VT_CHECK(!VAELEN_ENSURE(false));
	VT_CHECK(VAELEN_ENSURE(true));
	VT_CHECK_EQ(Capture.EnsureCount, 0);
	VT_CHECK_EQ(GetAssertFailureCount(), Before);
	VT_CHECK_EQ(CountElseBranches(true), 1);
	VT_CHECK_EQ(CountElseBranches(false), 5);
}

#endif // VAELEN_ASSERTS_ENABLED

// ── Build-independent API (Detail::ReportAssert is compiled in every build) ──

namespace
{
	struct CountingHandler
	{
		std::atomic<int32> Calls{0};
		std::atomic<int32> WrongUserData{0};

		static void OnAssert(const AssertInfo&, void* UserData)
		{
			// UserData is the handler itself: the (handler, user data) pair must
			// never be torn, and every report must reach this handler.
			auto* Self = static_cast<CountingHandler*>(UserData);
			if (Self == nullptr)
			{
				return;
			}
			Self->Calls.fetch_add(1, std::memory_order_relaxed);
		}
	};

	class CapturingLogSink final : public Vaelen::ILogSink
	{
	public:
		void Write(const Vaelen::LogRecord& Record) override
		{
			++Count;
			Last = Record.Message != nullptr ? Record.Message : "";
			LastLevel = Record.Level;
		}
		int32 Count = 0;
		std::string Last;
		Vaelen::LogLevel LastLevel = Vaelen::LogLevel::Trace;
	};
} // namespace

VAELEN_TEST(Assert, ReportAssertReachesHandlerInEveryBuild)
{
	RecordingHandler Handler;
	const uint64 Before = GetAssertFailureCount();

	Vaelen::Detail::ReportAssert(AssertKind::Ensure, "p != nullptr", "file.cpp", 42, "Func");
	VT_REQUIRE_EQ(Handler.CallCount, 1);
	VT_CHECK_EQ(Handler.LastKind, AssertKind::Ensure);
	VT_CHECK_EQ(Handler.LastLine, 42);
	VT_CHECK_STREQ(Handler.LastExpression, "p != nullptr");
	VT_CHECK_STREQ(Handler.LastFile, "file.cpp");
	VT_CHECK_STREQ(Handler.LastFunction, "Func");
	VT_CHECK_EQ(Handler.LastMessageLength, usize{0});
	VT_CHECK(Handler.LastUserData == static_cast<void*>(&Handler));

	Vaelen::Detail::ReportAssertF(AssertKind::Check, "x", "f.cpp", 7, "F", "v=%d %s", 7, "seven");
	VT_REQUIRE_EQ(Handler.CallCount, 2);
	VT_CHECK_EQ(Handler.LastKind, AssertKind::Check);
	VT_CHECK_STREQ(Handler.LastMessage, "v=7 seven");
	VT_CHECK_EQ(GetAssertFailureCount() - Before, uint64{2});
}

VAELEN_TEST(Assert, GetAssertHandlerReportsDefaultAndInstalled)
{
	AssertHandler Previous = nullptr;
	void* PreviousData = nullptr;
	Previous = Vaelen::GetAssertHandler(&PreviousData);
	{
		RecordingHandler Handler;
		void* Data = nullptr;
		VT_CHECK(Vaelen::GetAssertHandler(&Data) == &RecordingHandler::OnAssert);
		VT_CHECK(Data == static_cast<void*>(&Handler));
		VT_CHECK(Vaelen::GetAssertHandler() == &RecordingHandler::OnAssert);
	}
	// RecordingHandler restores the default: nullptr, no user data.
	void* Data = reinterpret_cast<void*>(1);
	VT_CHECK(Vaelen::GetAssertHandler(&Data) == nullptr);
	VT_CHECK(Data == nullptr);
	VT_CHECK(Vaelen::GetAssertHandler() == nullptr);
	SetAssertHandler(Previous, PreviousData);
}

VAELEN_TEST(Assert, DefaultHandlerLogsEnsureAndContinues)
{
	// With no handler installed an Ensure is reported to stderr and to LogCore,
	// and execution continues (only Check aborts). The stderr line is expected output.
	AssertHandler Previous = nullptr;
	void* PreviousData = nullptr;
	Previous = Vaelen::GetAssertHandler(&PreviousData);
	SetAssertHandler(nullptr);

	CapturingLogSink Sink;
	VT_REQUIRE(Vaelen::Log::AddSink(&Sink));
	const uint64 Before = GetAssertFailureCount();
	Vaelen::Detail::ReportAssertF(AssertKind::Ensure, "Value == 4", "Test_Assert.cpp", 1, "TestFunc", "value was %d",
								  3);
	Vaelen::Log::RemoveSink(&Sink);
	SetAssertHandler(Previous, PreviousData);

	VT_CHECK_EQ(GetAssertFailureCount() - Before, uint64{1});
	VT_REQUIRE_EQ(Sink.Count, 1);
	VT_CHECK_EQ(Sink.LastLevel, Vaelen::LogLevel::Error);
	VT_CHECK(Sink.Last.find("ENSURE failed at Test_Assert.cpp:1") != std::string::npos);
	VT_CHECK(Sink.Last.find("Value == 4 -- value was 3") != std::string::npos);
}

VAELEN_TEST(Assert, ConcurrentReportsAreAllCountedWithTheirUserData)
{
	CountingHandler Handler;
	SetAssertHandler(&CountingHandler::OnAssert, &Handler);
	const uint64 Before = GetAssertFailureCount();

	constexpr int32 Threads = 8;
	constexpr int32 PerThread = 1000;
	std::vector<std::thread> Workers;
	for (int32 t = 0; t < Threads; ++t)
	{
		Workers.emplace_back(
			[]()
			{
				for (int32 i = 0; i < PerThread; ++i)
				{
					Vaelen::Detail::ReportAssert(AssertKind::Ensure, "concurrent", "f.cpp", i, "F");
				}
			});
	}
	for (std::thread& Worker : Workers)
	{
		Worker.join();
	}
	SetAssertHandler(nullptr);

	VT_CHECK_EQ(Handler.Calls.load(), Threads * PerThread);
	VT_CHECK_EQ(GetAssertFailureCount() - Before, static_cast<uint64>(Threads * PerThread));
}

VAELEN_TEST(Assert, HandlerSwapDuringReportsNeverTearsThePair)
{
	// Two handlers with their own user data are swapped while other threads
	// report; each report must arrive at a handler with ITS OWN user data.
	struct PairHandler
	{
		std::atomic<int32> Calls{0};
		std::atomic<int32> Mismatches{0};
		static void A(const AssertInfo&, void* UserData) { Record(UserData, 'A'); }
		static void B(const AssertInfo&, void* UserData) { Record(UserData, 'B'); }
		static void Record(void* UserData, char Expected)
		{
			auto* Self = static_cast<PairHandler*>(UserData);
			Self->Calls.fetch_add(1, std::memory_order_relaxed);
			if (Self->Tag != Expected)
			{
				Self->Mismatches.fetch_add(1, std::memory_order_relaxed);
			}
		}
		char Tag = ' ';
	};
	PairHandler HandlerA;
	HandlerA.Tag = 'A';
	PairHandler HandlerB;
	HandlerB.Tag = 'B';

	SetAssertHandler(&PairHandler::A, &HandlerA);
	std::atomic<bool> Stop{false};
	std::vector<std::thread> Reporters;
	for (int32 t = 0; t < 4; ++t)
	{
		Reporters.emplace_back(
			[&Stop]()
			{
				while (!Stop.load(std::memory_order_relaxed))
				{
					Vaelen::Detail::ReportAssert(AssertKind::Ensure, "swap", "f.cpp", 1, "F");
				}
			});
	}
	for (int32 i = 0; i < 2000; ++i)
	{
		SetAssertHandler((i & 1) ? &PairHandler::B : &PairHandler::A,
						 (i & 1) ? static_cast<void*>(&HandlerB) : static_cast<void*>(&HandlerA));
	}
	Stop.store(true, std::memory_order_relaxed);
	for (std::thread& Reporter : Reporters)
	{
		Reporter.join();
	}
	SetAssertHandler(nullptr);

	VT_CHECK(HandlerA.Calls.load() + HandlerB.Calls.load() > 0);
	VT_CHECK_EQ(HandlerA.Mismatches.load(), 0);
	VT_CHECK_EQ(HandlerB.Mismatches.load(), 0);
}
