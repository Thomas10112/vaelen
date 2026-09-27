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
	/// THE ENGINE CHECKS THE WORLD IN THIS CONSTRUCTOR (check(CurrentWorld),
	/// then it walks the world's levels): a null world is a fatal assert in
	/// every build with checks, and a null dereference in Shipping. A parse
	/// cannot see the order of a null test and an iterator; the caller must
	/// test the world FIRST (the shim audit of 2026-09-27 found two that did
	/// not). The parameter is const in the engine; the flags and the class
	/// default are omitted here because nothing of ours passes them.
	explicit TActorIterator(const UWorld* InWorld) : World_(InWorld) {}
	explicit operator bool() const { return false; }
	TActorIterator& operator++() { return *this; }
	T* operator*() const { return nullptr; }
	T* operator->() const { return nullptr; }

private:
	const UWorld* World_ = nullptr;
};
