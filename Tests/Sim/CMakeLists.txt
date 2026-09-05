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
