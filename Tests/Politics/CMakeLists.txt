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
