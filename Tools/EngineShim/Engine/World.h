// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"

class UGameInstance;

class UWorld : public UObject
{
public:
	template <typename T>
	T* SpawnActor()
	{
		static T Made;
		return &Made;
	}

	/// How a world reaches the subsystem that holds the simulation (14.08).
	/// Declared, not defined: nothing here is linked.
	UGameInstance* GetGameInstance() const;
};
