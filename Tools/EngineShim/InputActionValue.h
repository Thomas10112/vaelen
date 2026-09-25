// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's InputActionValue.h (plugin EnhancedInput).
#pragma once

#include "CoreMinimal.h"

/// What an action carries. Get<T>() is specialised by the engine for bool,
/// float (Axis1D), FVector2D (Axis2D) and FVector (Axis3D).
struct FInputActionValue
{
	using Axis1D = float;
	using Axis2D = FVector2D;
	using Axis3D = FVector;

	template <typename T>
	T Get() const;
};

template <>
bool FInputActionValue::Get<bool>() const;
template <>
float FInputActionValue::Get<float>() const;
template <>
FVector2D FInputActionValue::Get<FVector2D>() const;
template <>
FVector FInputActionValue::Get<FVector>() const;

struct FInputActionInstance
{
	FInputActionValue GetValue() const;
};
