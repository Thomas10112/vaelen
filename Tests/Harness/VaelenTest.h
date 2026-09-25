// VAELEN - minimal test harness (no external dependency, no exceptions, no RTTI).
//
// STATUS: VALIDATED (Phase 00)
//
// Usage:
//   #include "VaelenTest.h"
//   VAELEN_TEST(Random, KnownAnswer)   // suite "Random" must match the file name Test_Random.cpp
//   {
//       VT_CHECK_EQ(1 + 1, 2);
//       VT_REQUIRE(Ptr != nullptr);     // stops this test on failure
//   }
//
// Run:  VaelenCoreTests [--suite Name] [--filter Substring] [--list] [--verbose] [--quiet-log]
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Assert.h"

#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

namespace VaelenTest
{
	struct Context
	{
		int Failures = 0;
		int Checks = 0;
		bool Verbose = false;
		bool Silent = false; ///< Record failures without printing (harness self-tests).

		void ReportFailure(const char* File, int Line, const char* What, const char* Detail = nullptr)
		{
			++Failures;
			if (Silent)
			{
				return;
			}
			if (Detail != nullptr && Detail[0] != '\0')
			{
				std::fprintf(stderr, "    FAIL %s:%d: %s\n         %s\n", File, Line, What, Detail);
			}
			else
			{
				std::fprintf(stderr, "    FAIL %s:%d: %s\n", File, Line, What);
			}
		}
	};

	using TestFunction = void (*)(Context&);

	struct TestCase
	{
		const char* Suite;
		const char* Name;
		TestFunction Function;
		const char* File;
		int Line;
		TestCase* Next;
	};

	/// Registry head (intrusive singly linked list, filled by static initialisers).
	TestCase*& Registry();

	struct Registrar
	{
		Registrar(TestCase* Case) noexcept
		{
			// Append preserving declaration order within a translation unit.
			TestCase** Tail = &Registry();
			while (*Tail != nullptr)
			{
				Tail = &(*Tail)->Next;
			}
			*Tail = Case;
		}
	};

	/// Captures kernel assertion failures instead of aborting, for tests that
	/// exercise VAELEN_CHECK / VAELEN_ENSURE paths. Restores the previously
	/// installed handler (and its user data) on destruction, so captures may
	/// be nested.
	class ScopedAssertCapture
	{
	public:
		ScopedAssertCapture() noexcept
		{
			PreviousHandler = Vaelen::GetAssertHandler(&PreviousUserData);
			Vaelen::SetAssertHandler(&OnAssert, this);
		}
		~ScopedAssertCapture() noexcept { Vaelen::SetAssertHandler(PreviousHandler, PreviousUserData); }
		ScopedAssertCapture(const ScopedAssertCapture&) = delete;
		ScopedAssertCapture& operator=(const ScopedAssertCapture&) = delete;

		int CheckCount = 0;
		int EnsureCount = 0;
		char LastExpression[256] = {};
		char LastMessage[512] = {};

	private:
		Vaelen::AssertHandler PreviousHandler = nullptr;
		void* PreviousUserData = nullptr;

		static void OnAssert(const Vaelen::AssertInfo& Info, void* UserData)
		{
			auto* Self = static_cast<ScopedAssertCapture*>(UserData);
			if (Info.Kind == Vaelen::AssertKind::Check)
			{
				++Self->CheckCount;
			}
			else
			{
				++Self->EnsureCount;
			}
			std::snprintf(Self->LastExpression, sizeof(Self->LastExpression), "%s", Info.Expression);
			std::snprintf(Self->LastMessage, sizeof(Self->LastMessage), "%s", Info.Message);
		}
	};

	namespace Detail
	{
		template <typename T>
		void FormatValue(char* Out, Vaelen::usize Size, const T& Value)
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				std::snprintf(Out, Size, "%s", Value ? "true" : "false");
			}
			else if constexpr (std::is_floating_point_v<T>)
			{
				std::snprintf(Out, Size, "%.17g", static_cast<double>(Value));
			}
			else if constexpr (std::is_enum_v<T>)
			{
				std::snprintf(Out, Size, "%lld", static_cast<long long>(Value));
			}
			else if constexpr (std::is_signed_v<T>)
			{
				std::snprintf(Out, Size, "%lld", static_cast<long long>(Value));
			}
			else if constexpr (std::is_unsigned_v<T>)
			{
				std::snprintf(Out, Size, "%llu (0x%llx)", static_cast<unsigned long long>(Value),
							  static_cast<unsigned long long>(Value));
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				std::snprintf(Out, Size, "%p", static_cast<const void*>(Value));
			}
			else
			{
				std::snprintf(Out, Size, "<value>");
			}
		}

		/// Integer types std::cmp_equal accepts: integral, but not bool or a
		/// character type.
		template <typename T>
		inline constexpr bool IsPlainInteger =
			std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> &&
			!std::is_same_v<T, wchar_t> && !std::is_same_v<T, char8_t> && !std::is_same_v<T, char16_t> &&
			!std::is_same_v<T, char32_t>;

		/// Equality that is mathematically correct across integer signedness
		/// (std::cmp_equal), so -1 never compares equal to ~uint64{0}.
		template <typename A, typename B>
		bool ValuesEqual(const A& Actual, const B& Expected)
		{
			if constexpr (IsPlainInteger<A> && IsPlainInteger<B>)
			{
				return std::cmp_equal(Actual, Expected);
			}
			else
			{
				return Actual == Expected;
			}
		}

		template <typename A, typename B>
		bool CheckEqual(Context& Ctx, const char* File, int Line, const char* Expr, const A& Actual, const B& Expected)
		{
			++Ctx.Checks;
			if (ValuesEqual(Actual, Expected))
			{
				return true;
			}
			char ActualText[128];
			char ExpectedText[128];
			FormatValue(ActualText, sizeof(ActualText), Actual);
			FormatValue(ExpectedText, sizeof(ExpectedText), Expected);
			char Detail[300];
			std::snprintf(Detail, sizeof(Detail), "actual: %s  expected: %s", ActualText, ExpectedText);
			Ctx.ReportFailure(File, Line, Expr, Detail);
			return false;
		}

		template <typename A, typename B>
		bool CheckNotEqual(Context& Ctx, const char* File, int Line, const char* Expr, const A& Actual,
						   const B& NotExpected)
		{
			++Ctx.Checks;
			if (!ValuesEqual(Actual, NotExpected))
			{
				return true;
			}
			char ActualText[128];
			FormatValue(ActualText, sizeof(ActualText), Actual);
			char Detail[200];
			std::snprintf(Detail, sizeof(Detail), "both values: %s", ActualText);
			Ctx.ReportFailure(File, Line, Expr, Detail);
			return false;
		}

		inline bool CheckStrEqual(Context& Ctx, const char* File, int Line, const char* Expr, const char* Actual,
								  const char* Expected)
		{
			++Ctx.Checks;
			if (Actual != nullptr && Expected != nullptr && std::strcmp(Actual, Expected) == 0)
			{
				return true;
			}
			char Detail[600];
			std::snprintf(Detail, sizeof(Detail), "actual: \"%s\"  expected: \"%s\"", Actual ? Actual : "(null)",
						  Expected ? Expected : "(null)");
			Ctx.ReportFailure(File, Line, Expr, Detail);
			return false;
		}

		inline bool CheckNear(Context& Ctx, const char* File, int Line, const char* Expr, double Actual,
							  double Expected, double Tolerance)
		{
			++Ctx.Checks;
			const double Diff = Actual > Expected ? Actual - Expected : Expected - Actual;
			if (Diff <= Tolerance)
			{
				return true;
			}
			char Detail[200];
			std::snprintf(Detail, sizeof(Detail), "actual: %.17g  expected: %.17g  tolerance: %.17g", Actual, Expected,
						  Tolerance);
			Ctx.ReportFailure(File, Line, Expr, Detail);
			return false;
		}

		// ── 17.07: assertions that can see their own vacuity ────────────────
		//
		// Phase 16 committed a round trip that compared two default-constructed
		// StartRules - written and read - and passed while the reader could
		// have written nothing into either. A reviewer found it; the suite did
		// not, because VT_CHECK asks whether two values are equal and not
		// whether the comparison could have come out any other way.

		/// A round trip is EVIDENCE only when the written value could not have
		/// been produced by a reader that did nothing. So the written value is
		/// checked against T{} BEFORE it is compared with what was read back,
		/// and a default-valued write is a recorded failure, not a pass.
		///
		/// Bytes are compared, which is what a round trip means for plain
		/// data, and the type must have unique object representations - no
		/// padding, no floating point - or byte equality would not be value
		/// equality and the vacuity check could be fooled by padding.
		template <typename T>
		bool CheckRoundTrip(Context& Ctx, const char* File, int Line, const char* What, const T& Written, const T& Read)
		{
			static_assert(std::is_trivially_copyable_v<T>,
						  "VT_CHECK_ROUNDTRIP compares bytes: the type must be trivially copyable");
			static_assert(std::has_unique_object_representations_v<T>,
						  "VT_CHECK_ROUNDTRIP compares bytes: a type with padding or floating point cannot be "
						  "compared this way, and its vacuity cannot be judged from its bytes");
			++Ctx.Checks;
			const T Default{};
			if (std::memcmp(&Written, &Default, sizeof(T)) == 0)
			{
				Ctx.ReportFailure(File, Line, What,
								  "VACUOUS: the written value is byte-equal to T{}, so a reader that wrote nothing "
								  "would pass this");
				return false;
			}
			if (std::memcmp(&Written, &Read, sizeof(T)) != 0)
			{
				Ctx.ReportFailure(File, Line, What, "the value read back differs from the value written");
				return false;
			}
			return true;
		}

		/// `EventLog::EmptyDigest` ("VAELEN-E"), repeated here as a literal
		/// because the harness sits below VaelenSim and may not include it;
		/// Tests/Sim pins that the two agree.
		inline constexpr unsigned long long EmptyLogDigest = 0x5641454c454e2d45ull;

		/// `HashConstants::Fnv1a64Offset`, repeated here for the same reason:
		/// FNV-1a over NO bytes is its offset basis, so this is what an empty
		/// story, an empty buffer or an empty string hashes to - the value the
		/// nine cells of 16.12's matrix that carried nobody compared against
		/// itself (each side `HashBytes` of an empty life). Tests/Core pins
		/// that the literal is the kernel's.
		inline constexpr unsigned long long EmptyBytesDigest = 0xcbf29ce484222325ull;

		/// Two digests are EVIDENCE only when neither is the value a digest
		/// has before anything was digested. 0 is what an unset field holds;
		/// EmptyLogDigest is what an empty log reports - and nine of the
		/// eighteen cells of 16.12's matrix compared an empty life digest
		/// against itself and were green.
		inline bool CheckDigestEqual(Context& Ctx, const char* File, int Line, const char* What, unsigned long long A,
									 unsigned long long B)
		{
			++Ctx.Checks;
			char Detail[192];
			if (A == 0ull || B == 0ull)
			{
				std::snprintf(Detail, sizeof(Detail),
							  "VACUOUS: %016llx against %016llx - a digest of 0 is the value an unset field holds", A,
							  B);
				Ctx.ReportFailure(File, Line, What, Detail);
				return false;
			}
			if (A == EmptyLogDigest || B == EmptyLogDigest)
			{
				std::snprintf(Detail, sizeof(Detail),
							  "VACUOUS: %016llx against %016llx - EventLog::EmptyDigest is what an EMPTY log "
							  "reports, so this compares nothing against itself",
							  A, B);
				Ctx.ReportFailure(File, Line, What, Detail);
				return false;
			}
			if (A == EmptyBytesDigest || B == EmptyBytesDigest)
			{
				std::snprintf(Detail, sizeof(Detail),
							  "VACUOUS: %016llx against %016llx - the FNV-1a offset basis is the digest of NO "
							  "bytes: an empty story, buffer or string, which is not evidence of anything",
							  A, B);
				Ctx.ReportFailure(File, Line, What, Detail);
				return false;
			}
			if (A != B)
			{
				std::snprintf(Detail, sizeof(Detail), "%016llx against %016llx", A, B);
				Ctx.ReportFailure(File, Line, What, Detail);
				return false;
			}
			return true;
		}
	} // namespace Detail
} // namespace VaelenTest

#define VAELEN_TEST(SuiteName, TestName)                                                                               \
	static void VaelenTest_##SuiteName##_##TestName(::VaelenTest::Context& Ctx);                                       \
	static ::VaelenTest::TestCase VaelenTestCase_##SuiteName##_##TestName = {                                          \
		#SuiteName, #TestName, &VaelenTest_##SuiteName##_##TestName, __FILE__, __LINE__, nullptr};                     \
	static ::VaelenTest::Registrar VaelenTestRegistrar_##SuiteName##_##TestName(                                       \
		&VaelenTestCase_##SuiteName##_##TestName);                                                                     \
	static void VaelenTest_##SuiteName##_##TestName([[maybe_unused]] ::VaelenTest::Context& Ctx)

/// Records a failure and continues.
#define VT_CHECK(Expr)                                                                                                 \
	do                                                                                                                 \
	{                                                                                                                  \
		++Ctx.Checks;                                                                                                  \
		VAELEN_MSVC_WARNING_SUPPRESS(4127)                                                                             \
		if (!(Expr))                                                                                                   \
		{                                                                                                              \
			Ctx.ReportFailure(__FILE__, __LINE__, "VT_CHECK(" #Expr ")");                                              \
		}                                                                                                              \
	} while (false)

/// Records a failure with a printf-style message and continues.
#define VT_CHECK_MSG(Expr, ...)                                                                                        \
	do                                                                                                                 \
	{                                                                                                                  \
		++Ctx.Checks;                                                                                                  \
		VAELEN_MSVC_WARNING_SUPPRESS(4127)                                                                             \
		if (!(Expr))                                                                                                   \
		{                                                                                                              \
			char VtDetail[512];                                                                                        \
			std::snprintf(VtDetail, sizeof(VtDetail), __VA_ARGS__);                                                    \
			Ctx.ReportFailure(__FILE__, __LINE__, "VT_CHECK(" #Expr ")", VtDetail);                                    \
		}                                                                                                              \
	} while (false)

/// Records a failure and RETURNS from the test.
#define VT_REQUIRE(Expr)                                                                                               \
	do                                                                                                                 \
	{                                                                                                                  \
		++Ctx.Checks;                                                                                                  \
		VAELEN_MSVC_WARNING_SUPPRESS(4127)                                                                             \
		if (!(Expr))                                                                                                   \
		{                                                                                                              \
			Ctx.ReportFailure(__FILE__, __LINE__, "VT_REQUIRE(" #Expr ")");                                            \
			return;                                                                                                    \
		}                                                                                                              \
	} while (false)

#define VT_CHECK_EQ(Actual, Expected)                                                                                  \
	::VaelenTest::Detail::CheckEqual(Ctx, __FILE__, __LINE__, "VT_CHECK_EQ(" #Actual ", " #Expected ")", (Actual),     \
									 (Expected))

#define VT_REQUIRE_EQ(Actual, Expected)                                                                                \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!::VaelenTest::Detail::CheckEqual(Ctx, __FILE__, __LINE__, "VT_REQUIRE_EQ(" #Actual ", " #Expected ")",    \
											  (Actual), (Expected)))                                                   \
		{                                                                                                              \
			return;                                                                                                    \
		}                                                                                                              \
	} while (false)

#define VT_CHECK_NE(Actual, NotExpected)                                                                               \
	::VaelenTest::Detail::CheckNotEqual(Ctx, __FILE__, __LINE__, "VT_CHECK_NE(" #Actual ", " #NotExpected ")",         \
										(Actual), (NotExpected))

#define VT_CHECK_STREQ(Actual, Expected)                                                                               \
	::VaelenTest::Detail::CheckStrEqual(Ctx, __FILE__, __LINE__, "VT_CHECK_STREQ(" #Actual ", " #Expected ")",         \
										(Actual), (Expected))

/// 17.07: a plain-data round trip that fails when the WRITTEN value is T{}.
#define VT_CHECK_ROUNDTRIP(Written, Read)                                                                              \
	::VaelenTest::Detail::CheckRoundTrip(Ctx, __FILE__, __LINE__, "VT_CHECK_ROUNDTRIP(" #Written ", " #Read ")",       \
										 (Written), (Read))

/// 17.07: two digests that must agree, and neither may be 0 or an empty log's.
#define VT_CHECK_DIGEST_EQ(A, B)                                                                                       \
	::VaelenTest::Detail::CheckDigestEqual(Ctx, __FILE__, __LINE__, "VT_CHECK_DIGEST_EQ(" #A ", " #B ")",              \
										   static_cast<unsigned long long>(A), static_cast<unsigned long long>(B))

#define VT_CHECK_NEAR(Actual, Expected, Tolerance)                                                                     \
	::VaelenTest::Detail::CheckNear(Ctx, __FILE__, __LINE__, "VT_CHECK_NEAR(" #Actual ", " #Expected ")",              \
									static_cast<double>(Actual), static_cast<double>(Expected),                        \
									static_cast<double>(Tolerance))
