// VAELEN - VaelenEconomy
// Export macro of the VaelenEconomy module (same scheme as VAELEN_SOCIETY_API).
//
// STATUS: VALIDATED (Phase 06)
#pragma once

#if defined(VAELEN_ECONOMY_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_ECONOMY_API __declspec(dllexport)
#	else
#		define VAELEN_ECONOMY_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_ECONOMY_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_ECONOMY_API __declspec(dllimport)
#	else
#		define VAELEN_ECONOMY_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_ECONOMY_API
#endif
