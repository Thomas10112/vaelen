# VaelenView tests: Tests/View/Test_<Suite>.cpp -> CTest entry View.<Suite>.
file(GLOB VAELEN_VIEW_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenViewTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_VIEW_TEST_SOURCES})

target_include_directories(VaelenViewTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenViewTests PRIVATE Vaelen::View Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_VIEW_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_VIEW_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME View.${SuiteName} COMMAND VaelenViewTests --suite ${SuiteName})
  list(APPEND VAELEN_VIEW_TEST_NAMES View.${SuiteName})
endforeach()

add_test(NAME View.Registry COMMAND VaelenViewTests --check-registry)
add_test(NAME View.Shuffled COMMAND VaelenViewTests --shuffle 1)
list(APPEND VAELEN_VIEW_TEST_NAMES View.Registry View.Shuffled)

set_tests_properties(${VAELEN_VIEW_TEST_NAMES} PROPERTIES TIMEOUT 1800)
set_tests_properties(View.Shuffled PROPERTIES TIMEOUT 5400)

# ── Scheduling ───────────────────────────────────────────────────────────────
# The wall clock of a CI leg is set by when its LONGEST test starts, not by how
# much work there is in total: ctest runs four at a time, and a forty-minute
# test picked up last adds forty minutes to the job whatever else has finished.
# ctest orders by descending COST when it has one, and has none on a fresh
# checkout, so the long poles are declared here. The numbers are relative, not
# seconds. View.Shuffled re-runs every suite of this module in one process
# and is by construction the longest thing in it.
set_tests_properties(View.Shuffled PROPERTIES COST 10000)
set_tests_properties(View.ViewGate PROPERTIES COST 5000)
