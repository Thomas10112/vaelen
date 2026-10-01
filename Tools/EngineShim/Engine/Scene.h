// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 23.03 (2026-10-01), a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Engine/Scene.h - FPostProcessSettings, of which only the
// members the sky's grade sets are here. Every value is an override only when its
// bOverride_ bit is set; the real struct has hundreds more of each.
#pragma once
#include "CoreMinimal.h"

enum EAutoExposureMethod : int
{
	AEM_Histogram,
	AEM_Basic,
	AEM_Manual,
};

struct FPostProcessSettings
{
	uint8 bOverride_AutoExposureMethod : 1;
	uint8 bOverride_AutoExposureMinBrightness : 1;
	uint8 bOverride_AutoExposureMaxBrightness : 1;
	uint8 bOverride_BloomIntensity : 1;
	uint8 bOverride_AmbientOcclusionIntensity : 1;
	uint8 bOverride_MotionBlurAmount : 1;
	uint8 bOverride_VignetteIntensity : 1;
	uint8 bOverride_ColorContrast : 1;
	EAutoExposureMethod AutoExposureMethod =
		AEM_Histogram; // TEnumAsByte in the engine; assignable from the enum either way
	float AutoExposureMinBrightness = 0.0f;
	float AutoExposureMaxBrightness = 0.0f;
	float BloomIntensity = 0.0f;
	float AmbientOcclusionIntensity = 0.0f;
	float MotionBlurAmount = 0.0f;
	float VignetteIntensity = 0.0f;
	FVector4 ColorContrast;
};
