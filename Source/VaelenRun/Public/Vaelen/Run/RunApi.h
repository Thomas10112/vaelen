// VAELEN - VaelenRun
// Export macro of the VaelenRun module (same scheme as VAELEN_VIEW_API).
//
// STATUS: PROTOTYPE (Phase 14)
#pragma once

#if defined(VAELEN_RUN_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_RUN_API __declspec(dllexport)
#	else
#		define VAELEN_RUN_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_RUN_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_RUN_API __declspec(dllimport)
#	else
#		define VAELEN_RUN_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_RUN_API
#endif
