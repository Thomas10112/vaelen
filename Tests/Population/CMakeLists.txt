# VaelenPopulation tests: Tests/Population/Test_<Suite>.cpp -> CTest entry Population.<Suite>.
file(GLOB VAELEN_POPULATION_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenPopulationTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_POPULATION_TEST_SOURCES})

target_include_directories(VaelenPopulationTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenPopulationTests PRIVATE Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_POPULATION_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_POPULATION_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Population.${SuiteName} COMMAND VaelenPopulationTests --suite ${SuiteName})
  list(APPEND VAELEN_POPULATION_TEST_NAMES Population.${SuiteName})
endforeach()

add_test(NAME Population.Registry COMMAND VaelenPopulationTests --check-registry)
add_test(NAME Population.Shuffled COMMAND VaelenPopulationTests --shuffle 1)
list(APPEND VAELEN_POPULATION_TEST_NAMES Population.Registry Population.Shuffled)

set_tests_properties(${VAELEN_POPULATION_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Population.PopulationGate PROPERTIES TIMEOUT 1800)
set_tests_properties(Population.Shuffled PROPERTIES TIMEOUT 5400)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Population.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Population.Shuffled PROPERTIES COST 10000)
set_tests_properties(Population.PopulationGate PROPERTIES COST 5000)
