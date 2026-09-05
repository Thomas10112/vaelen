# ─────────────────────────────────────────────────────────────────────────────
# VAELEN — headless build of the engine-agnostic simulation kernel.
#
# This CMake project compiles the SAME sources that Unreal Build Tool compiles
# inside the UE5 project (Source/VaelenCore, later Source/VaelenSim, ...), but
# without the engine. It exists for:
#   - unit / determinism / long-duration tests (CTest),
#   - continuous integration without an Unreal installation,
#   - stress tests and profiling of the simulation alone.
#
# The Unreal project itself is built with UBT (Vaelen.uproject); nothing here is
# needed to open the project in the editor.
# ─────────────────────────────────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.24)

project(Vaelen
  VERSION 0.0.1
  DESCRIPTION "VAELEN simulation kernel (headless)"
  LANGUAGES CXX)

option(VAELEN_BUILD_TESTS "Build the kernel test executables" ON)
option(VAELEN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(VAELEN_ENABLE_ASSERTS "Enable VAELEN_CHECK assertions" ON)
option(VAELEN_REQUIRE_PURITY_CHECK "Fail configuration when Python 3 (needed by the Kernel.Purity test) is missing" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ── Shared compile options mirroring Unreal's defaults (no exceptions, no RTTI)
add_library(vaelen_build_flags INTERFACE)
if(MSVC)
  # Unreal builds with _HAS_EXCEPTIONS=0 and /GR-. We do the same.
  string(REGEX REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REGEX REPLACE "/GR " "/GR- " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
  # /EHs-c-: exceptions explicitly off (without an explicit /EH mode, MSVC
  # emits C4577 on every noexcept, fatal under /WX; /wd4577 covers old cl).
  # /fp:precise forbids FMA contraction on Microsoft cl (VS 2022 17.0+); on
  # clang-cl it does not, hence the explicit -ffp-contract=off below.
  target_compile_options(vaelen_build_flags INTERFACE /W4 /permissive- /utf-8 /GR- /EHs-c- /wd4577 /fp:precise)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(vaelen_build_flags INTERFACE /clang:-ffp-contract=off)
  endif()
  target_compile_definitions(vaelen_build_flags INTERFACE _HAS_EXCEPTIONS=0 _CRT_SECURE_NO_WARNINGS NOMINMAX)
  if(VAELEN_WARNINGS_AS_ERRORS)
    target_compile_options(vaelen_build_flags INTERFACE /WX)
  endif()
else()
  # -ffp-contract=off: no fused multiply-add contraction, so floating-point
  # results are identical between GCC and Clang (determinism, see ADR-0009).
  target_compile_options(vaelen_build_flags INTERFACE
    -fno-exceptions -fno-rtti -ffp-contract=off
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wold-style-cast -Wcast-align -Wnon-virtual-dtor -Woverloaded-virtual
    -Wnull-dereference -Wdouble-promotion -Wformat=2)
  if(VAELEN_WARNINGS_AS_ERRORS)
    target_compile_options(vaelen_build_flags INTERFACE -Werror)
  endif()
endif()

# Export macros are owned by the kernel (VAELEN_CORE_API in CoreTypes.h):
# nothing is defined here, so static libraries get an empty macro.
target_compile_definitions(vaelen_build_flags INTERFACE
  VAELEN_HEADLESS_BUILD=1
  $<$<BOOL:${VAELEN_ENABLE_ASSERTS}>:VAELEN_ASSERTS_ENABLED=1>
  $<$<NOT:$<BOOL:${VAELEN_ENABLE_ASSERTS}>>:VAELEN_ASSERTS_ENABLED=0>)

# ── Kernel modules ───────────────────────────────────────────────────────────
add_subdirectory(Source/VaelenCore)
add_subdirectory(Source/VaelenSim)

# ── Tests ────────────────────────────────────────────────────────────────────
if(VAELEN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(Tests)
endif()
