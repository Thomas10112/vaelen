# VaelenPolitics tests: Tests/Politics/Test_<Suite>.cpp -> CTest entry Politics.<Suite>.
file(GLOB VAELEN_POLITICS_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenPoliticsTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_POLITICS_TEST_SOURCES})

target_include_directories(VaelenPoliticsTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenPoliticsTests PRIVATE Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_POLITICS_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_POLITICS_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Politics.${SuiteName} COMMAND VaelenPoliticsTests --suite ${SuiteName})
  list(APPEND VAELEN_POLITICS_TEST_NAMES Politics.${SuiteName})
endforeach()

add_test(NAME Politics.Registry COMMAND VaelenPoliticsTests --check-registry)
add_test(NAME Politics.Shuffled COMMAND VaelenPoliticsTests --shuffle 1)
list(APPEND VAELEN_POLITICS_TEST_NAMES Politics.Registry Politics.Shuffled)

set_tests_properties(${VAELEN_POLITICS_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Politics.Shuffled PROPERTIES TIMEOUT 5400)
# The Phase 07 gate runs 500 years of a whole world with two powers, twice.
set_tests_properties(Politics.PoliticsGate PROPERTIES TIMEOUT 2400)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Politics.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Politics.Shuffled PROPERTIES COST 10000)
set_tests_properties(Politics.PoliticsGate PROPERTIES COST 5000)
