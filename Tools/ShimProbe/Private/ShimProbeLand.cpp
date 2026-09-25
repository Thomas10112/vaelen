// VAELEN - Tools/ShimProbe (19.02). Not built; parsed. README.md says why.
#include "ShimProbeLand.h"

#include "CollisionQueryParams.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"

AShimProbeLand::AShimProbeLand()
{
	PrimaryActorTick.bCanEverTick = false;
	Ground = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Ground"));
	Ground->bUseComplexAsSimpleCollision = true;
	Ground->bUseAsyncCooking = false;
	Houses = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Houses"));
	Houses->SetupAttachment(Ground);
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetAtmosphereSunLight(true);
	Sun->SetIntensity(10.0f);
	Sun->SetLightColor(FLinearColor(1.0f, 0.95f, 0.9f));
	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->bRealTimeCapture = true;
	Air = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Air"));
	Haze = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Haze"));
	Haze->SetFogDensity(0.02f);
	OnViewsTaken.AddUObject(this, &AShimProbeLand::Rebuild);
}

void AShimProbeLand::Build(int32 Side, int32 StepCm)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colours;
	TArray<FProcMeshTangent> Tangents;
	for (int32 Y = 0; Y < Side; ++Y)
	{
		for (int32 X = 0; X < Side; ++X)
		{
			Vertices.Add(FVector(X * StepCm, Y * StepCm, 0.0));
			Normals.Add(FVector(0.0, 0.0, 1.0));
			UV0.Add(FVector2D(X, Y));
			Colours.Add(FLinearColor(0.3f, 0.5f, 0.2f));
			Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
		}
	}
	Triangles.Add(0);
	Triangles.Add(Side);
	Triangles.Add(1);
	Ground->ClearAllMeshSections();
	Ground->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colours, Tangents, true);
	Ground->SetMaterial(0, GEngine->VertexColorMaterial);
	Houses->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, 0.0), FVector(1.0, 1.0, 1.0)));
	OnViewsTaken.Broadcast();
}

float AShimProbeLand::Probe(double X, double Y) const
{
	FHitResult Hit;
	const FVector From(X, Y, 100000.0);
	const FVector To(X, Y, -100000.0);
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility) && Hit.bBlockingHit)
	{
		return static_cast<float>(Hit.ImpactPoint.Z);
	}
	return 0.0f;
}

void AShimProbeLand::Rebuild()
{
	Sky->RecaptureSky();
}
