// VAELEN - VaelenCore
// Assertion reporting: pluggable handler, failure counter, default log-and-abort.
//
// STATUS: VALIDATED (Phase 00) - covered by Tests/Core/Test_Assert.cpp
#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace Vaelen
{
	namespace
	{
		std::atomic<AssertHandler> GHandler{nullptr};
		std::atomic<void*> GHandlerUserData{nullptr};
		std::atomic<uint64> GFailureCount{0};

		void DefaultHandler(const AssertInfo& Info, void*)
		{
			const char* KindName = Info.Kind == AssertKind::Check ? "CHECK" : "ENSURE";
			if (Info.Message[0] != '\0')
			{
				VAELEN_LOG_ERROR(LogCore, "%s failed: %s (%s) at %s:%d in %s", KindName, Info.Expression, Info.Message,
								 Info.File, Info.Line, Info.Function);
			}
			else
			{
				VAELEN_LOG_ERROR(LogCore, "%s failed: %s at %s:%d in %s", KindName, Info.Expression, Info.File,
								 Info.Line, Info.Function);
			}
			Log::Flush();

			if (Info.Kind == AssertKind::Check)
			{
				Detail::AbortProcess();
			}
		}
	} // namespace

	void SetAssertHandler(AssertHandler Handler, void* UserData) noexcept
	{
		GHandlerUserData.store(UserData, std::memory_order_relaxed);
		GHandler.store(Handler, std::memory_order_release);
	}

	AssertHandler GetAssertHandler(void** OutUserData) noexcept
	{
		AssertHandler Handler = GHandler.load(std::memory_order_acquire);
		if (OutUserData != nullptr)
		{
			*OutUserData = GHandlerUserData.load(std::memory_order_relaxed);
		}
		return Handler;
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

				AssertHandler Handler = GHandler.load(std::memory_order_acquire);
				void* UserData = GHandlerUserData.load(std::memory_order_relaxed);
				if (Handler != nullptr)
				{
					Handler(Info, UserData);
				}
				else
				{
					DefaultHandler(Info, nullptr);
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
			char MessageBuffer[1024];
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
			Dispatch(Kind, Expression, File, Line, Function, MessageBuffer);
		}

		void AbortProcess() noexcept
		{
			std::fflush(stdout);
			std::fflush(stderr);
			VAELEN_DEBUG_BREAK();
			std::abort();
		}
	} // namespace Detail
} // namespace Vaelen
