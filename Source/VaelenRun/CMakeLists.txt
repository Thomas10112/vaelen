# VaelenRun — engine-agnostic run module (Phase 14): one wiring of a played
# AELVOR, its door, and the replay of what came through it.
# Every file listed here is ALSO compiled by UBT (Source/VaelenRun/*.Build.cs).
# VaelenRunModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_RUN_PUBLIC_HEADERS
  Public/Vaelen/Run/RunApi.h
  Public/Vaelen/Run/Aelvor.h
  Public/Vaelen/Run/Door.h
  Public/Vaelen/Run/Checkpoint.h
  Public/Vaelen/Run/Store.h
)

set(VAELEN_RUN_SOURCES
  Private/Aelvor.cpp
  Private/Door.cpp
  Private/Checkpoint.cpp
  Private/Store.cpp
)

add_library(VaelenRun STATIC ${VAELEN_RUN_SOURCES} ${VAELEN_RUN_PUBLIC_HEADERS})
add_library(Vaelen::Run ALIAS VaelenRun)

target_include_directories(VaelenRun
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenRun PUBLIC Vaelen::View Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenRun PUBLIC cxx_std_20)
