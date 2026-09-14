// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Object.h"

class UGameInstance : public UObject
{
public:
	using Super = UGameInstance;

	/// Returns null here, which is the honest answer for a stand-in and the
	/// answer the calling code has to handle anyway (UObject/Object.h says
	/// the same of NewObject).
	template <typename T>
	T* GetSubsystem()
	{
		return nullptr;
	}
};
