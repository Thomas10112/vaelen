# VaelenView — engine-agnostic view module (Phase 13).
# Every file listed here is ALSO compiled by UBT (Source/VaelenView/*.Build.cs).
# VaelenViewModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_VIEW_PUBLIC_HEADERS
  Public/Vaelen/View/ViewApi.h
  Public/Vaelen/View/Frame.h
  Public/Vaelen/View/Delta.h
  Public/Vaelen/View/Eye.h
)

set(VAELEN_VIEW_SOURCES
  Private/Frame.cpp
  Private/Delta.cpp
  Private/Eye.cpp
)

add_library(VaelenView STATIC ${VAELEN_VIEW_SOURCES} ${VAELEN_VIEW_PUBLIC_HEADERS})
add_library(Vaelen::View ALIAS VaelenView)

target_include_directories(VaelenView
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenView PUBLIC Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenView PUBLIC cxx_std_20)
