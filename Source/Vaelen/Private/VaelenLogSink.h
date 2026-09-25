// VAELEN - primary game module.
// Routes kernel log records into Unreal's logging system.
//
// STATUS: VALIDATED (UE 5.6, 2026-09-07) - compiled and run in the editor; not covered by the headless CI.
// BUILD: b0921 - Tools/engine_builds.txt; its code is what that build compiled (19.01).
#pragma once

#include "CoreMinimal.h"
#include "Vaelen/Core/Log.h"

class FVaelenLogSink final : public Vaelen::ILogSink
{
public:
	virtual void Write(const Vaelen::LogRecord& Record) override;
	virtual void Flush() override;
};
