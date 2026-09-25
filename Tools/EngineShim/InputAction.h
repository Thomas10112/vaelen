// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's InputAction.h (plugin EnhancedInput).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

enum class EInputActionValueType : uint8
{
	Boolean,
	Axis1D,
	Axis2D,
	Axis3D,
};

/// A data asset in the engine. ADR-0157 makes them at run time with
/// NewObject rather than as .uasset files, so this is the whole contract.
class UInputAction : public UObject
{
public:
	EInputActionValueType ValueType = EInputActionValueType::Boolean;
};
