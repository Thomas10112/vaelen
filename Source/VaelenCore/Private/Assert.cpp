// VAELEN - VaelenCore
// Assertion reporting: pluggable handler, failure counter, default report-and-abort.
//
// STATUS: VALIDATED (Phase 00) - covered by Tests/Core/Test_Assert.cpp
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace Vaelen
{
	namespace
	{
		struct HandlerSlot
		{
			AssertHandler Handler = nullptr;
			void* UserData = nullptr;
		};

		// The (handler, user data) pair is installed and read under one lock so
		// a concurrent SetAssertHandler can never deliver a torn pair.
		std::mutex& HandlerMutex()
		{
			static std::mutex Mutex;
			return Mutex;
		}

		HandlerSlot& Slot()
		{
			static HandlerSlot Current;
			return Current;
		}

		std::atomic<uint64> GFailureCount{0};

		constexpr usize MessageBufferSize = 1024;

		/// Marks a truncated snprintf result so that a cut line is recognisable.
		void MarkTruncated(char* Buffer, usize BufferSize, int Written)
		{
			if (Written >= 0 && static_cast<usize>(Written) >= BufferSize && BufferSize >= 4)
			{
				std::memcpy(Buffer + BufferSize - 4, "...", 4);
			}
		}

		void DefaultHandler(const AssertInfo& Info)
		{
			const char* KindName = Info.Kind == AssertKind::Check ? "CHECK" : "ENSURE";

			// Unconditional stderr trace: independent of sinks and level filters,
			// so a failure is never silent. Location first, message last.
			std::fprintf(stderr, "VAELEN %s failed at %s:%d (%s): %s%s%s\n", KindName, Info.File, Info.Line,
						 Info.Function, Info.Expression, Info.Message[0] != '\0' ? " -- " : "", Info.Message);
			std::fflush(stderr);

			VAELEN_LOG_ERROR(LogCore, "%s failed at %s:%d (%s): %s%s%s", KindName, Info.File, Info.Line, Info.Function,
							 Info.Expression, Info.Message[0] != '\0' ? " -- " : "", Info.Message);
			Log::Flush();

			if (Info.Kind == AssertKind::Check)
			{
				Detail::AbortProcess();
			}
		}
	} // namespace

	void SetAssertHandler(AssertHandler Handler, void* UserData) noexcept
	{
		std::lock_guard<std::mutex> Lock(HandlerMutex());
		Slot().Handler = Handler;
		Slot().UserData = UserData;
	}

	AssertHandler GetAssertHandler(void** OutUserData) noexcept
	{
		std::lock_guard<std::mutex> Lock(HandlerMutex());
		if (OutUserData != nullptr)
		{
			*OutUserData = Slot().UserData;
		}
		return Slot().Handler;
	}

	uint64 GetAssertFailureCount() noexcept
	{
		return GFailureCount.load(std::memory_order_relaxed);
	}

	namespace Detail
	{
		namespace
		{
			void Dispatch(AssertKind Kind, const char* Expression, const char* File, int32 Line, const char* Function,
						  const char* Message)
			{
				GFailureCount.fetch_add(1, std::memory_order_relaxed);

				AssertInfo Info;
				Info.Kind = Kind;
				Info.Expression = Expression;
				Info.File = File;
				Info.Line = Line;
				Info.Function = Function;
				Info.Message = Message;

				HandlerSlot Installed;
				{
					std::lock_guard<std::mutex> Lock(HandlerMutex());
					Installed = Slot();
				}
				if (Installed.Handler != nullptr)
				{
					Installed.Handler(Info, Installed.UserData);
				}
				else
				{
					DefaultHandler(Info);
				}
			}
		} // namespace

		void ReportAssert(AssertKind Kind, const char* Expression, const char* File, int32 Line, const char* Function)
		{
			Dispatch(Kind, Expression, File, Line, Function, "");
		}

		void ReportAssertF(AssertKind Kind, const char* Expression, const char* File, int32 Line, const char* Function,
						   const char* Format, ...)
		{
			char MessageBuffer[MessageBufferSize];
			MessageBuffer[0] = '\0';
			std::va_list Args;
			va_start(Args, Format);
			const int Written = std::vsnprintf(MessageBuffer, sizeof(MessageBuffer), Format, Args);
			va_end(Args);
			if (Written < 0)
			{
				// Encoding error: the buffer content is unspecified, never hand
				// it to the handler.
				MessageBuffer[0] = '\0';
			}
			MarkTruncated(MessageBuffer, sizeof(MessageBuffer), Written);
			Dispatch(Kind, Expression, File, Line, Function, MessageBuffer);
		}

		void AbortProcess() noexcept
		{
			std::fflush(stdout);
			std::fflush(stderr);
			std::abort();
		}
	} // namespace Detail
} // namespace Vaelen
