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
#	error                                                                                                              \
		"Tools/EngineShim is a parse-only stand-in for Unreal (ADR-0134) and must never be on a real build's include path. Only Tools/parse_engine_modules.py may include it."
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Unreal's integer names, SPELLED AS UNREAL SPELLS THEM and not as <cstdint>
// would. This was wrong in the first version - uint64 was std::uint64_t, which
// is `unsigned long` on Linux while Unreal's is `unsigned long long` - and the
// thing that caught it was the project's own code:
//
//     Source/Vaelen/Private/Vaelen.cpp
//     static_assert(std::is_same_v<Vaelen::uint64, ::uint64>,
//                   "Vaelen::uint64 must be Unreal's uint64");
//
// A shim that had smoothed that over would have reported green on a file the
// real build rejects, which is the exact failure ADR-0134 names as worse than
// no light at all. The rule held: the code is the authority, the shim is what
// gets fixed.
using uint8 = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using uint64 = unsigned long long;
using int8 = signed char;
using int16 = signed short;
using int32 = int;
using int64 = long long;

using TCHAR = wchar_t;
#define TEXT(x) L##x

/// Real conversion macros build a temporary; here they only have to produce
/// something of the right shape for the call around them to typecheck.
///
/// Through a function, not as a bare (L""): a macro that throws its argument
/// away typechecks anything, including the day somebody hands one of these a
/// number or a pointer of the wrong width. These evaluate what they are given
/// and then answer an empty string, which is the honest answer here and the
/// one the calling code has to handle anyway (UObject/Object.h says the same
/// of NewObject).
namespace VaelenShim
{
	inline const wchar_t* Widen(const char*)
	{
		return L"";
	}
	inline const char* Narrow(const wchar_t*)
	{
		return "";
	}
} // namespace VaelenShim

#define UTF8_TO_TCHAR(x) (VaelenShim::Widen(x))
#define TCHAR_TO_UTF8(x) (VaelenShim::Narrow(x))
#define ANSI_TO_TCHAR(x) (VaelenShim::Widen(x))
#define TCHAR_TO_ANSI(x) (VaelenShim::Narrow(x))

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

/// Backed by std::deque and NOT std::vector, for one reason: std::vector<bool>
/// is specialised to a bit field, its operator[] returns a proxy, and binding
/// `bool&` to it does not compile. Unreal's TArray<bool> holds actual bools and
/// the project's atlas actor keeps one. Found by extending this shim to a
/// second module, which is the argument for extending it.
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
	/// A hint in the engine and nothing at all here: std::deque has no
	/// reserve, and a parse does not care how memory was arranged.
	void Reserve(int32 Count) { (void)Count; }
	void Empty() { Items.clear(); }
	void Reset() { Items.clear(); }
	void SetNum(int32 Count) { Items.resize(static_cast<std::size_t>(Count < 0 ? 0 : Count)); }
	void Init(const T& Value, int32 Count) { Items.assign(static_cast<std::size_t>(Count < 0 ? 0 : Count), Value); }
	void SetNumZeroed(int32 Count) { Items.assign(static_cast<std::size_t>(Count < 0 ? 0 : Count), T{}); }
	bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < Num(); }
	void RemoveAt(int32 Index)
	{
		if (IsValidIndex(Index))
		{
			Items.erase(Items.begin() + Index);
		}
	}
	bool IsEmpty() const { return Items.empty(); }
	int32 Num() const { return static_cast<int32>(Items.size()); }
	T& operator[](int32 Index) { return Items[static_cast<std::size_t>(Index)]; }
	const T& operator[](int32 Index) const { return Items[static_cast<std::size_t>(Index)]; }
	typename std::deque<T>::iterator begin() { return Items.begin(); }
	typename std::deque<T>::iterator end() { return Items.end(); }
	typename std::deque<T>::const_iterator begin() const { return Items.begin(); }
	typename std::deque<T>::const_iterator end() const { return Items.end(); }

private:
	std::deque<T> Items;
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
	/// The engine's signature, out-parameter included: TSet::Add reports
	/// whether the item was already there, and a shim that dropped that
	/// parameter would reject code the real build accepts.
	void Add(const T& Item, bool* bIsAlreadyInSetPtr = nullptr)
	{
		const bool bHad = Items.find(Item) != Items.end();
		if (bIsAlreadyInSetPtr != nullptr)
		{
			*bIsAlreadyInSetPtr = bHad;
		}
		Items.insert(Item);
	}
	bool Contains(const T& Item) const { return Items.find(Item) != Items.end(); }
	int32 Num() const { return static_cast<int32>(Items.size()); }

private:
	std::unordered_set<T> Items;
};

/// Unreal's owning pointer. Move-only, like the engine's.
template <typename T>
class TUniquePtr
{
public:
	TUniquePtr() = default;
	explicit TUniquePtr(T* In) : Ptr(In) {}
	TUniquePtr(const TUniquePtr&) = delete;
	TUniquePtr& operator=(const TUniquePtr&) = delete;
	TUniquePtr(TUniquePtr&& Other) : Ptr(Other.Ptr) { Other.Ptr = nullptr; }
	TUniquePtr& operator=(TUniquePtr&& Other)
	{
		Ptr = Other.Ptr;
		Other.Ptr = nullptr;
		return *this;
	}
	~TUniquePtr() { delete Ptr; }
	T* Get() const { return Ptr; }
	T* operator->() const { return Ptr; }
	T& operator*() const { return *Ptr; }
	explicit operator bool() const { return Ptr != nullptr; }
	void Reset(T* In = nullptr)
	{
		delete Ptr;
		Ptr = In;
	}
	T* Release()
	{
		T* Was = Ptr;
		Ptr = nullptr;
		return Was;
	}

private:
	T* Ptr = nullptr;
};

template <typename T, typename... Args>
TUniquePtr<T> MakeUnique(Args&&... Rest)
{
	return TUniquePtr<T>(new T(static_cast<Args&&>(Rest)...));
}

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

/// A class handed about as a value: what a game mode names its HUD and its
/// controller with. The engine's carries a UClass*; this carries the same and
/// nothing else, because what this file is for is catching the day somebody
/// assigns the wrong kind of class to one.
class UClass;

template <typename T>
class TSubclassOf
{
public:
	TSubclassOf() = default;
	TSubclassOf(UClass* In) : Class(In) {}

	UClass* Get() const { return Class; }

private:
	UClass* Class = nullptr;
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
	void Reset() { Text.clear(); }
	bool operator==(const FString& Other) const { return Text == Other.Text; }
	bool IsEmpty() const { return Text.empty(); }
	int32 Len() const { return static_cast<int32>(Text.size()); }
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

/// Unreal's interned name. ToString is what the project's code calls on it,
/// and keeping it a distinct type from FString is the point: the two are not
/// interchangeable in the engine and must not be here either.
class FName
{
public:
	FName() = default;
	FName(const TCHAR* In) : Text(In == nullptr ? L"" : In) {}
	FString ToString() const { return FString(Text.c_str()); }
	bool IsNone() const { return Text.empty(); }
	bool operator==(const FName& Other) const { return Text == Other.Text; }

private:
	std::wstring Text;
};

/// std::move by another name.
template <typename T>
constexpr typename std::remove_reference<T>::type&& MoveTemp(T&& Value) noexcept
{
	return static_cast<typename std::remove_reference<T>::type&&>(Value);
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

struct FVector2D
{
	double X = 0.0;
	double Y = 0.0;
	FVector2D() = default;
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
	FVector2D operator+(const FVector2D& Other) const { return FVector2D(X + Other.X, Y + Other.Y); }
	FVector2D operator-(const FVector2D& Other) const { return FVector2D(X - Other.X, Y - Other.Y); }
	FVector2D operator*(double Scale) const { return FVector2D(X * Scale, Y * Scale); }
	double Size() const;
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

	/// The engine's accessors, which is how the atlas actor builds a transform
	/// in pieces rather than in one constructor call.
	void SetLocation(const FVector& Where) { Translation = Where; }
	void SetScale3D(const FVector& How) { Scale = How; }
	void SetRotation(const FRotator& Which) { Rotation = Which; }
	FVector GetLocation() const { return Translation; }
	FVector GetScale3D() const { return Scale; }
	FVector TransformPosition(const FVector& Point) const;
	FVector TransformVector(const FVector& Direction) const;
	FVector InverseTransformPosition(const FVector& Point) const;
};

/// The 8-bit colour. Its named constants are used as values, so they exist as
/// values; nothing here is ever linked, so they are declared and not defined.
struct FColor
{
	uint8 R = 0, G = 0, B = 0, A = 255;
	constexpr FColor() = default;
	constexpr FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : R(InR), G(InG), B(InB), A(InA) {}
	static const FColor White;
	static const FColor Black;
	static const FColor Red;
	static const FColor Green;
	static const FColor Blue;
	static const FColor Yellow;
	static const FColor Cyan;
	static const FColor Magenta;
	static const FColor Orange;
	static const FColor Silver;
	static const FColor Emerald;
	static const FColor Turquoise;
	static const FColor Purple;
};

struct FLinearColor
{
	float R = 0.0f;
	float G = 0.0f;
	float B = 0.0f;
	float A = 1.0f;
	constexpr FLinearColor() = default;
	constexpr FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
	/// The engine converts FColor to FLinearColor implicitly-ish; the project's
	/// code writes FLinearColor(SomeFColor), so that spelling has to work.
	explicit FLinearColor(const FColor& From);
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

/// Unreal's verbosity is an unscoped enum inside a NAMESPACE, so the code that
/// uses it writes ELogVerbosity::Type and ELogVerbosity::VeryVerbose. A scoped
/// enum class would be tidier and would reject exactly that spelling, which is
/// how the first version of this file got it wrong.
namespace ELogVerbosity
{
	enum Type : uint8
	{
		NoLogging = 0,
		Fatal,
		Error,
		Warning,
		Display,
		Log,
		Verbose,
		VeryVerbose,
		All = VeryVerbose,
	};
}

struct FShimLogCategory
{
	ELogVerbosity::Type GetVerbosity() const { return ELogVerbosity::Log; }
	bool IsSuppressed(ELogVerbosity::Type Level) const { return false; }
	void SetVerbosity(ELogVerbosity::Type Level) {}
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
} // namespace VaelenShim

/// The engine's global log device. The project's log sink flushes it so a
/// crash does not lose the last lines, which is a real thing to keep working.
class FOutputDevice
{
public:
	virtual ~FOutputDevice() = default;
	virtual void Flush() {}
	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Level, const class FName& Category) {}
};

extern FOutputDevice* GLog;
#define UE_LOG(Category, Verbosity, Format, ...) VaelenShim::Swallow(&Category, Format, ##__VA_ARGS__)

/// The ensure family returns a bool and evaluates everything it is handed,
/// which is what lets `if (!ensure(X))` typecheck.
#define ensure(Expression) (VaelenShim::Swallow(Expression), static_cast<bool>(Expression))
#define ensureMsgf(Expression, Format, ...) (VaelenShim::Swallow(Format, ##__VA_ARGS__), static_cast<bool>(Expression))
#define ensureAlways(Expression) (static_cast<bool>(Expression))
#define ensureAlwaysMsgf(Expression, Format, ...)                                                                      \
	(VaelenShim::Swallow(Format, ##__VA_ARGS__), static_cast<bool>(Expression))
#define checkf(Expression, Format, ...) VaelenShim::Swallow(Format, ##__VA_ARGS__)
#define check(Expression) VaelenShim::Swallow(Expression)
