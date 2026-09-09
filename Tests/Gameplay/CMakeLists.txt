# VaelenGameplay tests: Tests/Gameplay/Test_<Suite>.cpp -> CTest entry Gameplay.<Suite>.
file(GLOB VAELEN_GAMEPLAY_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenGameplayTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_GAMEPLAY_TEST_SOURCES})

target_include_directories(VaelenGameplayTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenGameplayTests PRIVATE Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core Threads::Threads)

set(VAELEN_GAMEPLAY_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_GAMEPLAY_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Gameplay.${SuiteName} COMMAND VaelenGameplayTests --suite ${SuiteName})
  list(APPEND VAELEN_GAMEPLAY_TEST_NAMES Gameplay.${SuiteName})
endforeach()

add_test(NAME Gameplay.Registry COMMAND VaelenGameplayTests --check-registry)
add_test(NAME Gameplay.Shuffled COMMAND VaelenGameplayTests --shuffle 1)
list(APPEND VAELEN_GAMEPLAY_TEST_NAMES Gameplay.Registry Gameplay.Shuffled)

set_tests_properties(${VAELEN_GAMEPLAY_TEST_NAMES} PROPERTIES TIMEOUT 1800)
# Fame builds five worlds of 128x128 with three centuries of pre-history each and
# then runs a dozen years of a lively region in every one of them: 331 s here, and
# the Windows and macOS legs are slower.
set_tests_properties(Gameplay.Fame PROPERTIES TIMEOUT 5400)
set_tests_properties(Gameplay.Shuffled PROPERTIES TIMEOUT 5400)
