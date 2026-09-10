# VaelenSociety tests: Tests/Society/Test_<Suite>.cpp -> CTest entry Society.<Suite>.
file(GLOB VAELEN_SOCIETY_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenSocietyTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_SOCIETY_TEST_SOURCES})

target_include_directories(VaelenSocietyTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenSocietyTests PRIVATE Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_SOCIETY_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_SOCIETY_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Society.${SuiteName} COMMAND VaelenSocietyTests --suite ${SuiteName})
  list(APPEND VAELEN_SOCIETY_TEST_NAMES Society.${SuiteName})
endforeach()

add_test(NAME Society.Registry COMMAND VaelenSocietyTests --check-registry)
add_test(NAME Society.Shuffled COMMAND VaelenSocietyTests --shuffle 1)
list(APPEND VAELEN_SOCIETY_TEST_NAMES Society.Registry Society.Shuffled)

set_tests_properties(${VAELEN_SOCIETY_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Society.SocietyGate PROPERTIES TIMEOUT 1800)
set_tests_properties(Society.Shuffled PROPERTIES TIMEOUT 5400)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. Society.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(Society.Shuffled PROPERTIES COST 10000)
set_tests_properties(Society.SocietyGate PROPERTIES COST 5000)
