// VAELEN - VaelenCore tests
// Logging: delivery of formatted records to sinks (category, level, message,
// file, line), category and global verbosity thresholds, sink table
// management (null, duplicate, capacity, removal), the dispatch counter,
// message truncation, LogLevel names, LogCore defaults and a multi-threaded
// dispatch smoke test.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace Vaelen;

namespace
{
	// Documented capacity of the sink table (Log.h: "max 8 sinks").
	constexpr usize MaxSinks = 8;

	// Size of the formatting buffer in Log::Write (Log.cpp). Longer messages
	// are cut to MessageBufferSize - 1 characters plus the terminator.
	constexpr usize MessageBufferSize = 2048;

	// ── Compile-time contract ────────────────────────────────────────────────
	// The VAELEN_LOG_* macros hard-code the numeric value of each level for
	// the compile-time floor; they must agree with the enum.
	static_assert(ToUnderlying(LogLevel::Trace) == 0);
	static_assert(ToUnderlying(LogLevel::Debug) == 1);
	static_assert(ToUnderlying(LogLevel::Info) == 2);
	static_assert(ToUnderlying(LogLevel::Warning) == 3);
	static_assert(ToUnderlying(LogLevel::Error) == 4);
	static_assert(ToUnderlying(LogLevel::Fatal) == 5);
	static_assert(ToUnderlying(LogLevel::Off) == 6);
	static_assert(sizeof(LogLevel) == 1, "LogLevel must stay a uint8 enum");
	static_assert(VAELEN_LOG_COMPILED_MIN_LEVEL == 0,
				  "these tests assume the headless default compile-time floor (Trace)");

	// ── Categories ───────────────────────────────────────────────────────────
	VAELEN_DECLARE_LOG_CATEGORY(LogTestTrace); // the declaration macro must compile too
	VAELEN_DEFINE_LOG_CATEGORY(LogTestDefault);
	VAELEN_DEFINE_LOG_CATEGORY_LEVEL(LogTestTrace, Trace);
	VAELEN_DEFINE_LOG_CATEGORY_LEVEL(LogTestWarning, Warning);
	VAELEN_DEFINE_LOG_CATEGORY_LEVEL(LogTestThreads, Trace);

	// With assertions enabled the default handler logs the failure through the
	// sink lock the sink is already holding; without them nothing is reported.
#if VAELEN_ASSERTS_ENABLED
#	define VT_ENSURE_FROM_SINK() ((void)VAELEN_ENSURE(false))
#else
#	define VT_ENSURE_FROM_SINK() ((void)0)
#endif

	// ── Sinks ────────────────────────────────────────────────────────────────
	struct CapturedRecord
	{
		const LogCategory* Category = nullptr;
		std::string CategoryName;
		LogLevel Level = LogLevel::Info;
		std::string Message;
		std::string File;
		int32 Line = 0;
	};

	/// Stores a deep copy of every record: LogRecord::Message only lives for
	/// the duration of ILogSink::Write.
	class CapturingSink final : public ILogSink
	{
	public:
		void Write(const LogRecord& Record) override
		{
			CapturedRecord Copy;
			Copy.Category = Record.Category;
			Copy.CategoryName =
				(Record.Category != nullptr && Record.Category->Name != nullptr) ? Record.Category->Name : "";
			Copy.Level = Record.Level;
			Copy.Message = Record.Message != nullptr ? Record.Message : "";
			Copy.File = Record.File != nullptr ? Record.File : "";
			Copy.Line = Record.Line;
			Records.push_back(std::move(Copy));
		}

		void Flush() override { ++FlushCount; }

		usize Count() const noexcept { return Records.size(); }

		const CapturedRecord& Last() const noexcept { return Records.back(); }

		std::vector<CapturedRecord> Records;
		int32 FlushCount = 0;
	};

	/// Registers a CapturingSink for the enclosing scope and removes it again
	/// on every exit path, so a failing VT_REQUIRE cannot leak a sink.
	class ScopedSink final
	{
	public:
		ScopedSink() noexcept : Added(Log::AddSink(&Sink)) {}
		~ScopedSink() noexcept { Log::RemoveSink(&Sink); }
		ScopedSink(const ScopedSink&) = delete;
		ScopedSink& operator=(const ScopedSink&) = delete;

		CapturingSink Sink;
		bool Added;
	};

	/// Sets the global minimum level for the enclosing scope and restores the
	/// previous value on every exit path.
	class ScopedGlobalMinLevel final
	{
	public:
		explicit ScopedGlobalMinLevel(LogLevel Level) noexcept : Previous(Log::GetGlobalMinLevel())
		{
			Log::SetGlobalMinLevel(Level);
		}
		~ScopedGlobalMinLevel() noexcept { Log::SetGlobalMinLevel(Previous); }
		ScopedGlobalMinLevel(const ScopedGlobalMinLevel&) = delete;
		ScopedGlobalMinLevel& operator=(const ScopedGlobalMinLevel&) = delete;

		LogLevel Previous;
	};

	class NullSink final : public ILogSink
	{
	public:
		void Write(const LogRecord&) override {}
	};

	/// Number of registered sinks, straight from the kernel.
	usize CountRegisteredSinks() noexcept
	{
		return Log::GetSinkCount();
	}

	/// Tests/Harness/TestMain.cpp registers a StdioLogSink for --verbose runs.
	/// A test that has to call RemoveAllSinks() drops it and cannot re-add that
	/// exact object (its address is private to main and sinks cannot be
	/// enumerated), so when a foreign sink was present in a verbose run it
	/// registers an equivalent process-lifetime stdout sink instead; later
	/// tests keep echoing kernel logs. In quiet runs nothing is registered.
	void RestoreForeignSinks(const VaelenTest::Context& Ctx, usize ForeignSinksBefore) noexcept
	{
		if (ForeignSinksBefore > 0 && Ctx.Verbose)
		{
			static StdioLogSink ReplacementSink;
			Log::AddSink(&ReplacementSink);
		}
	}

	/// Thread-safety sink: counts records with atomics only, so the test stays
	/// race-free even if the kernel's dispatch lock were broken.
	class ConcurrencySink final : public ILogSink
	{
	public:
		static constexpr int32 ThreadCount = 8;
		static constexpr int32 RecordsPerThread = 1000;
		static constexpr usize TotalRecords = static_cast<usize>(ThreadCount) * static_cast<usize>(RecordsPerThread);

		ConcurrencySink() : Seen(TotalRecords) {}

		void Write(const LogRecord& Record) override
		{
			Total.fetch_add(1, std::memory_order_relaxed);
			int T = -1;
			int I = -1;
			if (Record.Category != &LogTestThreads || Record.Level != LogLevel::Info || Record.Message == nullptr ||
				Record.File == nullptr || Record.Line <= 0 || std::sscanf(Record.Message, "t=%d i=%d", &T, &I) != 2 ||
				T < 0 || T >= ThreadCount || I < 0 || I >= RecordsPerThread)
			{
				Malformed.fetch_add(1, std::memory_order_relaxed);
				return;
			}
			const usize Slot = static_cast<usize>(T) * static_cast<usize>(RecordsPerThread) + static_cast<usize>(I);
			Seen[Slot].fetch_add(1, std::memory_order_relaxed);
		}

		std::atomic<uint64> Total{0};
		std::atomic<uint64> Malformed{0};
		std::vector<std::atomic<uint32>> Seen;
	};
} // namespace

// ── Record delivery ──────────────────────────────────────────────────────────

VAELEN_TEST(Log, SinkReceivesFormattedRecord)
{
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);

	const int32 ExpectedLine = __LINE__ + 1; // line of the VAELEN_LOG_INFO call below
	VAELEN_LOG_INFO(LogTestTrace, "x=%d", 42);
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{1});

	const CapturedRecord& Rec = Scope.Sink.Last();
	VT_CHECK(Rec.Category == &LogTestTrace);
	VT_CHECK_STREQ(Rec.CategoryName.c_str(), "LogTestTrace");
	VT_CHECK_EQ(Rec.Level, LogLevel::Info);
	VT_CHECK_STREQ(Rec.Message.c_str(), "x=42");
	VT_CHECK_STREQ(Rec.File.c_str(), __FILE__);
	VT_CHECK(Rec.Line > 0);
	VT_CHECK_EQ(Rec.Line, ExpectedLine);

	// Mixed argument types go through printf formatting.
	VAELEN_LOG_WARNING(LogTestTrace, "%s:%u:%.2f:%c:%lld", "s", 7u, 1.5, 'z', 123456789012LL);
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{2});
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "s:7:1.50:z:123456789012");
	VT_CHECK_EQ(Scope.Sink.Last().Level, LogLevel::Warning);
}

VAELEN_TEST(Log, EveryLevelMacroDeliversItsOwnLevel)
{
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);

	VAELEN_LOG_TRACE(LogTestTrace, "%s", "trace");
	VAELEN_LOG_DEBUG(LogTestTrace, "%s", "debug");
	VAELEN_LOG_INFO(LogTestTrace, "%s", "info");
	VAELEN_LOG_WARNING(LogTestTrace, "%s", "warning");
	VAELEN_LOG_ERROR(LogTestTrace, "%s", "error");
	VAELEN_LOG_FATAL(LogTestTrace, "%s", "fatal");
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{6});

	constexpr LogLevel ExpectedLevels[] = {LogLevel::Trace,	  LogLevel::Debug, LogLevel::Info,
										   LogLevel::Warning, LogLevel::Error, LogLevel::Fatal};
	constexpr const char* ExpectedMessages[] = {"trace", "debug", "info", "warning", "error", "fatal"};
	static_assert(ArrayCount(ExpectedLevels) == ArrayCount(ExpectedMessages));
	for (usize i = 0; i < ArrayCount(ExpectedLevels); ++i)
	{
		VT_CHECK_EQ(Scope.Sink.Records[i].Level, ExpectedLevels[i]);
		VT_CHECK_STREQ(Scope.Sink.Records[i].Message.c_str(), ExpectedMessages[i]);
		VT_CHECK(Scope.Sink.Records[i].Category == &LogTestTrace);
	}
}

VAELEN_TEST(Log, ArgumentLessMessageIsDelivered)
{
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);

	VAELEN_LOG_INFO(LogTestTrace, "plain");
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{1});
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "plain");
	VT_CHECK_EQ(Scope.Sink.Last().Level, LogLevel::Info);

	// The message is still a printf format: "%%" collapses to "%".
	VAELEN_LOG_INFO(LogTestTrace, "100%% done");
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{2});
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "100% done");

	// An empty formatted message is still a record.
	VAELEN_LOG_INFO(LogTestTrace, "%s", "");
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{3});
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "");
}

VAELEN_TEST(Log, FatalLevelIsJustARecord)
{
	// Fatal is a verbosity level, not an abort: the test must continue.
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);
	VAELEN_LOG_FATAL(LogTestTrace, "fatal %s", "record");
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{1});
	VT_CHECK_EQ(Scope.Sink.Last().Level, LogLevel::Fatal);
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "fatal record");
}

// ── Thresholds ───────────────────────────────────────────────────────────────

VAELEN_TEST(Log, CategoryThresholdDropsRecordsBelowMinLevel)
{
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);

	// VAELEN_DEFINE_LOG_CATEGORY_LEVEL stringifies the name and sets the floor.
	VT_CHECK_STREQ(LogTestWarning.Name, "LogTestWarning");
	VT_CHECK_EQ(LogTestWarning.MinLevel, LogLevel::Warning);
	VT_CHECK(!LogTestWarning.IsEnabled(LogLevel::Trace));
	VT_CHECK(!LogTestWarning.IsEnabled(LogLevel::Debug));
	VT_CHECK(!LogTestWarning.IsEnabled(LogLevel::Info));
	VT_CHECK(LogTestWarning.IsEnabled(LogLevel::Warning));
	VT_CHECK(LogTestWarning.IsEnabled(LogLevel::Error));
	VT_CHECK(LogTestWarning.IsEnabled(LogLevel::Fatal));

	VAELEN_LOG_TRACE(LogTestWarning, "dropped %d", 0);
	VAELEN_LOG_DEBUG(LogTestWarning, "dropped %d", 1);
	VAELEN_LOG_INFO(LogTestWarning, "dropped %d", 2);
	VT_CHECK_EQ(Scope.Sink.Count(), usize{0});

	VAELEN_LOG_WARNING(LogTestWarning, "kept %d", 3);
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{1});
	VT_CHECK_EQ(Scope.Sink.Last().Level, LogLevel::Warning);
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "kept 3");
	VT_CHECK_STREQ(Scope.Sink.Last().CategoryName.c_str(), "LogTestWarning");

	VAELEN_LOG_ERROR(LogTestWarning, "kept %d", 4);
	VAELEN_LOG_FATAL(LogTestWarning, "kept %d", 5);
	VT_CHECK_EQ(Scope.Sink.Count(), usize{3});

	// A category constructed directly with a threshold behaves the same.
	LogCategory Local("Local", LogLevel::Error);
	VT_CHECK_STREQ(Local.Name, "Local");
	VAELEN_LOG_WARNING(Local, "dropped");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{3});
	VAELEN_LOG_ERROR(Local, "kept");
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{4});
	VT_CHECK_STREQ(Scope.Sink.Last().CategoryName.c_str(), "Local");
	VT_CHECK(Scope.Sink.Last().Category == &Local);

	// The threshold is a runtime knob: Off silences everything, Trace opens it.
	Local.MinLevel = LogLevel::Off;
	VT_CHECK(!Local.IsEnabled(LogLevel::Fatal));
	VAELEN_LOG_FATAL(Local, "dropped");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{4});
	Local.MinLevel = LogLevel::Trace;
	VAELEN_LOG_TRACE(Local, "kept");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{5});
}

VAELEN_TEST(Log, DefaultCategoryMinLevelIsInfo)
{
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);

	VT_CHECK_STREQ(LogTestDefault.Name, "LogTestDefault");
	VT_CHECK_EQ(LogTestDefault.MinLevel, LogLevel::Info);
	const LogCategory Implicit("Implicit");
	VT_CHECK_EQ(Implicit.MinLevel, LogLevel::Info);

	VAELEN_LOG_TRACE(LogTestDefault, "dropped");
	VAELEN_LOG_DEBUG(LogTestDefault, "dropped");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{0});
	VAELEN_LOG_INFO(LogTestDefault, "kept");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{1});
	VAELEN_LOG_INFO(Implicit, "kept");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{2});
}

VAELEN_TEST(Log, GlobalMinLevelAppliesBeforeCategoryThreshold)
{
	// Nothing in this process may leave the global level raised.
	VT_REQUIRE_EQ(Log::GetGlobalMinLevel(), LogLevel::Trace);

	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);
	{
		ScopedGlobalMinLevel Raised(LogLevel::Error);
		VT_CHECK_EQ(Log::GetGlobalMinLevel(), LogLevel::Error);

		// A Trace category cannot get below the global floor.
		VAELEN_LOG_TRACE(LogTestTrace, "dropped");
		VAELEN_LOG_INFO(LogTestTrace, "dropped");
		VAELEN_LOG_WARNING(LogTestTrace, "dropped");
		VT_CHECK_EQ(Scope.Sink.Count(), usize{0});
		VAELEN_LOG_ERROR(LogTestTrace, "kept");
		VAELEN_LOG_FATAL(LogTestTrace, "kept");
		VT_CHECK_EQ(Scope.Sink.Count(), usize{2});

		// The category threshold still applies on top of the global one.
		LogCategory FatalOnly("FatalOnly", LogLevel::Fatal);
		VAELEN_LOG_ERROR(FatalOnly, "dropped");
		VT_CHECK_EQ(Scope.Sink.Count(), usize{2});
		VAELEN_LOG_FATAL(FatalOnly, "kept");
		VT_CHECK_EQ(Scope.Sink.Count(), usize{3});
	}
	VT_CHECK_EQ(Log::GetGlobalMinLevel(), LogLevel::Trace);
	VAELEN_LOG_TRACE(LogTestTrace, "kept again");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{4});

	{
		ScopedGlobalMinLevel Silenced(LogLevel::Off);
		VT_CHECK_EQ(Log::GetGlobalMinLevel(), LogLevel::Off);
		VAELEN_LOG_FATAL(LogTestTrace, "dropped");
		VAELEN_LOG_FATAL(LogCore, "dropped");
		VT_CHECK_EQ(Scope.Sink.Count(), usize{4});
	}
	VT_CHECK_EQ(Log::GetGlobalMinLevel(), LogLevel::Trace);

	// Explicit set/get round trip through every level, ending back on Trace.
	constexpr LogLevel AllLevels[] = {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warning,
									  LogLevel::Error, LogLevel::Fatal, LogLevel::Off};
	for (LogLevel Level : AllLevels)
	{
		Log::SetGlobalMinLevel(Level);
		VT_CHECK_EQ(Log::GetGlobalMinLevel(), Level);
	}
	Log::SetGlobalMinLevel(LogLevel::Trace);
	VT_CHECK_EQ(Log::GetGlobalMinLevel(), LogLevel::Trace);
}

VAELEN_TEST(Log, DirectWriteBypassesLevelFilters)
{
	// Log::Write is the unfiltered primitive; the macros do the filtering.
	// Documented here so nobody relies on Write honouring thresholds.
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);

	LogCategory Muted("Muted", LogLevel::Off);
	const uint64 Before = Log::GetDispatchedRecordCount();
	Log::Write(Muted, LogLevel::Trace, "somefile.cpp", 77, "direct %d", 7);
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{1});
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Before + uint64{1});
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "direct 7");
	VT_CHECK_STREQ(Scope.Sink.Last().File.c_str(), "somefile.cpp");
	VT_CHECK_EQ(Scope.Sink.Last().Line, int32{77});
	VT_CHECK_EQ(Scope.Sink.Last().Level, LogLevel::Trace);
	VT_CHECK_STREQ(Scope.Sink.Last().CategoryName.c_str(), "Muted");

	{
		ScopedGlobalMinLevel Silenced(LogLevel::Off);
		Log::Write(Muted, LogLevel::Debug, __FILE__, __LINE__, "still %s", "delivered");
	}
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{2});
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "still delivered");
	VT_CHECK_EQ(Log::GetGlobalMinLevel(), LogLevel::Trace);
}

// ── Sink table ───────────────────────────────────────────────────────────────

VAELEN_TEST(Log, AddSinkRejectsNull)
{
	VT_CHECK(!Log::AddSink(nullptr));
	VT_CHECK(!Log::RemoveSink(nullptr));

	// The table is intact afterwards.
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);
	VAELEN_LOG_INFO(LogTestTrace, "after null");
	VT_CHECK_EQ(Scope.Sink.Count(), usize{1});
}

VAELEN_TEST(Log, AddSinkIsIdempotent)
{
	CapturingSink Sink;
	VT_REQUIRE(Log::AddSink(&Sink));
	VT_CHECK(Log::AddSink(&Sink)); // re-adding reports success ...
	VT_CHECK(Log::AddSink(&Sink));

	VAELEN_LOG_INFO(LogTestTrace, "once");
	VT_CHECK_EQ(Sink.Count(), usize{1}); // ... but the record is delivered once

	VT_CHECK(Log::RemoveSink(&Sink));  // a single removal suffices
	VT_CHECK(!Log::RemoveSink(&Sink)); // it is gone
	VAELEN_LOG_INFO(LogTestTrace, "not delivered");
	VT_CHECK_EQ(Sink.Count(), usize{1});
}

VAELEN_TEST(Log, RemoveSinkReturnsFalseForUnknownSink)
{
	CapturingSink Unknown;
	VT_CHECK(!Log::RemoveSink(&Unknown));

	CapturingSink A;
	CapturingSink B;
	CapturingSink C;
	VT_REQUIRE(Log::AddSink(&A));
	VT_REQUIRE(Log::AddSink(&B));
	VT_REQUIRE(Log::AddSink(&C));

	// Removing the middle entry keeps the others registered.
	VT_CHECK(Log::RemoveSink(&B));
	VT_CHECK(!Log::RemoveSink(&B));
	VAELEN_LOG_INFO(LogTestTrace, "after removing B");
	VT_CHECK_EQ(A.Count(), usize{1});
	VT_CHECK_EQ(B.Count(), usize{0});
	VT_CHECK_EQ(C.Count(), usize{1});

	VT_CHECK(Log::RemoveSink(&C));
	VT_CHECK(Log::RemoveSink(&A));
	VT_CHECK(!Log::RemoveSink(&A));
	VT_CHECK(!Log::RemoveSink(&C));
	VAELEN_LOG_INFO(LogTestTrace, "after removing all");
	VT_CHECK_EQ(A.Count(), usize{1});
	VT_CHECK_EQ(B.Count(), usize{0});
	VT_CHECK_EQ(C.Count(), usize{1});
}

VAELEN_TEST(Log, SinkTableHoldsAtMostEightSinks)
{
	const usize Foreign = CountRegisteredSinks();
	Log::RemoveAllSinks();
	VT_CHECK_EQ(CountRegisteredSinks(), usize{0});

	CapturingSink Sinks[MaxSinks + 1];
	for (usize i = 0; i < MaxSinks; ++i)
	{
		VT_CHECK_MSG(Log::AddSink(&Sinks[i]), "sink %llu of %llu was rejected", static_cast<unsigned long long>(i + 1),
					 static_cast<unsigned long long>(MaxSinks));
	}
	VT_CHECK(!Log::AddSink(&Sinks[MaxSinks]));	  // the 9th sink is rejected ...
	VT_CHECK(!Log::RemoveSink(&Sinks[MaxSinks])); // ... and was never registered

	VAELEN_LOG_INFO(LogTestTrace, "fan-out");
	for (usize i = 0; i < MaxSinks; ++i)
	{
		VT_CHECK_EQ(Sinks[i].Count(), usize{1});
	}
	VT_CHECK_EQ(Sinks[MaxSinks].Count(), usize{0});

	// Freeing one slot makes room for the previously rejected sink.
	VT_CHECK(Log::RemoveSink(&Sinks[0]));
	VT_CHECK(Log::AddSink(&Sinks[MaxSinks]));
	VAELEN_LOG_INFO(LogTestTrace, "fan-out again");
	VT_CHECK_EQ(Sinks[0].Count(), usize{1});
	for (usize i = 1; i < MaxSinks; ++i)
	{
		VT_CHECK_EQ(Sinks[i].Count(), usize{2});
	}
	VT_CHECK_EQ(Sinks[MaxSinks].Count(), usize{1});

	for (usize i = 1; i <= MaxSinks; ++i)
	{
		VT_CHECK(Log::RemoveSink(&Sinks[i]));
	}
	VT_CHECK_EQ(CountRegisteredSinks(), usize{0});

	// Unconditional sweep so a failed check above can never leave a dangling
	// sink behind, then give a verbose run its stdout echo back.
	for (usize i = 0; i <= MaxSinks; ++i)
	{
		Log::RemoveSink(&Sinks[i]);
	}
	RestoreForeignSinks(Ctx, Foreign);
}

VAELEN_TEST(Log, RemoveAllSinksEmptiesTable)
{
	const usize Foreign = CountRegisteredSinks();

	CapturingSink A;
	CapturingSink B;
	CapturingSink C;
	VT_CHECK(Log::AddSink(&A));
	VT_CHECK(Log::AddSink(&B));
	VT_CHECK(Log::AddSink(&C));
	VAELEN_LOG_INFO(LogTestTrace, "before");
	VT_CHECK_EQ(A.Count(), usize{1});
	VT_CHECK_EQ(B.Count(), usize{1});
	VT_CHECK_EQ(C.Count(), usize{1});

	Log::RemoveAllSinks();
	VT_CHECK_EQ(CountRegisteredSinks(), usize{0});
	VT_CHECK(!Log::RemoveSink(&A));
	VT_CHECK(!Log::RemoveSink(&B));
	VT_CHECK(!Log::RemoveSink(&C));

	// Actual behaviour with no sink registered: a record that passes the
	// filters is still "dispatched" (the counter counts records, not
	// deliveries), it just reaches nobody.
	const uint64 Before = Log::GetDispatchedRecordCount();
	VAELEN_LOG_INFO(LogTestTrace, "after");
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Before + uint64{1});
	VT_CHECK_EQ(A.Count(), usize{1});
	VT_CHECK_EQ(B.Count(), usize{1});
	VT_CHECK_EQ(C.Count(), usize{1});

	// Flushing an empty table and clearing it twice are harmless no-ops.
	Log::Flush();
	Log::RemoveAllSinks();
	VT_CHECK_EQ(CountRegisteredSinks(), usize{0});

	// The table is usable again afterwards.
	VT_CHECK(Log::AddSink(&A));
	VAELEN_LOG_INFO(LogTestTrace, "re-added");
	VT_CHECK_EQ(A.Count(), usize{2});
	VT_CHECK(Log::RemoveSink(&A));

	Log::RemoveSink(&B);
	Log::RemoveSink(&C);
	RestoreForeignSinks(Ctx, Foreign);
}

VAELEN_TEST(Log, FlushReachesEveryRegisteredSink)
{
	ScopedSink A;
	ScopedSink B;
	VT_REQUIRE(A.Added);
	VT_REQUIRE(B.Added);

	Log::Flush();
	VT_CHECK_EQ(A.Sink.FlushCount, int32{1});
	VT_CHECK_EQ(B.Sink.FlushCount, int32{1});
	Log::Flush();
	VT_CHECK_EQ(A.Sink.FlushCount, int32{2});
	VT_CHECK_EQ(B.Sink.FlushCount, int32{2});

	CapturingSink Removed;
	VT_REQUIRE(Log::AddSink(&Removed));
	Log::Flush();
	VT_CHECK_EQ(Removed.FlushCount, int32{1});
	VT_CHECK(Log::RemoveSink(&Removed));
	Log::Flush();
	VT_CHECK_EQ(Removed.FlushCount, int32{1});
	VT_CHECK_EQ(A.Sink.FlushCount, int32{4});
}

// ── Dispatch counter ─────────────────────────────────────────────────────────

VAELEN_TEST(Log, DispatchedRecordCountIncrementsOncePerDeliveredRecord)
{
	ScopedSink First;
	VT_REQUIRE(First.Added);
	const uint64 Start = Log::GetDispatchedRecordCount();

	VAELEN_LOG_INFO(LogTestTrace, "one");
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Start + uint64{1});
	VT_CHECK_EQ(First.Sink.Count(), usize{1});
	VAELEN_LOG_INFO(LogTestTrace, "two");
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Start + uint64{2});

	// Records dropped by the category or global filter are never dispatched.
	VAELEN_LOG_DEBUG(LogTestWarning, "dropped");
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Start + uint64{2});
	{
		ScopedGlobalMinLevel Silenced(LogLevel::Off);
		VAELEN_LOG_FATAL(LogTestTrace, "dropped");
	}
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Start + uint64{2});
	VT_CHECK_EQ(First.Sink.Count(), usize{2});

	// A second sink does not double count: one record, one increment, two deliveries.
	ScopedSink Second;
	VT_REQUIRE(Second.Added);
	VAELEN_LOG_INFO(LogTestTrace, "three");
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Start + uint64{3});
	VT_CHECK_EQ(First.Sink.Count(), usize{3});
	VT_CHECK_EQ(Second.Sink.Count(), usize{1});

	for (int32 i = 0; i < 100; ++i)
	{
		VAELEN_LOG_TRACE(LogTestTrace, "burst %d", i);
	}
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Start + uint64{103});
	VT_CHECK_EQ(First.Sink.Count(), usize{103});
	VT_CHECK_EQ(Second.Sink.Count(), usize{101});
}

// ── Names, defaults and helpers ──────────────────────────────────────────────

VAELEN_TEST(Log, LevelToStringCoversAllLevels)
{
	VT_CHECK_STREQ(LogLevelToString(LogLevel::Trace), "Trace");
	VT_CHECK_STREQ(LogLevelToString(LogLevel::Debug), "Debug");
	VT_CHECK_STREQ(LogLevelToString(LogLevel::Info), "Info");
	VT_CHECK_STREQ(LogLevelToString(LogLevel::Warning), "Warning");
	VT_CHECK_STREQ(LogLevelToString(LogLevel::Error), "Error");
	VT_CHECK_STREQ(LogLevelToString(LogLevel::Fatal), "Fatal");
	VT_CHECK_STREQ(LogLevelToString(LogLevel::Off), "Off");

	// Values outside the enumeration are reported, never dereferenced blindly.
	VT_CHECK_STREQ(LogLevelToString(static_cast<LogLevel>(7)), "Unknown");
	VT_CHECK_STREQ(LogLevelToString(static_cast<LogLevel>(200)), "Unknown");

	// Names are non-empty and pairwise distinct.
	constexpr LogLevel AllLevels[] = {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warning,
									  LogLevel::Error, LogLevel::Fatal, LogLevel::Off};
	for (usize i = 0; i < ArrayCount(AllLevels); ++i)
	{
		const char* Name = LogLevelToString(AllLevels[i]);
		VT_REQUIRE(Name != nullptr);
		VT_CHECK(Name[0] != '\0');
		for (usize j = i + 1; j < ArrayCount(AllLevels); ++j)
		{
			VT_CHECK_MSG(std::strcmp(Name, LogLevelToString(AllLevels[j])) != 0, "levels %llu and %llu share name %s",
						 static_cast<unsigned long long>(i), static_cast<unsigned long long>(j), Name);
		}
	}
}

VAELEN_TEST(Log, LogCoreIsTraceEnabled)
{
	VT_CHECK_STREQ(LogCore.Name, "LogCore");
	VT_CHECK_EQ(LogCore.MinLevel, LogLevel::Trace);
	VT_CHECK(LogCore.IsEnabled(LogLevel::Trace));
	VT_CHECK(LogCore.IsEnabled(LogLevel::Debug));
	VT_CHECK(LogCore.IsEnabled(LogLevel::Info));
	VT_CHECK(LogCore.IsEnabled(LogLevel::Warning));
	VT_CHECK(LogCore.IsEnabled(LogLevel::Error));
	VT_CHECK(LogCore.IsEnabled(LogLevel::Fatal));

	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);
	VAELEN_LOG_TRACE(LogCore, "core trace %d", 1);
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{1});
	VT_CHECK(Scope.Sink.Last().Category == &LogCore);
	VT_CHECK_STREQ(Scope.Sink.Last().CategoryName.c_str(), "LogCore");
	VT_CHECK_EQ(Scope.Sink.Last().Level, LogLevel::Trace);
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "core trace 1");
}

VAELEN_TEST(Log, RecordDefaultsAndStdioSinkWithoutCategory)
{
	LogRecord Record;
	VT_CHECK(Record.Category == nullptr);
	VT_CHECK_EQ(Record.Level, LogLevel::Info);
	VT_REQUIRE(Record.Message != nullptr);
	VT_CHECK_STREQ(Record.Message, "");
	VT_REQUIRE(Record.File != nullptr);
	VT_CHECK_STREQ(Record.File, "");
	VT_CHECK_EQ(Record.Line, int32{0});

	// The stdio sink is exercised on two temporary files: a record without a
	// category prints "?", Warning and above go to the error stream.
	std::FILE* Out = std::tmpfile();
	std::FILE* Err = std::tmpfile();
	VT_REQUIRE(Out != nullptr && Err != nullptr);
	{
		StdioLogSink Sink(Out, Err);
		Record.Level = LogLevel::Trace;
		Record.Message = "no category";
		Sink.Write(Record);
		LogRecord Warning;
		Warning.Category = &LogCore;
		Warning.Level = LogLevel::Warning;
		Warning.Message = "to stderr";
		Sink.Write(Warning);
		LogRecord Info;
		Info.Category = &LogCore;
		Info.Level = LogLevel::Info;
		Info.Message = "to stdout";
		Sink.Write(Info);
		Sink.Flush();
	}
	auto ReadAll = [](std::FILE* File)
	{
		std::rewind(File);
		std::string Text;
		char Line[256];
		while (std::fgets(Line, sizeof(Line), File) != nullptr)
		{
			Text += Line;
		}
		return Text;
	};
	const std::string OutText = ReadAll(Out);
	const std::string ErrText = ReadAll(Err);
	std::fclose(Out);
	std::fclose(Err);
	VT_CHECK(OutText == "[Trace] ?: no category\n[Info] LogCore: to stdout\n");
	VT_CHECK(ErrText == "[Warning] LogCore: to stderr\n");
}

VAELEN_TEST(Log, SinkCountTracksAddAndRemove)
{
	const usize Base = Log::GetSinkCount();
	CapturingSink A;
	CapturingSink B;
	VT_CHECK(Log::AddSink(&A));
	VT_CHECK_EQ(Log::GetSinkCount(), Base + 1);
	VT_CHECK(Log::AddSink(&A)); // idempotent
	VT_CHECK_EQ(Log::GetSinkCount(), Base + 1);
	VT_CHECK(Log::AddSink(&B));
	VT_CHECK_EQ(Log::GetSinkCount(), Base + 2);
	VT_CHECK(Log::RemoveSink(&A));
	VT_CHECK_EQ(Log::GetSinkCount(), Base + 1);
	VT_CHECK(!Log::AddSink(nullptr));
	VT_CHECK_EQ(Log::GetSinkCount(), Base + 1);
	VT_CHECK(Log::RemoveSink(&B));
	VT_CHECK_EQ(Log::GetSinkCount(), Base);
}

namespace
{
	/// Detects two overlapping Write() calls: proves sinks are serialised.
	class OverlapDetectingSink final : public ILogSink
	{
	public:
		void Write(const LogRecord&) override
		{
			if (InWrite.exchange(true, std::memory_order_acq_rel))
			{
				Overlaps.fetch_add(1, std::memory_order_relaxed);
			}
			++PlainCount; // safe only if the kernel serialises sinks
			std::this_thread::yield();
			InWrite.store(false, std::memory_order_release);
		}
		std::atomic<bool> InWrite{false};
		std::atomic<uint64> Overlaps{0};
		uint64 PlainCount = 0;
	};

	/// A sink that logs, flushes, counts sinks and removes itself from inside
	/// Write(): every re-entrant path into Log::* from a sink.
	class ReentrantSink final : public ILogSink
	{
	public:
		void Write(const LogRecord& Record) override
		{
			++Calls;
			if (Depth > 0)
			{
				++NestedDeliveries;
				return;
			}
			++Depth;
			VAELEN_LOG_INFO(LogTestTrace, "nested from %s", Record.Message);
			Log::Flush();
			SinksSeen = Log::GetSinkCount();
			VT_ENSURE_FROM_SINK();
			Log::RemoveSink(this);
			--Depth;
		}
		void Flush() override { ++Flushes; }
		int32 Calls = 0;
		int32 NestedDeliveries = 0;
		int32 Flushes = 0;
		int32 Depth = 0;
		usize SinksSeen = 0;
	};
} // namespace

VAELEN_TEST(Log, SinksAreSerialisedAcrossThreads)
{
	OverlapDetectingSink Sink;
	VT_REQUIRE(Log::AddSink(&Sink));
	constexpr int32 Threads = 8;
	constexpr int32 PerThread = 500;
	std::vector<std::thread> Workers;
	for (int32 t = 0; t < Threads; ++t)
	{
		Workers.emplace_back(
			[t]()
			{
				for (int32 i = 0; i < PerThread; ++i)
				{
					VAELEN_LOG_INFO(LogTestThreads, "t=%d i=%d", t, i);
				}
			});
	}
	for (std::thread& Worker : Workers)
	{
		Worker.join();
	}
	VT_CHECK(Log::RemoveSink(&Sink));
	VT_CHECK_EQ(Sink.Overlaps.load(), uint64{0});
	VT_CHECK_EQ(Sink.PlainCount, static_cast<uint64>(Threads * PerThread));
}

VAELEN_TEST(Log, ReentrantSinkDoesNotDeadlock)
{
	// A sink may log, flush, inspect and edit the sink table and fail an
	// ensure while being called; the default assertion handler logs through
	// the same lock. (The stderr line from the ensure is expected output.)
	ScopedSink Capture;
	VT_REQUIRE(Capture.Added);
	ReentrantSink Sink;
	VT_REQUIRE(Log::AddSink(&Sink));
	const usize Registered = Log::GetSinkCount();

	VAELEN_LOG_INFO(LogTestTrace, "outer");

#if VAELEN_ASSERTS_ENABLED
	// "outer", its own nested record, and the default handler's error record.
	VT_CHECK_EQ(Sink.Calls, 3);
	VT_CHECK_EQ(Sink.NestedDeliveries, 2);
#else
	VT_CHECK_EQ(Sink.Calls, 2); // "outer" and its own nested record
	VT_CHECK_EQ(Sink.NestedDeliveries, 1);
#endif
	VT_CHECK(Sink.Flushes >= 1);
	VT_CHECK_EQ(Sink.SinksSeen, Registered);
	VT_CHECK_EQ(Log::GetSinkCount(), Registered - 1); // it removed itself
	VT_CHECK(!Log::RemoveSink(&Sink));
	// The capturing sink saw outer, nested, and the ensure's error record.
	bool SawOuter = false;
	bool SawNested = false;
	for (const CapturedRecord& Record : Capture.Sink.Records)
	{
		SawOuter = SawOuter || Record.Message == "outer";
		SawNested = SawNested || Record.Message == "nested from outer";
	}
	VT_CHECK(SawOuter);
	VT_CHECK(SawNested);
}

// ── Robustness ───────────────────────────────────────────────────────────────

VAELEN_TEST(Log, LongMessageIsTruncatedSafely)
{
	ScopedSink Scope;
	VT_REQUIRE(Scope.Added);

	const std::string Long(4000, 'A');
	VAELEN_LOG_INFO(LogTestTrace, "%s", Long.c_str());
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{1});
	{
		const std::string& Message = Scope.Sink.Last().Message;
		VT_CHECK(Message.size() < MessageBufferSize);
		VT_CHECK_EQ(Message.size(), MessageBufferSize - 1);
		// A truncated message ends with the "..." marker; everything before it
		// is the original prefix.
		VT_CHECK(Message.compare(Message.size() - 3, 3, "...") == 0);
		VT_CHECK(Message.find_first_not_of('A') == Message.size() - 3);
	}

	// The prefix survives; only the tail is cut.
	VAELEN_LOG_INFO(LogTestTrace, "head:%s", Long.c_str());
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{2});
	{
		const std::string& Message = Scope.Sink.Last().Message;
		VT_CHECK_EQ(Message.size(), MessageBufferSize - 1);
		VT_CHECK(Message.compare(0, 5, "head:") == 0);
		VT_CHECK(Message.find_first_not_of('A', 5) == Message.size() - 3);
		VT_CHECK(Message.compare(Message.size() - 3, 3, "...") == 0);
	}

	// Exactly fitting: MessageBufferSize - 1 characters pass untouched.
	const std::string Fit(MessageBufferSize - 1, 'B');
	VAELEN_LOG_INFO(LogTestTrace, "%s", Fit.c_str());
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{3});
	VT_CHECK(Scope.Sink.Last().Message == Fit);

	// One character over loses exactly that character.
	const std::string Over(MessageBufferSize, 'C');
	VAELEN_LOG_INFO(LogTestTrace, "%s", Over.c_str());
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{4});
	VT_CHECK_EQ(Scope.Sink.Last().Message.size(), MessageBufferSize - 1);
	VT_CHECK(Scope.Sink.Last().Message.find_first_not_of('C') == MessageBufferSize - 4);
	VT_CHECK(Scope.Sink.Last().Message.compare(MessageBufferSize - 4, 3, "...") == 0);

	// Logging still works normally afterwards.
	VAELEN_LOG_INFO(LogTestTrace, "short");
	VT_REQUIRE_EQ(Scope.Sink.Count(), usize{5});
	VT_CHECK_STREQ(Scope.Sink.Last().Message.c_str(), "short");
}

VAELEN_TEST(Log, ConcurrentWritesFromEightThreads)
{
	ConcurrencySink Sink;
	VT_REQUIRE(Log::AddSink(&Sink));
	const uint64 Before = Log::GetDispatchedRecordCount();
	{
		std::vector<std::thread> Threads;
		Threads.reserve(static_cast<usize>(ConcurrencySink::ThreadCount));
		for (int32 T = 0; T < ConcurrencySink::ThreadCount; ++T)
		{
			Threads.emplace_back(
				[T]()
				{
					for (int32 I = 0; I < ConcurrencySink::RecordsPerThread; ++I)
					{
						VAELEN_LOG_INFO(LogTestThreads, "t=%d i=%d", T, I);
					}
				});
		}
		for (std::thread& Thread : Threads)
		{
			Thread.join();
		}
	}
	const uint64 After = Log::GetDispatchedRecordCount();
	VT_CHECK(Log::RemoveSink(&Sink));

	VT_CHECK_EQ(Sink.Total.load(), static_cast<uint64>(ConcurrencySink::TotalRecords));
	VT_CHECK_EQ(Sink.Malformed.load(), uint64{0});
	VT_CHECK_EQ(After - Before, static_cast<uint64>(ConcurrencySink::TotalRecords));

	// Every (thread, index) pair arrived exactly once: no lost or duplicated
	// records, and no corrupted messages under contention.
	usize Missing = 0;
	usize Duplicated = 0;
	for (usize Slot = 0; Slot < ConcurrencySink::TotalRecords; ++Slot)
	{
		const uint32 Hits = Sink.Seen[Slot].load();
		if (Hits == 0)
		{
			++Missing;
		}
		else if (Hits > 1)
		{
			++Duplicated;
		}
	}
	VT_CHECK_EQ(Missing, usize{0});
	VT_CHECK_EQ(Duplicated, usize{0});

	// Nothing reaches the sink once it is removed.
	VAELEN_LOG_INFO(LogTestThreads, "t=%d i=%d", 0, 0);
	VT_CHECK_EQ(Sink.Total.load(), static_cast<uint64>(ConcurrencySink::TotalRecords));
}
