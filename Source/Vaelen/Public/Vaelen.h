// VAELEN - primary game module.
//
// STATUS: UNVERIFIED - not compiled in the headless CI (requires UE5).
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

VAELEN_API DECLARE_LOG_CATEGORY_EXTERN(LogVaelen, Log, All);

class FVaelenLogSink;

/// Bridges the engine-agnostic kernel with Unreal: installs the kernel log
/// sink and assertion handler at startup, removes them at shutdown.
class FVaelenModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool IsGameModule() const override { return true; }

private:
	TUniquePtr<FVaelenLogSink> LogSink;
};
