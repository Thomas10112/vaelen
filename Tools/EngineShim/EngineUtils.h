// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

/// Enough of the actor iterator for `for (TActorIterator<A> It(W); It; ++It)`
/// to parse: a constructor from a world, a bool test, a pre-increment and a
/// dereference to the actor.
template <typename T>
class TActorIterator
{
public:
	explicit TActorIterator(UWorld* InWorld) : World_(InWorld) {}
	explicit operator bool() const { return false; }
	TActorIterator& operator++() { return *this; }
	T* operator*() const { return nullptr; }
	T* operator->() const { return nullptr; }

private:
	UWorld* World_ = nullptr;
};
