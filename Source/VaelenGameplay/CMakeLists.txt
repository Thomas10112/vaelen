# VaelenGameplay — engine-agnostic gameplay module (Phase 12).
# Every file listed here is ALSO compiled by UBT (Source/VaelenGameplay/*.Build.cs).
# VaelenGameplayModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_GAMEPLAY_PUBLIC_HEADERS
  Public/Vaelen/Gameplay/GameplayApi.h
  Public/Vaelen/Gameplay/Living.h
  Public/Vaelen/Gameplay/Repute.h
  Public/Vaelen/Gameplay/Documents.h
  Public/Vaelen/Gameplay/Maps.h
  Public/Vaelen/Gameplay/Fame.h
  Public/Vaelen/Gameplay/Judgement.h
  Public/Vaelen/Gameplay/GameplayHistory.h
)

set(VAELEN_GAMEPLAY_SOURCES
  Private/Living.cpp
  Private/Repute.cpp
  Private/Documents.cpp
  Private/Maps.cpp
  Private/Fame.cpp
  Private/Judgement.cpp
  Private/GameplayHistory.cpp
)

add_library(VaelenGameplay STATIC ${VAELEN_GAMEPLAY_SOURCES} ${VAELEN_GAMEPLAY_PUBLIC_HEADERS})
add_library(Vaelen::Gameplay ALIAS VaelenGameplay)

target_include_directories(VaelenGameplay
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenGameplay PUBLIC Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenGameplay PUBLIC cxx_std_20)
