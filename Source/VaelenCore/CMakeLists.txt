# VaelenCore — engine-agnostic kernel.
# Every file listed here is ALSO compiled by UBT (Source/VaelenCore/*.Build.cs).
# VaelenCoreModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_CORE_PUBLIC_HEADERS
  Public/Vaelen/Core/CoreTypes.h
  Public/Vaelen/Core/Version.h
  Public/Vaelen/Core/Assert.h
  Public/Vaelen/Core/Log.h
  Public/Vaelen/Core/Hash.h
  Public/Vaelen/Core/Random.h
  Public/Vaelen/Core/Ids.h
)

set(VAELEN_CORE_SOURCES
  Private/Assert.cpp
  Private/Log.cpp
  Private/Random.cpp
  Private/Ids.cpp
  Private/Version.cpp
)

add_library(VaelenCore STATIC ${VAELEN_CORE_SOURCES} ${VAELEN_CORE_PUBLIC_HEADERS})
add_library(Vaelen::Core ALIAS VaelenCore)

target_include_directories(VaelenCore
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenCore PUBLIC vaelen_build_flags)
target_compile_features(VaelenCore PUBLIC cxx_std_20)
