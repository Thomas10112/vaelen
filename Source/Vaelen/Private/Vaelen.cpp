// VAELEN - primary game module.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
#include "Vaelen.h"
#include "VaelenLogSink.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Version.h"

#include "Containers/Set.h"
#include "HAL/CriticalSection.h"
#include "Logging/LogVerbosity.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY(LogVaelen);

// The kernel's 64-bit types must be the engine's 64-bit types (CoreTypes.h).
static_assert(std::is_same_v<Vaelen::int64, ::int64>, "Vaelen::int64 must be Unreal's int64");
static_assert(std::is_same_v<Vaelen::uint64, ::uint64>, "Vaelen::uint64 must be Unreal's uint64");
static_assert(std::is_same_v<Vaelen::uint32, ::uint32>, "Vaelen::uint32 must be Unreal's uint32");

namespace
{
	// Kernel ensures report every failing evaluation; the engine's ensure walks
	// the stack and may file a report each time. Report the first occurrence of
	// each kernel call site through ensureAlwaysMsgf and later ones as warnings.
	FCriticalSection GReportedSitesLock;
	TSet<uint64> GReportedSites;

	bool IsFirstReportOfSite(const Vaelen::AssertInfo& Info)
	{
		const uint64 Site = Vaelen::HashCombine(Vaelen::HashString(Info.File), static_cast<uint64>(Info.Line));
		FScopeLock Lock(&GReportedSitesLock);
		bool bAlreadyReported = false;
		GReportedSites.Add(Site, &bAlreadyReported);
		return !bAlreadyReported;
	}

	void VaelenUnrealAssertHandler(const Vaelen::AssertInfo& Info, void*)
	{
		const FString Expr = UTF8_TO_TCHAR(Info.Expression);
		const FString Msg = UTF8_TO_TCHAR(Info.Message);
		const FString File = ANSI_TO_TCHAR(Info.File);

		if (Info.Kind == Vaelen::AssertKind::Ensure)
		{
			if (IsFirstReportOfSite(Info))
			{
				ensureAlwaysMsgf(false, TEXT("VAELEN ensure failed: %s %s (%s:%d)"), *Expr, *Msg, *File, Info.Line);
			}
			else
			{
				UE_LOG(LogVaelen, Warning, TEXT("VAELEN ensure failed (repeat): %s %s (%s:%d)"), *Expr, *Msg, *File,
					   Info.Line);
			}
			return;
		}
		UE_LOG(LogVaelen, Fatal, TEXT("VAELEN check failed: %s %s (%s:%d)"), *Expr, *Msg, *File, Info.Line);
	}

	/// Kernel records below the LogVaelen verbosity would be formatted and
	/// dispatched only to be dropped by UE_LOG; align the kernel floor.
	void SyncKernelLogFloor()
	{
		const ELogVerbosity::Type Verbosity = LogVaelen.GetVerbosity();
		Vaelen::LogLevel Floor = Vaelen::LogLevel::Info;
		if (Verbosity >= ELogVerbosity::VeryVerbose)
		{
			Floor = Vaelen::LogLevel::Trace;
		}
		else if (Verbosity >= ELogVerbosity::Verbose)
		{
			Floor = Vaelen::LogLevel::Debug;
		}
		Vaelen::Log::SetGlobalMinLevel(Floor);
	}
} // namespace

void FVaelenModule::StartupModule()
{
	LogSink = MakeUnique<FVaelenLogSink>();
	Vaelen::Log::AddSink(LogSink.Get());
	SyncKernelLogFloor();
	Vaelen::SetAssertHandler(&VaelenUnrealAssertHandler);

	UE_LOG(LogVaelen, Log, TEXT("VAELEN %s - kernel save format v%u - kernel asserts %s - module started"),
		   ANSI_TO_TCHAR(Vaelen::GetProjectVersionString()), Vaelen::GetSaveFormatVersion(),
		   VAELEN_ASSERTS_ENABLED ? TEXT("on") : TEXT("off"));
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
