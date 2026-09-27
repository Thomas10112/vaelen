// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 22.01 BELIEF: stands for UE 5.6's TimerManager.h - ONE verb of it, the
// next-tick deferral the front end uses to draw a loading page before a
// Begin that blocks for seconds. Not a Tick: it fires once, on the frame after
// the key, and the world moves on that Begin as it would on the console's.
#pragma once

#include "CoreMinimal.h"

class FTimerManager
{
public:
	template <class UserClass>
	void SetTimerForNextTick(UserClass* InObj, void (UserClass::*InMethod)())
	{
		(void)InObj;
		(void)InMethod;
	}
};
