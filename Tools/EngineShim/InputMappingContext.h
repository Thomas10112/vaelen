// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's InputMappingContext.h (plugin EnhancedInput).
#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputModifiers.h"
#include "UObject/Object.h"

struct FEnhancedActionKeyMapping
{
	const UInputAction* Action = nullptr;
	FKey Key;
	TArray<TObjectPtr<UInputModifier>> Modifiers;
};

class UInputMappingContext : public UObject
{
public:
	/// Not const: a context made at run time is filled by mapping keys into it.
	FEnhancedActionKeyMapping& MapKey(const UInputAction* Action, FKey ToKey);
	void UnmapAll();
};
