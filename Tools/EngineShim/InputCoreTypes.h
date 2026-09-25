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
	/// 19.02 BELIEF: a key from its name, which is how a host-chosen table of
	/// letters (ADR-0158) becomes keys.
	explicit FKey(const FName InName);

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
	extern const FKey Tab; ///< cycles what a verb is aimed at
	extern const FKey F9;  ///< writes the stream
	// 19.02 BELIEFS: the walk's keys - ZQSD, the arrows, the mouse, and F for Speak.
	extern const FKey Z;
	extern const FKey Q;
	extern const FKey D;
	extern const FKey F;
	extern const FKey Left;
	extern const FKey Right;
	extern const FKey Up;
	extern const FKey Down;
	extern const FKey Mouse2D;
} // namespace EKeys
