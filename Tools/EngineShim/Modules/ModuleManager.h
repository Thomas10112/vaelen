// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
#pragma once

#include "CoreMinimal.h"

class IModuleInterface
{
public:
	virtual ~IModuleInterface() = default;
	virtual void StartupModule() {}
	virtual void ShutdownModule() {}
	virtual bool IsGameModule() const { return false; }
};

/// Written without a trailing semicolon, because the real macro is used
/// without one. The instance is what makes the type complete-checked: an
/// IMPLEMENT_MODULE naming a class that does not exist still fails here.
#define IMPLEMENT_MODULE(ClassName, ModuleName) [[maybe_unused]] static ClassName ModuleName##_ShimInstance;

/// The game module's variant. Same shape, one more argument, and used without a
/// trailing semicolon like its sibling.
#define IMPLEMENT_PRIMARY_GAME_MODULE(ClassName, ModuleName, GameName)                                                 \
	[[maybe_unused]] static ClassName ModuleName##_ShimGameInstance;
