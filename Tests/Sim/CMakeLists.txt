# VaelenSim tests: Tests/Sim/Test_<Suite>.cpp -> CTest entry Sim.<Suite>.
file(GLOB VAELEN_SIM_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenSimTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_SIM_TEST_SOURCES})

target_include_directories(VaelenSimTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenSimTests PRIVATE Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_SIM_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_SIM_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Sim.${SuiteName} COMMAND VaelenSimTests --suite ${SuiteName})
  list(APPEND VAELEN_SIM_TEST_NAMES Sim.${SuiteName})
endforeach()

add_test(NAME Sim.Registry COMMAND VaelenSimTests --check-registry)
add_test(NAME Sim.Shuffled COMMAND VaelenSimTests --shuffle 1)
list(APPEND VAELEN_SIM_TEST_NAMES Sim.Registry Sim.Shuffled)

set_tests_properties(${VAELEN_SIM_TEST_NAMES} PROPERTIES TIMEOUT 300)
# The shuffled run repeats every Sim test, baselines included: MSVC debug needs
# more than 300 s for it (CI run 29 timed out at 300 s).
set_tests_properties(Sim.Shuffled PROPERTIES TIMEOUT 1200)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Sim.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Sim.Shuffled PROPERTIES COST 10000)
set_tests_properties(Sim.HistoryGate PROPERTIES COST 5000)
