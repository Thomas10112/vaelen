# VaelenScene - the ground, the fence, the layout and the sky, in integers (Phase 19, ADR-0156).
# Every file listed here is ALSO compiled by UBT (Source/VaelenScene/VaelenScene.Build.cs).
# VaelenSceneModule.cpp is the only Unreal-facing file and is intentionally absent.
# It reads view LEAVES only (Tools/check_ui_fence.py holds it to Core and the leaves),
# so it can be built from a MapView that outlived its world.
set(VAELEN_SCENE_PUBLIC_HEADERS
  Public/Vaelen/Scene/SceneApi.h
  Public/Vaelen/Scene/Terrain.h
  Public/Vaelen/Scene/Fence.h
)

set(VAELEN_SCENE_SOURCES
  Private/Terrain.cpp
  Private/Fence.cpp
)

add_library(VaelenScene STATIC ${VAELEN_SCENE_SOURCES} ${VAELEN_SCENE_PUBLIC_HEADERS})
add_library(Vaelen::Scene ALIAS VaelenScene)

target_include_directories(VaelenScene
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenScene PUBLIC Vaelen::View Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenScene PUBLIC cxx_std_20)
