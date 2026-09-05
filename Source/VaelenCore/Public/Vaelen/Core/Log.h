// VAELEN - VaelenCore
// Structured, category-based logging with pluggable sinks.
//
// STATUS: VALIDATED (Phase 00)
//
// Design:
//   - printf-style formatting (portable across MSVC, GCC, Clang and the
//     libc++ bundled with Unreal's Linux toolchain; std::format is not yet
//     guaranteed there).
//   - Categories are static objects: `VAELEN_DECLARE_LOG_CATEGORY(LogEconomy)`
//     in a header, `VAELEN_DEFINE_LOG_CATEGORY(LogEconomy)` in one .cpp.
//   - Sinks receive fully formatted records. The headless build installs a
//     stdout sink; the Unreal module installs a UE_LOG sink.
//   - Logging is NOT part of the simulation state: it must never influence
//     determinism. A sink may run on any thread; sinks are called under a
//     mutex so a sink implementation need not be thread-safe itself.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Assert.h"

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

	VAELENCORE_API const char* LogLevelToString(LogLevel Level) noexcept;

	/// A named log category with its own runtime verbosity threshold.
	struct VAELENCORE_API LogCategory
	{
		explicit LogCategory(const char* InName, LogLevel InMinLevel = LogLevel::Info) noexcept;

		const char* Name;
		LogLevel MinLevel;

		bool IsEnabled(LogLevel Level) const noexcept { return Level >= MinLevel; }
	};

	struct LogRecord
	{
		const LogCategory* Category = nullptr;
		LogLevel Level = LogLevel::Info;
		const char* Message = "";
		const char* File = "";
		int32 Line = 0;
	};

	/// Receives formatted log records. Implementations are owned by the caller.
	class VAELENCORE_API ILogSink
	{
	public:
		virtual ~ILogSink() = default;
		virtual void Write(const LogRecord& Record) = 0;
		virtual void Flush() {}
	};

	/// Simple sink writing "[Level] Category: message" to stdout / stderr.
	class VAELENCORE_API StdioLogSink final : public ILogSink
	{
	public:
		void Write(const LogRecord& Record) override;
		void Flush() override;
	};

	namespace Log
	{
		/// Global minimum level applied before category thresholds.
		VAELENCORE_API void SetGlobalMinLevel(LogLevel Level) noexcept;
		VAELENCORE_API LogLevel GetGlobalMinLevel() noexcept;

		/// Sinks are not owned. A sink must be removed before it is destroyed.
		/// Returns false when the sink table is full (max 8 sinks).
		VAELENCORE_API bool AddSink(ILogSink* Sink) noexcept;
		VAELENCORE_API bool RemoveSink(ILogSink* Sink) noexcept;
		VAELENCORE_API void RemoveAllSinks() noexcept;
		VAELENCORE_API void Flush();

		/// Total number of records dispatched to sinks since process start.
		VAELENCORE_API uint64 GetDispatchedRecordCount() noexcept;

		/// Formats and dispatches one record. Prefer the VAELEN_LOG_* macros.
		VAELENCORE_API void Write(const LogCategory& Category, LogLevel Level, const char* File, int32 Line,
								  const char* Format, ...) VAELEN_PRINTF_ATTR(5, 6);
	} // namespace Log

	/// Kernel-wide categories. Domain modules declare their own.
	VAELENCORE_API extern LogCategory LogCore;
} // namespace Vaelen

#define VAELEN_DECLARE_LOG_CATEGORY(CategoryName) extern ::Vaelen::LogCategory CategoryName
#define VAELEN_DEFINE_LOG_CATEGORY(CategoryName) ::Vaelen::LogCategory CategoryName(#CategoryName)
#define VAELEN_DEFINE_LOG_CATEGORY_LEVEL(CategoryName, MinLevel)                                                       \
	::Vaelen::LogCategory CategoryName(#CategoryName, ::Vaelen::LogLevel::MinLevel)

/// Compile-time floor: records below this level are removed from the binary.
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
