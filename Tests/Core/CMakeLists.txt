# VaelenCore tests. Every Tests/Core/Test_<Suite>.cpp registers tests in suite
# "<Suite>" (see VAELEN_TEST) and becomes CTest entry "Core.<Suite>".
file(GLOB VAELEN_CORE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Test_*.cpp)

add_executable(VaelenCoreTests
  ${CMAKE_SOURCE_DIR}/Tests/Harness/TestMain.cpp
  ${VAELEN_CORE_TEST_SOURCES})

target_include_directories(VaelenCoreTests PRIVATE ${CMAKE_SOURCE_DIR}/Tests/Harness)
target_link_libraries(VaelenCoreTests PRIVATE Vaelen::Core)

foreach(TestSource IN LISTS VAELEN_CORE_TEST_SOURCES)
  get_filename_component(TestFile ${TestSource} NAME_WE)
  string(REGEX REPLACE "^Test_" "" SuiteName ${TestFile})
  add_test(NAME Core.${SuiteName} COMMAND VaelenCoreTests --suite ${SuiteName})
endforeach()
