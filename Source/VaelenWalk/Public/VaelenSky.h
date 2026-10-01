// VAELEN - VaelenWalk
// Phase 19 task 19.06: a sky over a map that has none. Phase 23 task 23.03: lit.
//
// `/Engine/Maps/Entry` has no light. This actor makes one in C++ - a sun, a
// sky light, an atmosphere, a height fog, and since 23.03 the grade of the
// screen and the clouds - so that the walk needs no asset (ADR-0157). The sun
// stands where Vaelen/Scene/Sky.h says (19.09) and shines what it says
// (23.03, LightOf): from the world's hours and its day of the year, never
// from a frame clock, re-aimed when the views are retaken and at no other
// moment. The engine applies a light it did not invent.
//
// STATUS: UNVERIFIED (engine) since 23.03 (2026-10-01) - the light by height,
// the fog lit by the sun, the grade (UPostProcessComponent) and the clouds
// (UVolumetricCloudComponent) are parsed against Tools/EngineShim and not yet
// built: sitting S5 builds them. Build b1001 (d96a418) compiled the 19.06
// sky as it stood: a sun that rose in the east on the owner's machine.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vaelen/Scene/Sky.h"
#include "VaelenSky.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;

/// What one look level sets, so Vaelen.Look can print what it did rather than
/// what it meant to.
struct FVaelenLook
{
	int32 Level = 1;
	bool bClouds = false;
	bool bVolumetricFog = false;
	float AmbientOcclusion = 0.0f;
	float ExposureMin = 0.0f;
	float ExposureMax = 0.0f;
	float Bloom = 0.0f;
};

UCLASS()
class VAELENWALK_API AVaelenSky : public AActor
{
	GENERATED_BODY()

public:
	AVaelenSky();

	/// Aims the sun and lights it. Azimuth 900 is east (+X), 2700 west (-X);
	/// 1800 is towards the equator, which is +Y for a row above the map's
	/// middle and -Y below it (Sky.h measures a row by its distance to the
	/// equator, so the sun's side is the actor's to say). Elevation in tenths
	/// of a degree above the horizon; at or below 0 the sun is under the
	/// ground and the scene is night. The colour, the intensity, the sky
	/// light's scale and the fog's inscattering are LightOf's (23.03).
	void Aim(const Vaelen::Scene::Sun& Where, bool bEquatorIsPlusY);
	/// The sky light again, after the sun moved.
	void Recapture();

	/// 23.03: the look level, 0 to 2, held to that range. 0 is the T400 at
	/// ground level (no ambient occlusion, no volumetric fog, no clouds); 1
	/// the default (ambient occlusion, the grade); 2 the full air (volumetric
	/// fog and the clouds, which the T400 pays several milliseconds for).
	/// Returns what was set.
	FVaelenLook SetLook(int32 Level);
	FVaelenLook Look() const { return Look_; }
	/// Whether the engine's own cloud material was found on this disk; without
	/// it level 2 shows no clouds and says so.
	bool HasCloudMaterial() const { return CloudPaint != nullptr; }

private:
	UDirectionalLightComponent* Sun = nullptr;
	USkyLightComponent* Light = nullptr;
	USkyAtmosphereComponent* Air = nullptr;
	UExponentialHeightFogComponent* Haze = nullptr;
	UPostProcessComponent* Grade = nullptr;
	UVolumetricCloudComponent* Clouds = nullptr;
	UMaterialInterface* CloudPaint = nullptr;
	FVaelenLook Look_;
};
