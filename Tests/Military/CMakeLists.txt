# VaelenMilitary tests: Tests/Military/Test_<Suite>.cpp -> CTest entry Military.<Suite>.
file(GLOB VAELEN_MILITARY_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenMilitaryTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_MILITARY_TEST_SOURCES})

target_include_directories(VaelenMilitaryTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenMilitaryTests PRIVATE Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_MILITARY_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_MILITARY_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Military.${SuiteName} COMMAND VaelenMilitaryTests --suite ${SuiteName})
  list(APPEND VAELEN_MILITARY_TEST_NAMES Military.${SuiteName})
endforeach()

add_test(NAME Military.Registry COMMAND VaelenMilitaryTests --check-registry)
add_test(NAME Military.Shuffled COMMAND VaelenMilitaryTests --shuffle 1)
list(APPEND VAELEN_MILITARY_TEST_NAMES Military.Registry Military.Shuffled)

set_tests_properties(${VAELEN_MILITARY_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Military.Shuffled PROPERTIES TIMEOUT 5400)
# Five centuries at 256 with every Phase 04 to 08 system running.
set_tests_properties(Military.MilitaryGate PROPERTIES TIMEOUT 3600)


# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Military.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Military.Shuffled PROPERTIES COST 10000)
set_tests_properties(Military.MilitaryGate PROPERTIES COST 5000)
