// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "UObject/Object.h"

class UEngine : public UObject
{
public:
	void AddOnScreenDebugMessage(uint64 Key, float TimeToDisplay, const FColor& DisplayColor,
								 const FString& DebugMessage);
	UWorld* GetWorld() const;
};

extern UEngine* GEngine;


