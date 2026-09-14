// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class UFont : public UObject
{
public:
	using Super = UFont;
};

class UCanvas : public UObject
{
public:
	using Super = UCanvas;

	float SizeX = 0.0f;
	float SizeY = 0.0f;

	/// The engine's takes two more scale arguments with defaults. Kept to what
	/// this project draws with: a font, a string and where to put it.
	void DrawText(UFont* Font, const FString& Text, float X, float Y);
};
