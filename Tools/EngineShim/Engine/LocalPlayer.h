// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 19.02, a BELIEF until a sitting compiles a use of it (ADR-0157, Tools/shim_beliefs.txt):
// stands for UE 5.6's Engine/LocalPlayer.h.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class ULocalPlayer : public UObject
{
public:
	/// Both forms exist in the engine; the static one is what the Enhanced
	/// Input documentation shows.
	template <typename T>
	static T* GetSubsystem(const ULocalPlayer* LocalPlayer)
	{
		return nullptr;
	}
	template <typename T>
	T* GetSubsystem() const
	{
		return nullptr;
	}
};
