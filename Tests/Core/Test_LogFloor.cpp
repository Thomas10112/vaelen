// VAELEN - compile-time log floor.
//
// STATUS: VALIDATED (Phase 00)
//
// This translation unit defines VAELEN_LOG_COMPILED_MIN_LEVEL before including
// Log.h, so it proves the promise "records below the compiled floor are removed
// from the binary" without touching the build configuration.
#define VAELEN_LOG_COMPILED_MIN_LEVEL 3 // Warning

#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"

#include <string>

using namespace Vaelen;

namespace
{
	class CountingSink final : public ILogSink
	{
	public:
		void Write(const LogRecord& Record) override
		{
			++Count;
			Last = Record.Message;
		}
		int Count = 0;
		std::string Last;
	};
} // namespace

VAELEN_TEST(LogFloor, RecordsBelowCompiledFloorAreNotDispatchedOrEvaluated)
{
	static_assert(VAELEN_LOG_COMPILED_MIN_LEVEL == 3);
	CountingSink Sink;
	VT_REQUIRE(Log::AddSink(&Sink));
	const uint64 Before = Log::GetDispatchedRecordCount();

	int Evaluated = 0;
	VAELEN_LOG_TRACE(LogCore, "%d", ++Evaluated);
	VAELEN_LOG_DEBUG(LogCore, "%d", ++Evaluated);
	VAELEN_LOG_INFO(LogCore, "%d", ++Evaluated);
	VT_CHECK_EQ(Evaluated, 0); // arguments of removed records are never evaluated
	VT_CHECK_EQ(Log::GetDispatchedRecordCount(), Before);
	VT_CHECK_EQ(Sink.Count, 0);

	VAELEN_LOG_WARNING(LogCore, "%d", ++Evaluated);
	VAELEN_LOG_ERROR(LogCore, "%d", ++Evaluated);
	VT_CHECK_EQ(Evaluated, 2);
	VT_CHECK_EQ(Sink.Count, 2);
	VT_CHECK(Sink.Last == "2");

	VT_CHECK(Log::RemoveSink(&Sink));
}
