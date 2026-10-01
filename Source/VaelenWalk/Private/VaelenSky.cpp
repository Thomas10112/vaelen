// VAELEN - VaelenWalk
// Phase 19 task 19.06: a sky over a map that has none. Phase 23 task 23.03: lit. See VaelenSky.h.
//
// STATUS: UNVERIFIED (engine) since 23.03 (2026-10-01) - parsed against
// Tools/EngineShim, not yet built: sitting S5 builds it. Build b1001 compiled
// the 19.06 sky as it stood.
#include "VaelenSky.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Scene.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/// The grade's five settings per look level (23.03): what Vaelen.Look prints.
	constexpr float AmbientOcclusionOf(int32 Level)
	{
		return Level == 0 ? 0.0f : 0.6f;
	}
	constexpr float ExposureMinLux = 0.5f;
	constexpr float ExposureMaxLux = 4.0f;
	constexpr float BloomOf(int32 Level)
	{
		return Level == 0 ? 0.0f : 0.3f;
	}
} // namespace

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
	// The air (23.03): thinner than 19.06's 0.02, which greyed the far ring
	// at 30 m; falling off with height so the valleys hold it and the hills
	// stand out of it; starting past the walker's own tile; never opaque, so
	// the ridge line is always there.
	Haze = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Haze"));
	Haze->SetupAttachment(Sun);
	Haze->SetFogDensity(0.012f);
	Haze->SetFogHeightFalloff(0.15f);
	Haze->SetStartDistance(1500.0f);
	Haze->SetFogMaxOpacity(0.85f);
	Haze->SetDirectionalInscatteringExponent(8.0f);
	// The grade (23.03): unbound, so it is the whole screen's wherever the
	// walker stands. Exposure held between half a lux and four so dawn is dim
	// and noon is bright instead of both being auto-levelled to the same
	// grey; a little bloom; ambient occlusion so a wall meets its ground; no
	// motion blur (the T400 pays for it and a walk at 500 cm/s needs none);
	// a vignette and a tenth of contrast. Every other setting the engine's.
	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(Sun);
	Grade->bUnbound = true;
	Grade->bEnabled = true;
	Grade->Settings.bOverride_AutoExposureMethod = true;
	Grade->Settings.AutoExposureMethod = AEM_Histogram;
	Grade->Settings.bOverride_AutoExposureMinBrightness = true;
	Grade->Settings.AutoExposureMinBrightness = ExposureMinLux;
	Grade->Settings.bOverride_AutoExposureMaxBrightness = true;
	Grade->Settings.AutoExposureMaxBrightness = ExposureMaxLux;
	Grade->Settings.bOverride_BloomIntensity = true;
	Grade->Settings.bOverride_AmbientOcclusionIntensity = true;
	Grade->Settings.bOverride_MotionBlurAmount = true;
	Grade->Settings.MotionBlurAmount = 0.0f;
	Grade->Settings.bOverride_VignetteIntensity = true;
	Grade->Settings.VignetteIntensity = 0.3f;
	Grade->Settings.bOverride_ColorContrast = true;
	Grade->Settings.ColorContrast = FVector4(1.1, 1.1, 1.1, 1.1);
	// The clouds (23.03): the engine's own simple cloud material, which ships
	// with every UE 5.6 install (Tools/check_cook.py holds the package to it);
	// a layer from 2 km up, 4 km thick. Hidden until Vaelen.Look 2.
	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Sun);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMaterial(
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	CloudPaint = CloudMaterial.Succeeded() ? CloudMaterial.Object : nullptr;
	if (CloudPaint != nullptr)
	{
		Clouds->SetMaterial(CloudPaint);
	}
	Clouds->SetLayerBottomAltitude(2.0f);
	Clouds->SetLayerHeight(4.0f);
	SetLook(1);
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
	// What the height lights (23.03, Scene::LightOf): amber and dim at the
	// horizon, white and ten lux from thirty degrees up, nothing under it
	// (the review of 2026-09-27: at 0 the walk opened black, and a sun under
	// the ground gives nothing whatever its lux). The sky light scales with
	// it, and the fog takes the sun's colour along its direction and the
	// sky's blue-grey everywhere else, both dimmed with the sky light.
	const Vaelen::Scene::SunLight L = Vaelen::Scene::LightOf(Where);
	const FLinearColor SunColour(static_cast<float>(L.R) / 255.0f, static_cast<float>(L.G) / 255.0f,
								 static_cast<float>(L.B) / 255.0f);
	const float Lux = static_cast<float>(L.Lux10) / 10.0f;
	const float Sky = static_cast<float>(L.SkyMilli) / 1000.0f;
	Sun->SetIntensity(Lux);
	Sun->SetLightColor(SunColour);
	Light->SetIntensity(Sky);
	Haze->SetFogInscatteringColor(FLinearColor(0.45f * Sky, 0.55f * Sky, 0.72f * Sky));
	Haze->SetDirectionalInscatteringColor(SunColour * (Lux / 10.0f));
}

void AVaelenSky::Recapture()
{
	Light->RecaptureSky();
}

FVaelenLook AVaelenSky::SetLook(int32 Level)
{
	Level = Level < 0 ? 0 : (Level > 2 ? 2 : Level);
	FVaelenLook L;
	L.Level = Level;
	L.bClouds = Level == 2 && CloudPaint != nullptr;
	L.bVolumetricFog = Level == 2;
	L.AmbientOcclusion = AmbientOcclusionOf(Level);
	L.ExposureMin = ExposureMinLux;
	L.ExposureMax = ExposureMaxLux;
	L.Bloom = BloomOf(Level);
	Clouds->SetVisibility(L.bClouds);
	Haze->SetVolumetricFog(L.bVolumetricFog);
	Grade->Settings.AmbientOcclusionIntensity = L.AmbientOcclusion;
	Grade->Settings.BloomIntensity = L.Bloom;
	Look_ = L;
	return L;
}
