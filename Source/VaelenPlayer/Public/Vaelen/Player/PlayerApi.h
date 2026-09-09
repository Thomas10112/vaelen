// VAELEN - VaelenPlayer
// Export macro of the VaelenPlayer module (same scheme as VAELEN_INFRASTRUCTURE_API).
//
// STATUS: PROTOTYPE (Phase 10)
#pragma once

#if defined(VAELEN_PLAYER_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_PLAYER_API __declspec(dllexport)
#	else
#		define VAELEN_PLAYER_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_PLAYER_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_PLAYER_API __declspec(dllimport)
#	else
#		define VAELEN_PLAYER_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_PLAYER_API
#endif
