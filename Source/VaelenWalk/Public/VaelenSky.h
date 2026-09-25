// VAELEN - VaelenWalk
// Phase 19 task 19.06: a sky over a map that has none.
//
// `/Engine/Maps/Entry` has no light. This actor makes one in C++ - a sun, a
// sky light, an atmosphere and a height fog - so that the walk needs no
// asset (ADR-0157). The sun stands where Vaelen/Scene/Sky.h says (19.09):
// from the world's hours and its day of the year, never from a frame clock,
// re-aimed when the views are retaken and at no other moment.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
// That a directional light's rotation is (pitch = -elevation, yaw = the way
// it shines) is the belief here; a sun that rises in the west is the report.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vaelen/Scene/Sky.h"

#include "VaelenSky.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

UCLASS()
class VAELENWALK_API AVaelenSky : public AActor
{
	GENERATED_BODY()

public:
	AVaelenSky();

	/// Aims the sun. Azimuth 900 is east (+X), 2700 west (-X); 1800 is
	/// towards the equator, which is +Y for a row above the map's middle and
	/// -Y below it (Sky.h measures a row by its distance to the equator, so
	/// the sun's side is the actor's to say). Elevation in tenths of a degree
	/// above the horizon; at or below 0 the sun is under the ground and the
	/// scene is night.
	void Aim(const Vaelen::Scene::Sun& Where, bool bEquatorIsPlusY);

	/// The sky light again, after the sun moved.
	void Recapture();

private:
	UDirectionalLightComponent* Sun = nullptr;
	USkyLightComponent* Light = nullptr;
	USkyAtmosphereComponent* Air = nullptr;
	UExponentialHeightFogComponent* Haze = nullptr;
};
