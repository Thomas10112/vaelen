# VaelenPlayer tests: Tests/Player/Test_<Suite>.cpp -> CTest entry Player.<Suite>.
file(GLOB VAELEN_PLAYER_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenPlayerTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_PLAYER_TEST_SOURCES})

target_include_directories(VaelenPlayerTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenPlayerTests PRIVATE Vaelen::Player Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_PLAYER_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_PLAYER_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Player.${SuiteName} COMMAND VaelenPlayerTests --suite ${SuiteName})
  list(APPEND VAELEN_PLAYER_TEST_NAMES Player.${SuiteName})
endforeach()

add_test(NAME Player.Registry COMMAND VaelenPlayerTests --check-registry)
add_test(NAME Player.Shuffled COMMAND VaelenPlayerTests --shuffle 1)
list(APPEND VAELEN_PLAYER_TEST_NAMES Player.Registry Player.Shuffled)

set_tests_properties(${VAELEN_PLAYER_TEST_NAMES} PROPERTIES TIMEOUT 300)
set_tests_properties(Player.Shuffled PROPERTIES TIMEOUT 5400)
