// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

/// Takes the lock by POINTER, which is the engine's signature and is worth
/// copying exactly: passing a reference here would compile in the shim and not
/// in the engine, which is the one failure mode this whole directory exists to
/// avoid.
class FScopeLock
{
public:
	explicit FScopeLock(FCriticalSection* InSection) : Section(InSection) {}
	~FScopeLock() = default;

private:
	FCriticalSection* Section = nullptr;
};
