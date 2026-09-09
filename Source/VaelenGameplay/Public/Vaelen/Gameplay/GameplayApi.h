// VAELEN - VaelenGameplay
// Export macro of the VaelenGameplay module (same scheme as VAELEN_PLAYER_API).
//
// STATUS: PROTOTYPE (Phase 12)
#pragma once

#if defined(VAELEN_GAMEPLAY_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_GAMEPLAY_API __declspec(dllexport)
#	else
#		define VAELEN_GAMEPLAY_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_GAMEPLAY_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_GAMEPLAY_API __declspec(dllimport)
#	else
#		define VAELEN_GAMEPLAY_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_GAMEPLAY_API
#endif
