// VAELEN - VaelenCore
// Assertions for a kernel built without exceptions.
//
// STATUS: VALIDATED (Phase 00)
//
//   VAELEN_CHECK(expr)          - fatal in builds with VAELEN_ASSERTS_ENABLED,
//                                 compiled out otherwise (expr NOT evaluated).
//   VAELEN_CHECKF(expr, fmt...) - same, with a printf-style message.
//   VAELEN_VERIFY(expr)         - expr is ALWAYS evaluated; fatal only when
//                                 assertions are enabled.
//   VAELEN_ENSURE(expr)         - non-fatal: reports once per call site and
//                                 evaluates to the boolean result of expr.
//   VAELEN_UNREACHABLE()        - marks code that must never execute.
//
// The failure handler is pluggable so the Unreal module can route it to
// `check`/`ensure` and the headless tests can capture it.
#pragma once

#include "Vaelen/Core/CoreTypes.h"

#ifndef VAELEN_ASSERTS_ENABLED
#	if defined(VAELEN_UNREAL_BUILD) && defined(UE_BUILD_SHIPPING) && UE_BUILD_SHIPPING
#		define VAELEN_ASSERTS_ENABLED 0
#	elif defined(NDEBUG)
#		define VAELEN_ASSERTS_ENABLED 0
#	else
#		define VAELEN_ASSERTS_ENABLED 1
#	endif
#endif

#if defined(_MSC_VER)
#	define VAELEN_DEBUG_BREAK() __debugbreak()
#elif defined(__has_builtin)
#	if __has_builtin(__builtin_trap)
#		define VAELEN_DEBUG_BREAK() __builtin_trap()
#	else
#		define VAELEN_DEBUG_BREAK() ((void)0)
#	endif
#else
#	define VAELEN_DEBUG_BREAK() ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#	define VAELEN_LIKELY(x) __builtin_expect(!!(x), 1)
#	define VAELEN_UNLIKELY(x) __builtin_expect(!!(x), 0)
#	define VAELEN_PRINTF_ATTR(FmtIndex, ArgsIndex) __attribute__((format(printf, FmtIndex, ArgsIndex)))
#else
#	define VAELEN_LIKELY(x) (x)
#	define VAELEN_UNLIKELY(x) (x)
#	define VAELEN_PRINTF_ATTR(FmtIndex, ArgsIndex)
#endif

namespace Vaelen
{
	enum class AssertKind : uint8
	{
		Check,	 ///< Fatal: the program state is invalid.
		Ensure,	 ///< Recoverable: report and continue.
	};

	struct AssertInfo
	{
		AssertKind Kind = AssertKind::Check;
		const char* Expression = "";
		const char* File = "";
		int32 Line = 0;
		const char* Function = "";
		const char* Message = ""; ///< Formatted message; empty when none was given.
	};

	/// Called on every assertion failure. Returning from a Check handler is
	/// allowed (tests use this); the default handler logs and aborts.
	using AssertHandler = void (*)(const AssertInfo& Info, void* UserData);

	/// Installs a handler; passing nullptr restores the default (log + abort).
	VAELENCORE_API void SetAssertHandler(AssertHandler Handler, void* UserData = nullptr) noexcept;

	/// Number of assertion failures reported since process start (all kinds).
	VAELENCORE_API uint64 GetAssertFailureCount() noexcept;

	namespace Detail
	{
		VAELENCORE_API void ReportAssert(AssertKind Kind, const char* Expression, const char* File, int32 Line,
										 const char* Function, const char* Format, ...) VAELEN_PRINTF_ATTR(6, 7);

		[[noreturn]] VAELENCORE_API void AbortProcess() noexcept;
	} // namespace Detail
} // namespace Vaelen

#if defined(_MSC_VER)
#	define VAELEN_FUNCTION __FUNCSIG__
#else
#	define VAELEN_FUNCTION __PRETTY_FUNCTION__
#endif

#if VAELEN_ASSERTS_ENABLED
#	define VAELEN_CHECK(Expr)                                                                                         \
		do                                                                                                             \
		{                                                                                                              \
			if (VAELEN_UNLIKELY(!(Expr)))                                                                              \
			{                                                                                                          \
				::Vaelen::Detail::ReportAssert(::Vaelen::AssertKind::Check, #Expr, __FILE__, __LINE__,                 \
											   VAELEN_FUNCTION, "");                                                   \
			}                                                                                                          \
		} while (false)
#	define VAELEN_CHECKF(Expr, ...)                                                                                   \
		do                                                                                                             \
		{                                                                                                              \
			if (VAELEN_UNLIKELY(!(Expr)))                                                                              \
			{                                                                                                          \
				::Vaelen::Detail::ReportAssert(::Vaelen::AssertKind::Check, #Expr, __FILE__, __LINE__,                 \
											   VAELEN_FUNCTION, __VA_ARGS__);                                          \
			}                                                                                                          \
		} while (false)
#	define VAELEN_VERIFY(Expr) VAELEN_CHECK(Expr)
#	define VAELEN_ENSURE(Expr)                                                                                        \
		(VAELEN_LIKELY(!!(Expr)) ? true                                                                                \
								 : (::Vaelen::Detail::ReportAssert(::Vaelen::AssertKind::Ensure, #Expr, __FILE__,      \
																   __LINE__, VAELEN_FUNCTION, ""),                     \
									false))
#	define VAELEN_UNREACHABLE()                                                                                       \
		do                                                                                                             \
		{                                                                                                              \
			::Vaelen::Detail::ReportAssert(::Vaelen::AssertKind::Check, "unreachable", __FILE__, __LINE__,             \
										   VAELEN_FUNCTION, "");                                                       \
			::Vaelen::Detail::AbortProcess();                                                                          \
		} while (false)
#else
#	define VAELEN_CHECK(Expr) ((void)0)
#	define VAELEN_CHECKF(Expr, ...) ((void)0)
#	define VAELEN_VERIFY(Expr) ((void)(Expr))
#	define VAELEN_ENSURE(Expr) (!!(Expr))
#	define VAELEN_UNREACHABLE() ::Vaelen::Detail::AbortProcess()
#endif
