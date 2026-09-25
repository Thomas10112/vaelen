// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

enum class EComponentMobility : uint8
{
	Static,
	Stationary,
	Movable,
};

enum class ECollisionEnabled : uint8
{
	NoCollision,
	QueryOnly,
	PhysicsOnly,
	QueryAndPhysics,
};

class USceneComponent : public UObject
{
public:
	/// 19.02 BELIEF: the socket name, which a spring arm's camera is attached by.
	void SetupAttachment(USceneComponent* Parent, FName SocketName = FName());
	void SetRelativeRotation(const FRotator& How);
	void SetWorldRotation(const FRotator& How);
	void SetMobility(EComponentMobility Mobility);
	void SetCollisionEnabled(ECollisionEnabled Enabled);
	void SetCastShadow(bool bCast);
	void DestroyComponent(bool bPromoteChildren = false);
	void RegisterComponent();
	void SetWorldLocation(const FVector& Where);
	void SetRelativeLocation(const FVector& Where);
};
