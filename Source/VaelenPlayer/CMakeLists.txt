# VaelenPlayer — engine-agnostic player module (Phase 10).
# Every file listed here is ALSO compiled by UBT (Source/VaelenPlayer/*.Build.cs).
# VaelenPlayerModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_PLAYER_PUBLIC_HEADERS
  Public/Vaelen/Player/PlayerApi.h
  Public/Vaelen/Player/Player.h
  Public/Vaelen/Player/Start.h
  Public/Vaelen/Player/Hours.h
  Public/Vaelen/Player/Commands.h
)

set(VAELEN_PLAYER_SOURCES
  Private/Player.cpp
  Private/Start.cpp
  Private/Hours.cpp
  Private/Commands.cpp
)

add_library(VaelenPlayer STATIC ${VAELEN_PLAYER_SOURCES} ${VAELEN_PLAYER_PUBLIC_HEADERS})
add_library(Vaelen::Player ALIAS VaelenPlayer)

target_include_directories(VaelenPlayer
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenPlayer PUBLIC Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenPlayer PUBLIC cxx_std_20)
