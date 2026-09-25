// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's InputModifiers.h (plugin EnhancedInput).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class UInputModifier : public UObject
{
};

/// Z/Q/S/D and the arrows are four keys on one Axis2D action: S and Q are
/// negated, Z and S swizzled onto Y.
class UInputModifierNegate : public UInputModifier
{
public:
	bool bX = true;
	bool bY = true;
	bool bZ = true;
};

enum class EInputAxisSwizzle : uint8
{
	YXZ,
	ZYX,
	XZY,
	YZX,
	ZXY,
};

class UInputModifierSwizzleAxis : public UInputModifier
{
public:
	EInputAxisSwizzle Order = EInputAxisSwizzle::YXZ;
};
