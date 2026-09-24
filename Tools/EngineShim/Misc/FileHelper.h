// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

struct FFileHelper
{
	static bool SaveStringToFile(const FString& Text, const TCHAR* Path);
	static bool LoadFileToString(FString& Out, const TCHAR* Path);
	// 16.14: bytes, not text. A checkpoint is a VAELENCP container, and a
	// string round trip would stop at its first NUL. The engine's first
	// parameter is a TArrayView<const uint8>, which a TArray<uint8> binds to;
	// the shim takes the array itself, which is the call the store makes.
	static bool SaveArrayToFile(const TArray<uint8>& Bytes, const TCHAR* Path);
	static bool LoadFileToArray(TArray<uint8>& Out, const TCHAR* Path);
};
