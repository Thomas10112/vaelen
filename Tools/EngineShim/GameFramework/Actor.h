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

	/// 19.11b: the engine's own accessor. RootComponent itself is PROTECTED
	/// below, as in GameFramework/Actor.h - a free function reaching it
	/// through an AActor* was accepted by this shim and is MSVC C2248 on
	/// the real one (found by the review of 2026-09-26).
	USceneComponent* GetRootComponent() const;

	UWorld* GetWorld() const;
	FTransform GetActorTransform() const;
	FVector GetActorLocation() const;
	void SetActorLocation(const FVector& Where);

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

	/// Components made after construction have to be handed to the actor, and
	/// the project's atlas actor rebuilds its layers every run.
	void AddInstanceComponent(USceneComponent* Component);
	void RemoveInstanceComponent(USceneComponent* Component);

	virtual void BeginPlay();

protected:
	/// Protected, as the engine declares it: an actor's own constructor may
	/// assign it (every actor here does); nothing outside the class reads it.
	USceneComponent* RootComponent = nullptr;
};
