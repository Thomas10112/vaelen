# VaelenInfrastructure tests: Tests/Infrastructure/Test_<Suite>.cpp -> CTest entry Infrastructure.<Suite>.
file(GLOB VAELEN_INFRASTRUCTURE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenInfrastructureTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_INFRASTRUCTURE_TEST_SOURCES})

target_include_directories(VaelenInfrastructureTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenInfrastructureTests PRIVATE Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_INFRASTRUCTURE_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_INFRASTRUCTURE_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Infrastructure.${SuiteName} COMMAND VaelenInfrastructureTests --suite ${SuiteName})
  list(APPEND VAELEN_INFRASTRUCTURE_TEST_NAMES Infrastructure.${SuiteName})
endforeach()

add_test(NAME Infrastructure.Registry COMMAND VaelenInfrastructureTests --check-registry)
add_test(NAME Infrastructure.Shuffled COMMAND VaelenInfrastructureTests --shuffle 1)
list(APPEND VAELEN_INFRASTRUCTURE_TEST_NAMES Infrastructure.Registry Infrastructure.Shuffled)

set_tests_properties(${VAELEN_INFRASTRUCTURE_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Infrastructure.Shuffled PROPERTIES TIMEOUT 5400)
