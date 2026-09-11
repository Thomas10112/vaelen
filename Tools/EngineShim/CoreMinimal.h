// VAELEN - Tools/EngineShim. ADR-0134.
//
// ENOUGH OF UNREAL FOR A COMPILER TO READ VaelenPresentation, AND NOT ONE LINE
// MORE. This is not a reimplementation of the engine and must never grow into
// one: every declaration here exists because a file in VaelenPresentation names
// it, and anything nobody names does not belong.
//
// WHAT A GREEN PARSE PROVES, EXACTLY
//
//   The file is a well-formed C++20 program. Names resolve. Signatures match.
//   A member that was renamed is gone, an argument that was added is counted,
//   a type that changed is caught.
//
// WHAT IT DOES NOT PROVE
//
//   That the module BUILDS under UnrealBuildTool. That UHT accepts the UCLASS.
//   That the engine's real types behave as these do. That anything DRAWS.
//   VaelenPresentation stays UNVERIFIED until an editor has run it, and
//   ADR-0134 would be wrong if it changed a single STATUS line.
//
// THE DRIFT HAZARD, WHICH IS REAL AND IS THE PRICE OF THIS FILE
//
//   A shim that has drifted from the engine gives a green light to code that
//   does not build, which is worse than no light at all. So: THE UBT BUILD IS
//   THE AUTHORITY. When the two disagree, the shim is wrong, and the fix is
//   here - never a change to VaelenPresentation to please this file.
//
//   Signatures are therefore copied narrow rather than convenient. Parameters
//   are the engine's types, not templates that would swallow anything; return
//   types are the engine's, not auto. A shim that accepts everything proves
//   nothing, and would be the comfortable version of this work.
#pragma once

// A GUARD, SO THIS CAN NEVER BE MISTAKEN FOR THE ENGINE.
//
// The one way this file could do real harm is by ending up on a REAL build's
// include path, where it would shadow Unreal's own CoreMinimal.h and let a
// module build against a toy. Only Tools/parse_engine_modules.py defines the
// symbol below, so any other build that reaches this header stops here instead
// of quietly succeeding.
#if !defined(VAELEN_SHIM_PARSE)
#	error "Tools/EngineShim is a parse-only stand-in for Unreal (ADR-0134) and must never be on a real build's include path. Only Tools/parse_engine_modules.py may include it."
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Unreal's integer names, which the kernel's own Vaelen::uintNN deliberately
// shadow nowhere - the presentation files use both and the distinction matters,
// so it is reproduced rather than smoothed over.
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using TCHAR = wchar_t;
#define TEXT(x) L##x

// UHT's markers. They annotate; they do not generate anything a parser needs.
#define UCLASS(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define USTRUCT(...)
#define UENUM(...)
#define GENERATED_BODY(...)
#define GENERATED_UCLASS_BODY(...)
#define meta(...)

#define KINDA_SMALL_NUMBER 1.e-4f
#define SMALL_NUMBER 1.e-8f

// ---------------------------------------------------------------- containers

template <typename T>
class TArray
{
public:
	TArray() = default;
	void Add(const T& Item) { Items.push_back(Item); }
	void AddUnique(const T& Item)
	{
		for (const T& Have : Items)
		{
			if (Have == Item)
			{
				return;
			}
		}
		Items.push_back(Item);
	}
	void Reserve(int32 Count) { Items.reserve(static_cast<std::size_t>(Count < 0 ? 0 : Count)); }
	void Empty() { Items.clear(); }
	int32 Num() const { return static_cast<int32>(Items.size()); }
	T& operator[](int32 Index) { return Items[static_cast<std::size_t>(Index)]; }
	const T& operator[](int32 Index) const { return Items[static_cast<std::size_t>(Index)]; }
	typename std::vector<T>::iterator begin() { return Items.begin(); }
	typename std::vector<T>::iterator end() { return Items.end(); }
	typename std::vector<T>::const_iterator begin() const { return Items.begin(); }
	typename std::vector<T>::const_iterator end() const { return Items.end(); }

private:
	std::vector<T> Items;
};

template <typename K, typename V>
class TMap
{
public:
	V& FindOrAdd(const K& Key) { return Slots[Key]; }
	V* Find(const K& Key)
	{
		auto It = Slots.find(Key);
		return It == Slots.end() ? nullptr : &It->second;
	}
	const V* Find(const K& Key) const
	{
		auto It = Slots.find(Key);
		return It == Slots.end() ? nullptr : &It->second;
	}
	int32 Num() const { return static_cast<int32>(Slots.size()); }

private:
	std::unordered_map<K, V> Slots;
};

template <typename T>
class TSet
{
public:
	void Add(const T& Item) { Items.insert(Item); }
	bool Contains(const T& Item) const { return Items.find(Item) != Items.end(); }
	int32 Num() const { return static_cast<int32>(Items.size()); }

private:
	std::unordered_set<T> Items;
};

/// A handle to a UObject. The only property of it this project relies on is
/// that it converts to the raw pointer wherever one is wanted, which is what
/// makes `DrawFolk(..., Folk, ...)` legal with a TObjectPtr member.
template <typename T>
class TObjectPtr
{
public:
	TObjectPtr() = default;
	TObjectPtr(T* In) : Ptr(In) {}
	TObjectPtr& operator=(T* In)
	{
		Ptr = In;
		return *this;
	}
	operator T*() const { return Ptr; }
	T* operator->() const { return Ptr; }
	T& operator*() const { return *Ptr; }
	bool operator==(std::nullptr_t) const { return Ptr == nullptr; }
	bool operator!=(std::nullptr_t) const { return Ptr != nullptr; }

private:
	T* Ptr = nullptr;
};

// ------------------------------------------------------------------- strings

class FString
{
public:
	FString() = default;
	FString(const TCHAR* In) : Text(In == nullptr ? L"" : In) {}
	FString& operator+=(const FString& Other)
	{
		Text += Other.Text;
		return *this;
	}
	const TCHAR* operator*() const { return Text.c_str(); }
	template <typename... Args>
	static FString Printf(const TCHAR* Format, Args... Rest);

private:
	std::wstring Text;
};

template <typename... Args>
FString FString::Printf(const TCHAR* Format, Args...)
{
	return FString(Format);
}

struct FCString
{
	static int32 Atoi(const TCHAR* Text) { return Text == nullptr ? 0 : 0; }
};

// -------------------------------------------------------------------- maths

struct FMath
{
	template <typename T>
	static T Clamp(T Value, T Low, T High)
	{
		return Value < Low ? Low : (Value > High ? High : Value);
	}
	template <typename T>
	static T Max(T A, T B)
	{
		return A > B ? A : B;
	}
	template <typename T>
	static T Min(T A, T B)
	{
		return A < B ? A : B;
	}
	static double Pow(double Base, double Exponent);
};

struct FRotator
{
	double Pitch = 0.0;
	double Yaw = 0.0;
	double Roll = 0.0;
	FRotator() = default;
	FRotator(double InPitch, double InYaw, double InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
	static const FRotator ZeroRotator;
};

struct FVector
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	FVector() = default;
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
	FVector operator+(const FVector& Other) const { return FVector(X + Other.X, Y + Other.Y, Z + Other.Z); }
	FVector operator-(const FVector& Other) const { return FVector(X - Other.X, Y - Other.Y, Z - Other.Z); }
	FVector operator*(double Scale) const { return FVector(X * Scale, Y * Scale, Z * Scale); }
	double Size() const;
	FRotator Rotation() const;
	static const FVector ZeroVector;
};

struct FTransform
{
	FTransform() = default;
	FTransform(const FRotator& InRotation, const FVector& InTranslation, const FVector& InScale)
		: Rotation(InRotation), Translation(InTranslation), Scale(InScale)
	{
	}
	FRotator Rotation;
	FVector Translation;
	FVector Scale;
};

struct FLinearColor
{
	float R = 0.0f;
	float G = 0.0f;
	float B = 0.0f;
	float A = 1.0f;
	constexpr FLinearColor() = default;
	constexpr FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
	constexpr bool operator==(const FLinearColor& Other) const
	{
		return R == Other.R && G == Other.G && B == Other.B && A == Other.A;
	}
};

struct FPlatformTime
{
	static double Seconds();
};

// ------------------------------------------------------------------ logging

enum class EShimVerbosity : uint8
{
	Fatal,
	Error,
	Warning,
	Display,
	Log,
	Verbose,
	All,
};

struct FShimLogCategory
{
};

#define DECLARE_LOG_CATEGORY_EXTERN(Name, Default, Compile) extern FShimLogCategory Name
#define DEFINE_LOG_CATEGORY(Name) FShimLogCategory Name
#define DEFINE_LOG_CATEGORY_STATIC(Name, Default, Compile) static FShimLogCategory Name

/// The CATEGORY and the arguments are both named, which is the point: an
/// argument that no longer exists, and a log category that was never declared,
/// are exactly the defects this whole exercise is here to catch. The verbosity
/// is pasted by the real macro too and is not a name that has to resolve.
namespace VaelenShim
{
	inline void Swallow(...) {}
}
#define UE_LOG(Category, Verbosity, Format, ...) VaelenShim::Swallow(&Category, Format, ##__VA_ARGS__)
