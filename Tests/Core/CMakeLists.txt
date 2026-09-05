# VaelenCore tests. Every Tests/Core/Test_<Suite>.cpp registers tests in suite
# "<Suite>" (see VAELEN_TEST) and becomes CTest entry "Core.<Suite>". The runner
# itself refuses to run when a suite name does not match its file.
file(GLOB VAELEN_CORE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

find_package(Threads REQUIRED)

add_executable(VaelenCoreTests
  ${PROJECT_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_CORE_TEST_SOURCES})

target_include_directories(VaelenCoreTests PRIVATE ${PROJECT_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenCoreTests PRIVATE Vaelen::Core Threads::Threads)

set(VAELEN_CORE_TEST_NAMES "")
foreach(TestSource IN LISTS VAELEN_CORE_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Core.${SuiteName} COMMAND VaelenCoreTests --suite ${SuiteName})
  list(APPEND VAELEN_CORE_TEST_NAMES Core.${SuiteName})
endforeach()

# Every VAELEN_TEST lives in the file whose stem is its suite (else no entry
# above would ever select it), and the suites are order-independent.
add_test(NAME Core.Registry COMMAND VaelenCoreTests --check-registry)
add_test(NAME Core.Shuffled COMMAND VaelenCoreTests --shuffle 1)
add_test(NAME Core.Reversed COMMAND VaelenCoreTests --reverse)
list(APPEND VAELEN_CORE_TEST_NAMES Core.Registry Core.Shuffled Core.Reversed)

# A hang (deadlock) must fail, not stall CI for hours.
set_tests_properties(${VAELEN_CORE_TEST_NAMES} PROPERTIES TIMEOUT 300)
