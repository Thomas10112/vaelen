// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's ProceduralMeshComponent.h (plugin ProceduralMeshComponent).
#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"

class UMaterialInterface;

struct FProcMeshTangent
{
	FVector TangentX;
	bool bFlipTangentY = false;

	FProcMeshTangent() = default;
	FProcMeshTangent(float X, float Y, float Z);
	FProcMeshTangent(FVector InTangentX, bool bInFlipTangentY);
};

/// A mesh built from arrays at run time. Believed from UE 5.x: eight
/// parameters and a defaulted ninth, collision decided per section - the
/// argument count is exactly what a mutation of 19.02 gets wrong.
class UProceduralMeshComponent : public USceneComponent
{
public:
	bool bUseComplexAsSimpleCollision = true;
	bool bUseAsyncCooking = false;

	void CreateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices,
									   const TArray<int32>& Triangles, const TArray<FVector>& Normals,
									   const TArray<FVector2D>& UV0, const TArray<FLinearColor>& VertexColors,
									   const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision,
									   bool bSRGBConversion = false);
	void ClearMeshSection(int32 SectionIndex);
	void ClearAllMeshSections();
	void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);
	int32 GetNumSections() const;
	void SetMaterial(int32 ElementIndex, UMaterialInterface* Material);
};
