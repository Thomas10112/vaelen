// VAELEN - VaelenSociety
// Export macro of the VaelenSociety module (same scheme as VAELEN_POPULATION_API).
//
// STATUS: VALIDATED (Phase 05)
#pragma once

#if defined(VAELEN_SOCIETY_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_SOCIETY_API __declspec(dllexport)
#	else
#		define VAELEN_SOCIETY_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_SOCIETY_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_SOCIETY_API __declspec(dllimport)
#	else
#		define VAELEN_SOCIETY_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_SOCIETY_API
#endif
