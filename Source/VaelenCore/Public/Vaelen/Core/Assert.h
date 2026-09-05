// VAELEN - VaelenCore
// Assertions for a kernel built without exceptions.
//
// STATUS: VALIDATED (Phase 00) - unit/deterministic/edge tests in Tests/Core;
//         integration and long-duration tests deferred to Phase 01 (ROADMAP 01.07, 01.08).
//
//   VAELEN_CHECK(expr)          - fatal in builds with VAELEN_ASSERTS_ENABLED,
//                                 compiled out otherwise (expr NOT evaluated).
//   VAELEN_CHECKF(expr, fmt...) - same, with a printf-style message. The format
//                                 must be a string literal (enforced at compile time).
//   VAELEN_VERIFY(expr)         - expr is ALWAYS evaluated; fatal only when
//                                 assertions are enabled.
//   VAELEN_ENSURE(expr)         - non-fatal: reports every failing evaluation
//                                 and evaluates to the boolean result of expr
//                                 (expr is ALWAYS evaluated, exactly once).
//   VAELEN_UNREACHABLE()        - marks code that must never execute: reports
//                                 through the handler in EVERY build, then aborts.
//
// VAELEN_ASSERTS_ENABLED is normally defined by the build system (CMake option
// VAELEN_ENABLE_ASSERTS, VaelenCore.Build.cs from the target configuration).
// The fallback below mirrors Unreal's DO_CHECK (on except Shipping/Test) and,
// outside Unreal, the NDEBUG convention.
//
// The failure handler is pluggable so the Unreal module can route it to
// `check`/`ensure` and the headless tests can capture it. The default handler
// writes the failure to stderr unconditionally, logs it through LogCore, flushes
// the sinks and, for Check failures, calls std::abort() (SIGABRT).
#pragma once

#include "Vaelen/Core/CoreTypes.h"

#ifndef VAELEN_ASSERTS_ENABLED
#	if defined(VAELEN_UNREAL_BUILD)
#		if (defined(UE_BUILD_SHIPPING) && UE_BUILD_SHIPPING) || (defined(UE_BUILD_TEST) && UE_BUILD_TEST)
#			define VAELEN_ASSERTS_ENABLED 0
#		else
#			define VAELEN_ASSERTS_ENABLED 1
#		endif
#	elif defined(NDEBUG)
#		define VAELEN_ASSERTS_ENABLED 0
#	else
#		define VAELEN_ASSERTS_ENABLED 1
#	endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#	define VAELEN_LIKELY(x) __builtin_expect(!!(x), 1)
#	define VAELEN_UNLIKELY(x) __builtin_expect(!!(x), 0)
#	define VAELEN_PRINTF_ATTR(FmtIndex, ArgsIndex) __attribute__((format(printf, FmtIndex, ArgsIndex)))
#else
#	define VAELEN_LIKELY(x) (!!(x))
#	define VAELEN_UNLIKELY(x) (!!(x))
#	define VAELEN_PRINTF_ATTR(FmtIndex, ArgsIndex)
#endif

/// Compile-time proof that the first variadic argument is a string literal:
/// `"" Literal` only concatenates when Literal is a literal. Works on every
/// compiler, independent of -Wformat flags.
#define VAELEN_EXPAND(x) x
#define VAELEN_FIRST_ARG_(First, ...) First
#define VAELEN_FIRST_ARG(...) VAELEN_EXPAND(VAELEN_FIRST_ARG_(__VA_ARGS__, 0))
#define VAELEN_REQUIRE_LITERAL_FORMAT(...) ((void)sizeof("" VAELEN_FIRST_ARG(__VA_ARGS__)))

namespace Vaelen
{
	enum class AssertKind : uint8
	{
		Check,	///< Fatal: the program state is invalid.
		Ensure, ///< Recoverable: report and continue.
	};

	struct AssertInfo
	{
		AssertKind Kind = AssertKind::Check;
		const char* Expression = "";
		const char* File = "";
		int32 Line = 0;
		const char* Function = "";
		const char* Message = ""; ///< Formatted message; empty when none was given. Valid during the handler call only.
	};

	/// Called on every assertion failure. Returning from a Check handler is
	/// allowed (tests use this); the default handler reports and aborts.
	using AssertHandler = void (*)(const AssertInfo& Info, void* UserData);

	/// Installs a handler; passing nullptr restores the default. The handler
	/// and its user data are installed and read as one unit (thread-safe).
	VAELEN_CORE_API void SetAssertHandler(AssertHandler Handler, void* UserData = nullptr) noexcept;

	/// Returns the currently installed handler (nullptr when the default is
	/// active) and its user data, so that a caller can restore them later.
	VAELEN_CORE_API AssertHandler GetAssertHandler(void** OutUserData = nullptr) noexcept;

	/// Number of assertion failures reported since process start (all kinds).
	VAELEN_CORE_API uint64 GetAssertFailureCount() noexcept;

	namespace Detail
	{
		/// Reports a failure without a message. Compiled in every build.
		VAELEN_CORE_API void ReportAssert(AssertKind Kind, const char* Expression, const char* File, int32 Line,
										  const char* Function);

		/// Reports a failure with a printf-style message (truncated to 1023
		/// bytes, marked with "..." when truncated). Compiled in every build.
		VAELEN_CORE_API void ReportAssertF(AssertKind Kind, const char* Expression, const char* File, int32 Line,
										   const char* Function, const char* Format, ...) VAELEN_PRINTF_ATTR(6, 7);

		/// Flushes stdio and terminates with std::abort() (SIGABRT).
		[[noreturn]] VAELEN_CORE_API void AbortProcess() noexcept;

		/// Identity used by the compiled-out VAELEN_ENSURE so that a bare
		/// `VAELEN_ENSURE(x);` statement is not diagnosed as a statement with
		/// no effect (GCC -Wunused-value) when assertions are disabled.
		constexpr bool EnsureResult(bool Value) noexcept
		{
			return Value;
		}
	} // namespace Detail
} // namespace Vaelen

#if defined(_MSC_VER)
#	define VAELEN_FUNCTION __FUNCSIG__
#else
#	define VAELEN_FUNCTION __PRETTY_FUNCTION__
#endif

// The enabled forms are conditional expressions rather than `if` statements so
// that constant conditions (VAELEN_CHECK(false) in tests) do not trigger MSVC
// C4127 under /W4 /WX.
#if VAELEN_ASSERTS_ENABLED
#	define VAELEN_CHECK(Expr)                                                                                         \
		do                                                                                                             \
		{                                                                                                              \
			(VAELEN_LIKELY(Expr) ? void(0)                                                                             \
								 : ::Vaelen::Detail::ReportAssert(::Vaelen::AssertKind::Check, #Expr, __FILE__,        \
																  __LINE__, VAELEN_FUNCTION));                         \
		} while (false)
#	define VAELEN_CHECKF(Expr, ...)                                                                                   \
		do                                                                                                             \
		{                                                                                                              \
			VAELEN_REQUIRE_LITERAL_FORMAT(__VA_ARGS__);                                                                \
			(VAELEN_LIKELY(Expr) ? void(0)                                                                             \
								 : ::Vaelen::Detail::ReportAssertF(::Vaelen::AssertKind::Check, #Expr, __FILE__,       \
																   __LINE__, VAELEN_FUNCTION, __VA_ARGS__));           \
		} while (false)
#	define VAELEN_VERIFY(Expr) VAELEN_CHECK(Expr)
#	define VAELEN_ENSURE(Expr)                                                                                        \
		(VAELEN_LIKELY(Expr) ? true                                                                                    \
							 : (::Vaelen::Detail::ReportAssert(::Vaelen::AssertKind::Ensure, #Expr, __FILE__,          \
															   __LINE__, VAELEN_FUNCTION),                             \
								false))
#else
#	define VAELEN_CHECK(Expr) ((void)0)
#	define VAELEN_CHECKF(Expr, ...) ((void)0)
#	define VAELEN_VERIFY(Expr) ((void)(Expr))
#	define VAELEN_ENSURE(Expr) (::Vaelen::Detail::EnsureResult(!!(Expr)))
#endif

/// Reports through the installed handler in every build, then aborts.
#define VAELEN_UNREACHABLE()                                                                                           \
	do                                                                                                                 \
	{                                                                                                                  \
		::Vaelen::Detail::ReportAssert(::Vaelen::AssertKind::Check, "unreachable", __FILE__, __LINE__,                 \
									   VAELEN_FUNCTION);                                                               \
		::Vaelen::Detail::AbortProcess();                                                                              \
	} while (false)
