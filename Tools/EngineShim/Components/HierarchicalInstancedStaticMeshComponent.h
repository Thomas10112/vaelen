// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
//
// The one engine class the DRAWER touches, and therefore the one whose
// signatures are worth being fussy about. AddInstances returns the indices it
// placed; SetCustomDataValue takes an index, a channel, a value and a flag.
// Getting either of those wrong here would let a real defect through.
#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "CoreMinimal.h"

class UHierarchicalInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
};
