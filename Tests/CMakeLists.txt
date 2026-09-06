# VAELEN tests (headless). One executable per kernel module; one CTest entry
# per suite (= per Test_<Suite>.cpp file) so failures are attributed precisely.
add_subdirectory(Core)
add_subdirectory(Sim)
add_subdirectory(Population)
add_subdirectory(Society)

# Kernel purity: no engine headers, exceptions, RTTI or non-deterministic
# randomness inside engine-agnostic modules. Python 3 is a hard requirement:
# a silently missing check would be a fake green.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME Kernel.Purity
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_kernel_purity.py --root ${PROJECT_SOURCE_DIR})
  # The checker's own regression suite: proves every rule still fires.
  add_test(NAME Kernel.PuritySelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_kernel_purity.py --self-test)
  set_tests_properties(Kernel.Purity Kernel.PuritySelfTest PROPERTIES TIMEOUT 120)
elseif(VAELEN_REQUIRE_PURITY_CHECK)
  message(FATAL_ERROR "Python 3 is required for the Kernel.Purity test. Install it or configure with -DVAELEN_REQUIRE_PURITY_CHECK=OFF (the purity check is then NOT run).")
else()
  message(WARNING "Python3 not found: Kernel.Purity test is NOT registered (VAELEN_REQUIRE_PURITY_CHECK=OFF)")
endif()
