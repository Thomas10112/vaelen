// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "UObject/Object.h"

/// Forward-declared and NOT included, as in the engine's Engine/Engine.h: a
/// file that turns VertexColorMaterial into a UMaterialInterface* needs
/// Materials/Material.h itself, or MSVC sees an incomplete UMaterial (the
/// shim audit of 2026-09-27; this header used to include it and hid that).
class UMaterial;

class UEngine : public UObject
{
public:
	void AddOnScreenDebugMessage(uint64 Key, float TimeToDisplay, const FColor& DisplayColor,
								 const FString& DebugMessage);
	UWorld* GetWorld() const;
	/// The font a HUD draws text with when it ships no asset of its own, which
	/// is what 14.09's page of Canvas text does.
	UFont* GetSmallFont() const;
	/// 22.01 BELIEF: a console line run as if typed - the front end's way to
	/// the verbs the host already has (Vaelen.Play, Vaelen.Walk, Vaelen.Load)
	/// without naming the module that holds them. The engine's third
	/// parameter, the output device, defaults to the log and is omitted here.
	bool Exec(UWorld* InWorld, const TCHAR* Cmd);

	/// 19.02 BELIEF: the engine's own material that shows vertex colours -
	/// the asset-free way to paint a procedural mesh (ADR-0157).
	TObjectPtr<UMaterial> VertexColorMaterial;
};

extern UEngine* GEngine;
