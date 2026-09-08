// VAELEN - VaelenMilitary
// Export macro of the VaelenMilitary module (same scheme as VAELEN_POLITICS_API).
//
// STATUS: VALIDATED (Phase 08)
#pragma once

#if defined(VAELEN_MILITARY_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_MILITARY_API __declspec(dllexport)
#	else
#		define VAELEN_MILITARY_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_MILITARY_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_MILITARY_API __declspec(dllimport)
#	else
#		define VAELEN_MILITARY_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_MILITARY_API
#endif
