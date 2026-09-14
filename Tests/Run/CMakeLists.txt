# VaelenRun tests: Tests/Run/Test_<Suite>.cpp -> CTest entry Run.<Suite>.
file(GLOB VAELEN_RUN_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenRunTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_RUN_TEST_SOURCES})

target_include_directories(VaelenRunTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenRunTests PRIVATE Vaelen::Run Vaelen::View Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_RUN_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_RUN_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Run.${SuiteName} COMMAND VaelenRunTests --suite ${SuiteName})
  list(APPEND VAELEN_RUN_TEST_NAMES Run.${SuiteName})
endforeach()

add_test(NAME Run.Registry COMMAND VaelenRunTests --check-registry)
add_test(NAME Run.Shuffled COMMAND VaelenRunTests --shuffle 1)
list(APPEND VAELEN_RUN_TEST_NAMES Run.Registry Run.Shuffled)

set_tests_properties(${VAELEN_RUN_TEST_NAMES} PROPERTIES TIMEOUT 1800)
set_tests_properties(Run.Shuffled PROPERTIES TIMEOUT 5400)
# Run.Aelvor holds the 128/120 world of the frozen pair and a day measured at
# 256 with the colony; it is the long pole of this module after Shuffled.
set_tests_properties(Run.Aelvor PROPERTIES TIMEOUT 3600)

# ── Scheduling ───────────────────────────────────────────────────────────────
# See Tests/View/CMakeLists.txt: ctest orders by descending COST, and the long
# poles are declared so a leg's wall clock is not set by whichever one starts
# last. The numbers are relative, not seconds.
set_tests_properties(Run.Shuffled PROPERTIES COST 10000)
set_tests_properties(Run.Aelvor PROPERTIES COST 6000)
set_tests_properties(Run.Door PROPERTIES COST 3000)
