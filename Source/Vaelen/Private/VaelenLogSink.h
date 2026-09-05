// VAELEN - primary game module.
// Routes kernel log records into Unreal's logging system.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
#pragma once

#include "CoreMinimal.h"
#include "Vaelen/Core/Log.h"

class FVaelenLogSink final : public Vaelen::ILogSink
{
public:
	virtual void Write(const Vaelen::LogRecord& Record) override;
	virtual void Flush() override;
};
