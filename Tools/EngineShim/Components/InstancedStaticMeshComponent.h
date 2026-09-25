// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Components/InstancedStaticMeshComponent.h.
#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;

/// Split out of HierarchicalInstancedStaticMeshComponent.h in 19.02: Phase 19
/// draws houses and figures as plain ISMs, and UE includes the two apart.
/// AddInstances returns the indices it placed; SetCustomDataValue takes an
/// index, a channel, a value and a flag.
class UInstancedStaticMeshComponent : public USceneComponent
{
public:
	int32 NumCustomDataFloats = 0;

	TArray<int32> AddInstances(const TArray<FTransform>& Transforms, bool bShouldReturnIndices,
							   bool bWorldSpace = false);
	int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false);
	bool SetCustomDataValue(int32 InstanceIndex, int32 CustomDataIndex, float CustomDataValue,
							bool bMarkRenderStateDirty = false);
	void ClearInstances();
	int32 GetInstanceCount() const;
	void SetStaticMesh(UStaticMesh* Mesh);
	void SetMaterial(int32 ElementIndex, UMaterialInterface* Material);
	void MarkRenderStateDirty();
};
