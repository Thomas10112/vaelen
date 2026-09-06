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
set_tests_properties(Society.Shuffled PROPERTIES TIMEOUT 1200)
