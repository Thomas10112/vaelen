// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's EnhancedInputSubsystems.h (plugin EnhancedInput).
#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "UObject/Object.h"

struct FModifyContextOptions
{
	bool bIgnoreAllPressedKeysUntilRelease = true;
};

class UEnhancedInputLocalPlayerSubsystem : public UObject
{
public:
	void AddMappingContext(const UInputMappingContext* MappingContext, int32 Priority,
						   const FModifyContextOptions& ContextOptions = FModifyContextOptions());
	void RemoveMappingContext(const UInputMappingContext* MappingContext,
							  const FModifyContextOptions& ContextOptions = FModifyContextOptions());
	void ClearAllMappings();
};
