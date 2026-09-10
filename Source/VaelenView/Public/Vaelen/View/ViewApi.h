// VAELEN - VaelenView
// Export macro of the VaelenView module (same scheme as VAELEN_GAMEPLAY_API).
//
// STATUS: PROTOTYPE (Phase 13)
#pragma once

#if defined(VAELEN_VIEW_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_VIEW_API __declspec(dllexport)
#	else
#		define VAELEN_VIEW_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_VIEW_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_VIEW_API __declspec(dllimport)
#	else
#		define VAELEN_VIEW_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_VIEW_API
#endif
