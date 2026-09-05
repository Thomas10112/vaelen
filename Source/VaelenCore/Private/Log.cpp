// VAELEN - VaelenCore
#include "Vaelen/Core/Log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace Vaelen
{
	LogCategory LogCore("LogCore", LogLevel::Trace);

	const char* LogLevelToString(LogLevel Level) noexcept
	{
		switch (Level)
		{
		case LogLevel::Trace:
			return "Trace";
		case LogLevel::Debug:
			return "Debug";
		case LogLevel::Info:
			return "Info";
		case LogLevel::Warning:
			return "Warning";
		case LogLevel::Error:
			return "Error";
		case LogLevel::Fatal:
			return "Fatal";
		case LogLevel::Off:
			return "Off";
		}
		return "Unknown";
	}

	LogCategory::LogCategory(const char* InName, LogLevel InMinLevel) noexcept : Name(InName), MinLevel(InMinLevel) {}

	void StdioLogSink::Write(const LogRecord& Record)
	{
		std::FILE* Stream = Record.Level >= LogLevel::Warning ? stderr : stdout;
		std::fprintf(Stream, "[%s] %s: %s\n", LogLevelToString(Record.Level),
					 Record.Category != nullptr ? Record.Category->Name : "?", Record.Message);
	}

	void StdioLogSink::Flush()
	{
		std::fflush(stdout);
		std::fflush(stderr);
	}

	namespace Log
	{
		namespace
		{
			constexpr usize MaxSinks = 8;

			std::atomic<LogLevel> GGlobalMinLevel{LogLevel::Trace};
			std::atomic<uint64> GDispatched{0};

			// Sinks are protected by a mutex: logging is rare relative to
			// simulation work and must never reorder simulation state, so a
			// simple lock is the robust choice.
			std::mutex& SinkMutex()
			{
				static std::mutex Mutex;
				return Mutex;
			}

			struct SinkTable
			{
				ILogSink* Sinks[MaxSinks] = {};
				usize Count = 0;
			};

			SinkTable& Sinks()
			{
				static SinkTable Table;
				return Table;
			}
		} // namespace

		void SetGlobalMinLevel(LogLevel Level) noexcept
		{
			GGlobalMinLevel.store(Level, std::memory_order_relaxed);
		}

		LogLevel GetGlobalMinLevel() noexcept
		{
			return GGlobalMinLevel.load(std::memory_order_relaxed);
		}

		bool AddSink(ILogSink* Sink) noexcept
		{
			if (Sink == nullptr)
			{
				return false;
			}
			std::lock_guard<std::mutex> Lock(SinkMutex());
			SinkTable& Table = Sinks();
			for (usize i = 0; i < Table.Count; ++i)
			{
				if (Table.Sinks[i] == Sink)
				{
					return true; // already registered
				}
			}
			if (Table.Count >= MaxSinks)
			{
				return false;
			}
			Table.Sinks[Table.Count++] = Sink;
			return true;
		}

		bool RemoveSink(ILogSink* Sink) noexcept
		{
			std::lock_guard<std::mutex> Lock(SinkMutex());
			SinkTable& Table = Sinks();
			for (usize i = 0; i < Table.Count; ++i)
			{
				if (Table.Sinks[i] == Sink)
				{
					for (usize j = i + 1; j < Table.Count; ++j)
					{
						Table.Sinks[j - 1] = Table.Sinks[j];
					}
					--Table.Count;
					Table.Sinks[Table.Count] = nullptr;
					return true;
				}
			}
			return false;
		}

		void RemoveAllSinks() noexcept
		{
			std::lock_guard<std::mutex> Lock(SinkMutex());
			Sinks() = SinkTable{};
		}

		void Flush()
		{
			std::lock_guard<std::mutex> Lock(SinkMutex());
			SinkTable& Table = Sinks();
			for (usize i = 0; i < Table.Count; ++i)
			{
				Table.Sinks[i]->Flush();
			}
		}

		uint64 GetDispatchedRecordCount() noexcept
		{
			return GDispatched.load(std::memory_order_relaxed);
		}

		void Write(const LogCategory& Category, LogLevel Level, const char* File, int32 Line, const char* Format, ...)
		{
			char Buffer[2048];
			std::va_list Args;
			va_start(Args, Format);
			const int Written = std::vsnprintf(Buffer, sizeof(Buffer), Format, Args);
			va_end(Args);
			if (Written < 0)
			{
				Buffer[0] = '\0';
			}

			LogRecord Record;
			Record.Category = &Category;
			Record.Level = Level;
			Record.Message = Buffer;
			Record.File = File;
			Record.Line = Line;

			std::lock_guard<std::mutex> Lock(SinkMutex());
			GDispatched.fetch_add(1, std::memory_order_relaxed);
			SinkTable& Table = Sinks();
			for (usize i = 0; i < Table.Count; ++i)
			{
				Table.Sinks[i]->Write(Record);
			}
		}
	} // namespace Log
} // namespace Vaelen
