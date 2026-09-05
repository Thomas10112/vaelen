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

namespace VaelenTest
{
	struct Context
	{
		int Failures = 0;
		int Checks = 0;
		bool Verbose = false;

		void ReportFailure(const char* File, int Line, const char* What, const char* Detail = nullptr)
		{
			++Failures;
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

		template <typename A, typename B>
		bool CheckEqual(Context& Ctx, const char* File, int Line, const char* Expr, const A& Actual, const B& Expected)
		{
			++Ctx.Checks;
			if (Actual == Expected)
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
	do                                                                                                                 \
	{                                                                                                                  \
		++Ctx.Checks;                                                                                                  \
		if ((Actual) == (NotExpected))                                                                                 \
		{                                                                                                              \
			Ctx.ReportFailure(__FILE__, __LINE__, "VT_CHECK_NE(" #Actual ", " #NotExpected ")");                       \
		}                                                                                                              \
	} while (false)

#define VT_CHECK_STREQ(Actual, Expected)                                                                               \
	::VaelenTest::Detail::CheckStrEqual(Ctx, __FILE__, __LINE__, "VT_CHECK_STREQ(" #Actual ", " #Expected ")",         \
										(Actual), (Expected))

#define VT_CHECK_NEAR(Actual, Expected, Tolerance)                                                                     \
	::VaelenTest::Detail::CheckNear(Ctx, __FILE__, __LINE__, "VT_CHECK_NEAR(" #Actual ", " #Expected ")",              \
									static_cast<double>(Actual), static_cast<double>(Expected),                        \
									static_cast<double>(Tolerance))
