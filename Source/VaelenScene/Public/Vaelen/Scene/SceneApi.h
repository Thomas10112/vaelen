// VAELEN - VaelenScene
// Phase 19 task 19.05: the export macro, as ViewApi.h.
//
// STATUS: VALIDATED headless (Phase 19 task 19.05)
#pragma once

#if defined(VAELEN_SCENE_EXPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_SCENE_API __declspec(dllexport)
#	else
#		define VAELEN_SCENE_API __attribute__((visibility("default")))
#	endif
#elif defined(VAELEN_SCENE_IMPORTS)
#	if defined(_MSC_VER)
#		define VAELEN_SCENE_API __declspec(dllimport)
#	else
#		define VAELEN_SCENE_API __attribute__((visibility("default")))
#	endif
#else
#	define VAELEN_SCENE_API
#endif
