// VAELEN - VaelenColony
// Export macro of the VaelenColony module (same scheme as VAELEN_PLAYER_API).
//
// STATUS: PROTOTYPE (Phase 11)
#pragma once

#if defined(VAELEN_COLONY_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_COLONY_API __declspec(dllexport)
#	else
#		define VAELEN_COLONY_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_COLONY_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_COLONY_API __declspec(dllimport)
#	else
#		define VAELEN_COLONY_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_COLONY_API
#endif
