// VAELEN - parse-only stand-in for Unreal's Templates/PimplPtr.h (ADR-0134).
//
// TPimplPtr exists in the engine for ONE reason and it is the reason 14.08
// needs it: a UCLASS that holds a pointer to an incomplete type. TUniquePtr
// cannot do that, because TDefaultDelete deletes through the type and
// UnrealHeaderTool writes code that instantiates that deleter in a translation
// unit which has only seen the forward declaration - the generated destructor,
// and DEFINE_VTABLE_PTR_HELPER_CTOR, which is emitted whatever the class
// declares. TPimplPtr binds its deleter at CONSTRUCTION, where the type is
// whole, and calls it through a pointer afterwards, so nothing downstream ever
// needs the definition.
//
// Narrow on purpose: only what Source/VaelenGame uses. The parse must be a
// SUBSET of the truth, never a superset - a member invented here would compile
// in CI and fail on the owner's machine, which is the whole failure mode this
// directory exists to prevent.
#pragma once

#include "CoreMinimal.h"

/// The engine's is a template parameter with two modes; VaelenGame uses the
/// default, which is the non-copyable one.
enum class EPimplPtrMode : unsigned char
{
	NoCopy,
	DeepCopy
};

template <typename T, EPimplPtrMode Mode = EPimplPtrMode::NoCopy>
struct TPimplPtr
{
	TPimplPtr() = default;
	TPimplPtr(decltype(nullptr)) {}
	TPimplPtr(const TPimplPtr&) = delete;
	TPimplPtr& operator=(const TPimplPtr&) = delete;
	TPimplPtr(TPimplPtr&& Other) : Ptr(Other.Ptr), Kill(Other.Kill)
	{
		Other.Ptr = nullptr;
		Other.Kill = nullptr;
	}
	TPimplPtr& operator=(TPimplPtr&& Other)
	{
		Undo();
		Ptr = Other.Ptr;
		Kill = Other.Kill;
		Other.Ptr = nullptr;
		Other.Kill = nullptr;
		return *this;
	}
	/// THE POINT: through a function pointer taken where T was complete, so
	/// this destructor compiles wherever it is instantiated.
	~TPimplPtr() { Undo(); }

	bool IsValid() const { return Ptr != nullptr; }
	explicit operator bool() const { return Ptr != nullptr; }
	T* Get() const { return Ptr; }
	T* operator->() const { return Ptr; }
	T& operator*() const { return *Ptr; }

	/// Only the shim needs these two public; the engine's are private.
	T* Ptr = nullptr;
	void (*Kill)(T*) = nullptr;

private:
	void Undo()
	{
		if (Kill != nullptr && Ptr != nullptr)
		{
			Kill(Ptr);
		}
		Ptr = nullptr;
		Kill = nullptr;
	}
};

template <typename T, EPimplPtrMode Mode = EPimplPtrMode::NoCopy, typename... TArgs>
TPimplPtr<T, Mode> MakePimpl(TArgs&&... Args)
{
	TPimplPtr<T, Mode> Out;
	Out.Ptr = new T(static_cast<TArgs&&>(Args)...);
	Out.Kill = [](T* What) { delete What; };
	return Out;
}
