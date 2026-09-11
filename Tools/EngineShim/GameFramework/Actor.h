// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "UObject/Object.h"

struct FActorTickFunction
{
	bool bCanEverTick = false;
};

class UWorld;

class AActor : public UObject
{
public:
	/// UnrealHeaderTool writes `typedef AActor Super;` into each actor's
	/// generated header. GENERATED_BODY() is a no-op here and a macro cannot
	/// name a class's base, so Super is inherited from AActor instead.
	///
	/// THE LIMIT OF THAT, said rather than discovered: it is exactly right for
	/// an actor derived straight from AActor, which is every actor this project
	/// has. The day one derives from another, its Super would resolve one step
	/// too far up and this file is what gets fixed - never the actor.
	using Super = AActor;

	FActorTickFunction PrimaryActorTick;
	USceneComponent* RootComponent = nullptr;

	UWorld* GetWorld() const;

	/// The engine's real signature is a variadic forwarding template. Kept
	/// narrow here on purpose: this project only ever passes a name, and a
	/// template that swallowed anything would stop catching the class of
	/// mistake this shim exists for.
	template <typename T>
	T* CreateDefaultSubobject(const TCHAR* Name)
	{
		static T Made;
		return &Made;
	}

	virtual void BeginPlay();
};
