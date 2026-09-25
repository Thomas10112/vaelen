// VAELEN - VaelenWalk
// Phase 19 task 19.06: a sky over a map that has none. See VaelenSky.h.
//
// STATUS: UNVERIFIED (engine) - written and PARSED against Tools/EngineShim on
// 2026-09-25, not yet built by UnrealBuildTool nor run: sitting S2 builds it.
#include "VaelenSky.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"

AVaelenSky::AVaelenSky()
{
	PrimaryActorTick.bCanEverTick = false;
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	RootComponent = Sun;
	Sun->SetAtmosphereSunLight(true);
	Sun->SetIntensity(10.0f);
	Sun->SetLightColor(FLinearColor(1.0f, 0.95f, 0.9f));
	Sun->SetMobility(EComponentMobility::Movable);
	Light = CreateDefaultSubobject<USkyLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Sun);
	Light->bRealTimeCapture = true;
	Air = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Air"));
	Air->SetupAttachment(Sun);
	Haze = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Haze"));
	Haze->SetupAttachment(Sun);
	Haze->SetFogDensity(0.02f);
}

void AVaelenSky::Aim(const Vaelen::Scene::Sun& Where, bool bEquatorIsPlusY)
{
	// The sun's bearing in the map's frame, degrees from +X towards the
	// equator's side: 900 -> 0, 1800 -> +-90, 2700 -> +-180. The light shines
	// the other way, so its yaw is that bearing plus 180; its pitch is the
	// elevation, downwards.
	const double Bearing = static_cast<double>(Where.Azimuth - 900) / 10.0 * (bEquatorIsPlusY ? 1.0 : -1.0);
	const double Elevation = static_cast<double>(Where.Elevation) / 10.0;
	Sun->SetWorldRotation(FRotator(-Elevation, Bearing + 180.0, 0.0));
	// Night: the sun under the ground gives nothing; the sky light carries
	// what little there is.
	Sun->SetIntensity(Where.Elevation > 0 ? 10.0f : 0.0f);
}

void AVaelenSky::Recapture()
{
	Light->RecaptureSky();
}
