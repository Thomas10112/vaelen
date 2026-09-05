// VAELEN - primary game module.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
#include "Vaelen.h"
#include "VaelenLogSink.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Version.h"

DEFINE_LOG_CATEGORY(LogVaelen);

namespace
{
	void VaelenUnrealAssertHandler(const Vaelen::AssertInfo& Info, void*)
	{
		const FString Expr = ANSI_TO_TCHAR(Info.Expression);
		const FString Msg = ANSI_TO_TCHAR(Info.Message);
		const FString File = ANSI_TO_TCHAR(Info.File);

		if (Info.Kind == Vaelen::AssertKind::Ensure)
		{
			ensureAlwaysMsgf(false, TEXT("VAELEN ensure failed: %s %s (%s:%d)"), *Expr, *Msg, *File, Info.Line);
			return;
		}
		UE_LOG(LogVaelen, Fatal, TEXT("VAELEN check failed: %s %s (%s:%d)"), *Expr, *Msg, *File, Info.Line);
	}
} // namespace

void FVaelenModule::StartupModule()
{
	LogSink = MakeUnique<FVaelenLogSink>();
	Vaelen::Log::AddSink(LogSink.Get());
	Vaelen::SetAssertHandler(&VaelenUnrealAssertHandler);

	UE_LOG(LogVaelen, Log, TEXT("VAELEN %s - kernel save format v%u - module started"),
		   ANSI_TO_TCHAR(Vaelen::GetProjectVersionString()), Vaelen::GetSaveFormatVersion());
}

void FVaelenModule::ShutdownModule()
{
	Vaelen::SetAssertHandler(nullptr);
	if (LogSink)
	{
		Vaelen::Log::RemoveSink(LogSink.Get());
		LogSink.Reset();
	}
}

IMPLEMENT_PRIMARY_GAME_MODULE(FVaelenModule, Vaelen, "Vaelen");
