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
	void SetupAttachment(USceneComponent* Parent);
	void SetMobility(EComponentMobility Mobility);
	void SetCollisionEnabled(ECollisionEnabled Enabled);
	void SetCastShadow(bool bCast);
};
