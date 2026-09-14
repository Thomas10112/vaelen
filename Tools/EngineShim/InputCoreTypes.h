// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

enum EInputEvent : uint8
{
	IE_Pressed = 0,
	IE_Released,
	IE_Repeat,
	IE_DoubleClick,
	IE_Axis,
	IE_MAX
};

struct FKey
{
	FKey() = default;
	explicit FKey(const TCHAR* InName);

	FName Name;
};

/// The keys the first screen binds (14.09), and the one that turns the day.
/// Named, not generated: a key this file does not carry is a key the UI
/// cannot bind, which is the whole point of a parse-only stand-in.
namespace EKeys
{
	extern const FKey W;
	extern const FKey R;
	extern const FKey E;
	extern const FKey T;
	extern const FKey S;
	extern const FKey G;
	extern const FKey K;
	extern const FKey M;
	extern const FKey SpaceBar;
} // namespace EKeys
