// VAELEN - VaelenPolitics
// Export macro of the VaelenPolitics module (same scheme as VAELEN_ECONOMY_API).
//
// STATUS: VALIDATED (Phase 07)
#pragma once

#if defined(VAELEN_POLITICS_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_POLITICS_API __declspec(dllexport)
#	else
#		define VAELEN_POLITICS_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_POLITICS_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_POLITICS_API __declspec(dllimport)
#	else
#		define VAELEN_POLITICS_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_POLITICS_API
#endif
