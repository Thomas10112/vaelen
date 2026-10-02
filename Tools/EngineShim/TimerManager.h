// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
// 22.01 BELIEF: stands for UE 5.6's TimerManager.h - ONE verb of it, the
// next-tick deferral the front end uses to draw a loading page before a
// Begin that blocks for seconds. Not a Tick: it fires once, on the frame after
// the key, and the world moves on that Begin as it would on the console's.
#pragma once

#include "CoreMinimal.h"

/// 23.06 BELIEF: the handle a looping timer is set and cleared by.
struct FTimerHandle
{
	uint64 Handle = 0;
};

class FTimerManager
{
public:
	/// 23.06 BELIEF: a looping timer - the one presentation clock of the walk,
	/// which replants the grass around the walker and reads nothing of the world.
	template <class UserClass>
	void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, void (UserClass::*InMethod)(), float InRate,
				  bool InbLoop = false, float InFirstDelay = -1.0f)
	{
		(void)InOutHandle;
		(void)InObj;
		(void)InMethod;
		(void)InRate;
		(void)InbLoop;
		(void)InFirstDelay;
	}
	void ClearTimer(FTimerHandle& InHandle);
	template <class UserClass>
	void SetTimerForNextTick(UserClass* InObj, void (UserClass::*InMethod)())
	{
		(void)InObj;
		(void)InMethod;
	}
};
