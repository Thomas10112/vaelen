// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

namespace ConstructorHelpers
{
	template <typename T>
	struct FObjectFinder
	{
		explicit FObjectFinder(const TCHAR* Path) {}
		bool Succeeded() const { return false; }
		T* Object = nullptr;
	};

	template <typename T>
	struct FClassFinder
	{
		explicit FClassFinder(const TCHAR* Path) {}
		bool Succeeded() const { return false; }
		T* Class = nullptr;
	};
} // namespace ConstructorHelpers
