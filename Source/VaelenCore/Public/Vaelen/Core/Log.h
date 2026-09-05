// VAELEN - VaelenCore
// Structured, category-based logging with pluggable sinks.
//
// STATUS: VALIDATED (Phase 00) - unit/deterministic/edge tests in Tests/Core;
//         integration and long-duration tests deferred to Phase 01 (ROADMAP 01.07, 01.08).
//
// Design:
//   - printf-style formatting (portable across MSVC, GCC, Clang and the
//     libc++ bundled with Unreal's Linux toolchain; std::format is not yet
//     guaranteed there).
//   - Categories are static objects: `VAELEN_DECLARE_LOG_CATEGORY(LogEconomy)`
//     in a header, `VAELEN_DEFINE_LOG_CATEGORY(LogEconomy)` in one .cpp.
//   - Sinks receive fully formatted records. No sink is installed by default:
//     the test runner installs StdioLogSink with --verbose, the Unreal module
//     installs a UE_LOG sink at startup.
//   - Logging is NOT part of the simulation state: it must never influence
//     determinism. A sink may run on any thread; sinks are called under a
//     recursive mutex so a sink implementation need not be thread-safe itself,
//     and a sink may itself log, flush or edit the sink table (records
//     produced by a sink are delivered nested, on the same thread).
//   - Formats must be string literals; the macros enforce it at compile time
//     on every compiler (no reliance on -Wformat). Messages are truncated to
//     2047 bytes and marked with a trailing "..." when cut.
#pragma once

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/CoreTypes.h"

#include <atomic>
#include <cstdio>

namespace Vaelen
{
	enum class LogLevel : uint8
	{
		Trace = 0,
		Debug = 1,
		Info = 2,
		Warning = 3,
		Error = 4,
		Fatal = 5,
		Off = 6,
	};

	VAELEN_CORE_API const char* LogLevelToString(LogLevel Level) noexcept;

	/// A named log category with its own runtime verbosity threshold. The
	/// threshold is atomic: it may be changed from any thread at any time.
	/// Categories are constant-initialised (constexpr constructor), so they
	/// are usable during static initialisation of other objects.
	struct VAELEN_CORE_API LogCategory
	{
		constexpr explicit LogCategory(const char* InName, LogLevel InMinLevel = LogLevel::Info) noexcept
			: Name(InName), MinLevel(InMinLevel)
		{
		}

		const char* Name;
		std::atomic<LogLevel> MinLevel;

		bool IsEnabled(LogLevel Level) const noexcept { return Level >= MinLevel.load(std::memory_order_relaxed); }
		void SetMinLevel(LogLevel Level) noexcept { MinLevel.store(Level, std::memory_order_relaxed); }
		LogLevel GetMinLevel() const noexcept { return MinLevel.load(std::memory_order_relaxed); }
	};

	/// One formatted record. Message points into a buffer owned by Log::Write
	/// and Category may point to a caller-owned category: both are valid only
	/// for the duration of ILogSink::Write(); a sink that keeps a record must copy.
	struct LogRecord
	{
		const LogCategory* Category = nullptr;
		LogLevel Level = LogLevel::Info;
		const char* Message = "";
		const char* File = "";
		int32 Line = 0;
	};

	/// Receives formatted log records. Implementations are owned by the caller.
	class VAELEN_CORE_API ILogSink
	{
	public:
		virtual ~ILogSink() = default;
		virtual void Write(const LogRecord& Record) = 0;
		virtual void Flush() {}
	};

	/// Simple sink writing "[Level] Category: message" to two C streams:
	/// Warning and above go to Err, everything else to Out (stdout/stderr by
	/// default). The streams are not owned.
	class VAELEN_CORE_API StdioLogSink final : public ILogSink
	{
	public:
		explicit StdioLogSink(std::FILE* InOut = stdout, std::FILE* InErr = stderr) noexcept;
		void Write(const LogRecord& Record) override;
		void Flush() override;

	private:
		std::FILE* Out;
		std::FILE* Err;
	};

	namespace Log
	{
		/// Global minimum level applied before category thresholds.
		VAELEN_CORE_API void SetGlobalMinLevel(LogLevel Level) noexcept;
		VAELEN_CORE_API LogLevel GetGlobalMinLevel() noexcept;

		/// Sinks are not owned. A sink must be removed before it is destroyed.
		/// Returns false when the sink table is full (max 8 sinks).
		VAELEN_CORE_API bool AddSink(ILogSink* Sink) noexcept;
		VAELEN_CORE_API bool RemoveSink(ILogSink* Sink) noexcept;
		VAELEN_CORE_API void RemoveAllSinks() noexcept;
		/// Number of currently registered sinks.
		VAELEN_CORE_API usize GetSinkCount() noexcept;
		VAELEN_CORE_API void Flush();

		/// Total number of records dispatched to sinks since process start.
		VAELEN_CORE_API uint64 GetDispatchedRecordCount() noexcept;

		/// Formats and dispatches one record. Prefer the VAELEN_LOG_* macros.
		VAELEN_CORE_API void Write(const LogCategory& Category, LogLevel Level, const char* File, int32 Line,
								   const char* Format, ...) VAELEN_PRINTF_ATTR(5, 6);
	} // namespace Log

	/// Kernel-wide categories. Domain modules declare their own.
	VAELEN_CORE_API extern LogCategory LogCore;
} // namespace Vaelen

#define VAELEN_DECLARE_LOG_CATEGORY(CategoryName) extern ::Vaelen::LogCategory CategoryName
#define VAELEN_DEFINE_LOG_CATEGORY(CategoryName) ::Vaelen::LogCategory CategoryName(#CategoryName)
#define VAELEN_DEFINE_LOG_CATEGORY_LEVEL(CategoryName, MinLevel)                                                       \
	::Vaelen::LogCategory CategoryName(#CategoryName, ::Vaelen::LogLevel::MinLevel)

/// Compile-time floor: records below this level are removed from the binary
/// (no dispatch, arguments not evaluated). Normally defined by the build
/// system (VaelenCore.Build.cs: 2 in Shipping, else 0); fallback below.
#ifndef VAELEN_LOG_COMPILED_MIN_LEVEL
#	if defined(VAELEN_UNREAL_BUILD) && defined(UE_BUILD_SHIPPING) && UE_BUILD_SHIPPING
#		define VAELEN_LOG_COMPILED_MIN_LEVEL 2 /* Info */
#	else
#		define VAELEN_LOG_COMPILED_MIN_LEVEL 0 /* Trace */
#	endif
#endif

#define VAELEN_LOG_IMPL(Category, Level, LevelValue, ...)                                                              \
	do                                                                                                                 \
	{                                                                                                                  \
		if constexpr ((LevelValue) >= VAELEN_LOG_COMPILED_MIN_LEVEL)                                                   \
		{                                                                                                              \
			VAELEN_REQUIRE_LITERAL_FORMAT(__VA_ARGS__);                                                                \
			if ((Category).IsEnabled(::Vaelen::LogLevel::Level) &&                                                     \
				::Vaelen::LogLevel::Level >= ::Vaelen::Log::GetGlobalMinLevel())                                       \
			{                                                                                                          \
				::Vaelen::Log::Write((Category), ::Vaelen::LogLevel::Level, __FILE__, __LINE__, __VA_ARGS__);          \
			}                                                                                                          \
		}                                                                                                              \
	} while (false)

#define VAELEN_LOG_TRACE(Category, ...) VAELEN_LOG_IMPL(Category, Trace, 0, __VA_ARGS__)
#define VAELEN_LOG_DEBUG(Category, ...) VAELEN_LOG_IMPL(Category, Debug, 1, __VA_ARGS__)
#define VAELEN_LOG_INFO(Category, ...) VAELEN_LOG_IMPL(Category, Info, 2, __VA_ARGS__)
#define VAELEN_LOG_WARNING(Category, ...) VAELEN_LOG_IMPL(Category, Warning, 3, __VA_ARGS__)
#define VAELEN_LOG_ERROR(Category, ...) VAELEN_LOG_IMPL(Category, Error, 4, __VA_ARGS__)
#define VAELEN_LOG_FATAL(Category, ...) VAELEN_LOG_IMPL(Category, Fatal, 5, __VA_ARGS__)
