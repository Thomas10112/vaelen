// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"

class UInputComponent : public UObject
{
public:
	using Super = UInputComponent;

	/// The engine's is a family of overloads over FInputChord and delegate
	/// types. Narrowed to the one shape this project uses - a key, an event,
	/// an object and one of its methods - because a template that swallowed
	/// anything would stop catching the mistake this shim exists for.
	template <typename UserClass>
	void BindKey(const FKey& Key, EInputEvent Event, UserClass* Object, void (UserClass::*Method)());
};
