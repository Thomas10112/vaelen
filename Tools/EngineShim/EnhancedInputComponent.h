// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's EnhancedInputComponent.h (plugin EnhancedInput).
#pragma once

#include "Components/InputComponent.h"
#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputActionValue.h"

/// Flags in the engine, hence the powers of two.
enum class ETriggerEvent : uint8
{
	None = 0,
	Triggered = 1 << 0,
	Started = 1 << 1,
	Ongoing = 1 << 2,
	Canceled = 1 << 3,
	Completed = 1 << 4,
};

struct FEnhancedInputActionEventBinding
{
};

/// Three handler shapes and no others: none, a value, an instance. Narrowed
/// here as InputComponent.h narrows BindKey, so a handler of any other shape
/// is an error in the parse as it is in the engine.
class UEnhancedInputComponent : public UInputComponent
{
public:
	template <typename UserClass>
	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent,
												 UserClass* Object, void (UserClass::*Func)());
	template <typename UserClass>
	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent,
												 UserClass* Object,
												 void (UserClass::*Func)(const FInputActionValue&));
	template <typename UserClass>
	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent,
												 UserClass* Object,
												 void (UserClass::*Func)(const FInputActionInstance&));
	void ClearActionBindings();
};
