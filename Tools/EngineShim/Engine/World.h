// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
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
	/// 19.06 BELIEF: the local player's controller, which Vaelen.Walk places on the ground.
	class APlayerController* GetFirstPlayerController() const;

	/// 19.02 BELIEF: Vaelen.Probe compares these traces with the scene's own heights.
	bool LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
								  ECollisionChannel TraceChannel,
								  const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam) const;
};
