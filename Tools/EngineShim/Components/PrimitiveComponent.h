// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// Stands for UE 5.6's Components/PrimitiveComponent.h - the shim audit of
// 2026-09-27: SetCollisionEnabled, SetCastShadow and the channel responses are
// a PRIMITIVE's verbs in the engine, and this shim had them on every scene
// component, so a call on a light or a camera parsed here and fails there.
#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class UPrimitiveComponent : public USceneComponent
{
public:
	void SetCollisionEnabled(ECollisionEnabled Enabled);
	void SetCastShadow(bool bCast);
	/// 19.11b BELIEF: per channel, for the whole component.
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);
};
