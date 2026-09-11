// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
//
// The one engine class the DRAWER touches, and therefore the one whose
// signatures are worth being fussy about. AddInstances returns the indices it
// placed; SetCustomDataValue takes an index, a channel, a value and a flag.
// Getting either of those wrong here would let a real defect through.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

class UStaticMesh;
class UMaterialInterface;

class UInstancedStaticMeshComponent : public USceneComponent
{
public:
	int32 NumCustomDataFloats = 0;

	TArray<int32> AddInstances(const TArray<FTransform>& Transforms, bool bShouldReturnIndices,
							   bool bWorldSpace = false);
	bool SetCustomDataValue(int32 InstanceIndex, int32 CustomDataIndex, float CustomDataValue,
							bool bMarkRenderStateDirty = false);
	void ClearInstances();
	void SetStaticMesh(UStaticMesh* Mesh);
	void SetMaterial(int32 ElementIndex, UMaterialInterface* Material);
	void MarkRenderStateDirty();
};

class UHierarchicalInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
};
