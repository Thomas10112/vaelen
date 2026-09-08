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

