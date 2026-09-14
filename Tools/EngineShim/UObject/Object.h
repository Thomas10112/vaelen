// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

class UObject
{
public:
	virtual ~UObject() = default;
};

/// Constructs a UObject at runtime. The engine's takes an outer and a name.
template <typename T>
T* NewObject(UObject* Outer, FName Name = FName())
{
	return nullptr;
}

/// Loads an asset by path at runtime. Returns null here, which is the honest
/// answer for a stand-in and the answer the calling code has to handle anyway.
template <typename T>
T* LoadObject(UObject* Outer, const TCHAR* Path)
{
	return nullptr;
}
