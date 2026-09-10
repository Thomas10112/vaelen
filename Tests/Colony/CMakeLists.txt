# VaelenColony tests: Tests/Colony/Test_<Suite>.cpp -> CTest entry Colony.<Suite>.
file(GLOB VAELEN_COLONY_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenColonyTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_COLONY_TEST_SOURCES})

target_include_directories(VaelenColonyTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenColonyTests PRIVATE Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_COLONY_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_COLONY_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Colony.${SuiteName} COMMAND VaelenColonyTests --suite ${SuiteName})
  list(APPEND VAELEN_COLONY_TEST_NAMES Colony.${SuiteName})
endforeach()

add_test(NAME Colony.Registry COMMAND VaelenColonyTests --check-registry)
add_test(NAME Colony.Shuffled COMMAND VaelenColonyTests --shuffle 1)
list(APPEND VAELEN_COLONY_TEST_NAMES Colony.Registry Colony.Shuffled)

set_tests_properties(${VAELEN_COLONY_TEST_NAMES} PROPERTIES TIMEOUT 600)
set_tests_properties(Colony.Shuffled PROPERTIES TIMEOUT 5400)
# The phase gate lives a century at 256 with a colony inside it, and replays it.
set_tests_properties(Colony.ColonyGate PROPERTIES TIMEOUT 5400)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Colony.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Colony.Shuffled PROPERTIES COST 10000)
set_tests_properties(Colony.ColonyGate PROPERTIES COST 5000)
