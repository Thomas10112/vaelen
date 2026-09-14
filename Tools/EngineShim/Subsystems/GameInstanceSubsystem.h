// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class FSubsystemCollectionBase;

class UGameInstanceSubsystem : public UObject
{
public:
	using Super = UGameInstanceSubsystem; ///< see GameFramework/HUD.h

	virtual void Initialize(FSubsystemCollectionBase& Collection);
	virtual void Deinitialize();
};
