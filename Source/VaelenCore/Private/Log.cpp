// VAELEN - VaelenCore
// Logging implementation: level names, the stdout/stderr sink and the
// mutex-protected sink table behind Vaelen::Log (see Log.h).
//
// STATUS: VALIDATED (Phase 00) - covered by Tests/Core/Test_Log.cpp
#include "Vaelen/Core/Log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
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

	StdioLogSink::StdioLogSink(std::FILE* InOut, std::FILE* InErr) noexcept : Out(InOut), Err(InErr) {}

	void StdioLogSink::Write(const LogRecord& Record)
	{
		std::FILE* Stream = Record.Level >= LogLevel::Warning ? Err : Out;
		std::fprintf(Stream, "[%s] %s: %s\n", LogLevelToString(Record.Level),
					 Record.Category != nullptr ? Record.Category->Name : "?", Record.Message);
	}

	void StdioLogSink::Flush()
	{
		std::fflush(Out);
		std::fflush(Err);
	}

	namespace Log
	{
		namespace
		{
			constexpr usize MaxSinks = 8;

			std::atomic<LogLevel> GGlobalMinLevel{LogLevel::Trace};
			std::atomic<uint64> GDispatched{0};

			// Sinks are protected by a recursive mutex: logging is rare relative
			// to simulation work and must never reorder simulation state, so a
			// simple lock is the robust choice. Recursive so that a sink (or the
			// default assertion handler running inside a sink) may log, flush or
			// edit the sink table without deadlocking the calling thread.
			std::recursive_mutex& SinkMutex()
			{
				static std::recursive_mutex Mutex;
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
			std::lock_guard<std::recursive_mutex> Lock(SinkMutex());
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
			std::lock_guard<std::recursive_mutex> Lock(SinkMutex());
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
			std::lock_guard<std::recursive_mutex> Lock(SinkMutex());
			Sinks() = SinkTable{};
		}

		usize GetSinkCount() noexcept
		{
			std::lock_guard<std::recursive_mutex> Lock(SinkMutex());
			return Sinks().Count;
		}

		void Flush()
		{
			std::lock_guard<std::recursive_mutex> Lock(SinkMutex());
			// Snapshot: a sink may add or remove sinks while being called.
			const SinkTable Snapshot = Sinks();
			for (usize i = 0; i < Snapshot.Count; ++i)
			{
				Snapshot.Sinks[i]->Flush();
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
			else if (static_cast<usize>(Written) >= sizeof(Buffer))
			{
				// Truncated: make the cut visible.
				std::memcpy(Buffer + sizeof(Buffer) - 4, "...", 4);
			}

			LogRecord Record;
			Record.Category = &Category;
			Record.Level = Level;
			Record.Message = Buffer;
			Record.File = File;
			Record.Line = Line;

			std::lock_guard<std::recursive_mutex> Lock(SinkMutex());
			GDispatched.fetch_add(1, std::memory_order_relaxed);
			// Snapshot: a sink may add or remove sinks (itself included) while
			// being called; nested Log::Write from a sink is allowed (recursive lock).
			const SinkTable Snapshot = Sinks();
			for (usize i = 0; i < Snapshot.Count; ++i)
			{
				Snapshot.Sinks[i]->Write(Record);
			}
		}
	} // namespace Log
} // namespace Vaelen
