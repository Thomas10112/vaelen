// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

class FCriticalSection
{
public:
	void Lock() {}
	void Unlock() {}
	bool TryLock() { return true; }
};
