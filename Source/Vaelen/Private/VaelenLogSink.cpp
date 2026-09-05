// VAELEN - primary game module.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
#include "VaelenLogSink.h"
#include "Vaelen.h"

void FVaelenLogSink::Write(const Vaelen::LogRecord& Record)
{
	// Category names are ASCII literals; messages are UTF-8 (kernel strings
	// are formatted with vsnprintf and may carry generated names).
	const FString Category = ANSI_TO_TCHAR(Record.Category ? Record.Category->Name : "?");
	const FString Message = UTF8_TO_TCHAR(Record.Message);

	switch (Record.Level)
	{
	case Vaelen::LogLevel::Trace:
		UE_LOG(LogVaelen, VeryVerbose, TEXT("[%s] %s"), *Category, *Message);
		break;
	case Vaelen::LogLevel::Debug:
		UE_LOG(LogVaelen, Verbose, TEXT("[%s] %s"), *Category, *Message);
		break;
	case Vaelen::LogLevel::Info:
		UE_LOG(LogVaelen, Log, TEXT("[%s] %s"), *Category, *Message);
		break;
	case Vaelen::LogLevel::Warning:
		UE_LOG(LogVaelen, Warning, TEXT("[%s] %s"), *Category, *Message);
		break;
	case Vaelen::LogLevel::Error:
		UE_LOG(LogVaelen, Error, TEXT("[%s] %s"), *Category, *Message);
		break;
	case Vaelen::LogLevel::Fatal:
		// Kernel Fatal is reported, not crashed here: the kernel's own
		// assertion handler decides whether the process must stop.
		UE_LOG(LogVaelen, Error, TEXT("[FATAL][%s] %s"), *Category, *Message);
		break;
	case Vaelen::LogLevel::Off:
		break;
	}
}

void FVaelenLogSink::Flush()
{
	if (GLog)
	{
		GLog->Flush();
	}
}
