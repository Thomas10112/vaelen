# VAELEN tests (headless). One executable per kernel module; one CTest entry
# per suite (= per Test_<Suite>.cpp file) so failures are attributed precisely.
add_subdirectory(Core)

# Kernel purity: no engine headers, exceptions, RTTI or non-deterministic
# randomness inside engine-agnostic modules.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME Kernel.Purity
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Tools/check_kernel_purity.py --root ${CMAKE_SOURCE_DIR})
else()
  message(WARNING "Python3 not found: Kernel.Purity test is not registered")
endif()
