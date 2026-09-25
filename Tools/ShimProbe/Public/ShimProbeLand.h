// VAELEN - Tools/ShimProbe (19.02). Not built; parsed. README.md says why.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShimProbeLand.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

DECLARE_MULTICAST_DELEGATE(FShimProbeViewsTaken);

UCLASS()
class AShimProbeLand : public AActor
{
	GENERATED_BODY()

public:
	AShimProbeLand();

	void Build(int32 Side, int32 StepCm);
	float Probe(double X, double Y) const;

	FShimProbeViewsTaken OnViewsTaken;

private:
	void Rebuild();

	UProceduralMeshComponent* Ground = nullptr;
	UInstancedStaticMeshComponent* Houses = nullptr;
	UDirectionalLightComponent* Sun = nullptr;
	USkyLightComponent* Sky = nullptr;
	USkyAtmosphereComponent* Air = nullptr;
	UExponentialHeightFogComponent* Haze = nullptr;
};
