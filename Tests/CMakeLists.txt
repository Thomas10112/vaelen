# VAELEN tests (headless). One executable per kernel module; one CTest entry
# per suite (= per Test_<Suite>.cpp file) so failures are attributed precisely.
add_subdirectory(Core)
add_subdirectory(Sim)
add_subdirectory(Population)
add_subdirectory(Society)
add_subdirectory(Economy)
add_subdirectory(Politics)
add_subdirectory(Military)
add_subdirectory(Infrastructure)
add_subdirectory(Colony)
add_subdirectory(Player)
add_subdirectory(Gameplay)
add_subdirectory(View)

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

# ── The atlas tool ───────────────────────────────────────────────────────────
# Compiling proves nothing about a tool. These run it: once to see it produce a
# world, twice to see it produce the SAME world, and the checker reads the file
# back rather than trusting an exit code (Tools/check_atlas_output.py).
set(VAELEN_ATLAS_A ${CMAKE_CURRENT_BINARY_DIR}/atlas-run-a.json)
set(VAELEN_ATLAS_B ${CMAKE_CURRENT_BINARY_DIR}/atlas-run-b.json)
# --every exercises the timeline path, and --colony the mining one, so CI runs
# the tool the way it is actually used rather than only its simplest shape.
add_test(NAME Atlas.Runs
  COMMAND VaelenAtlas --size 48 --prehistory 30 --years 10 --every 5 --colony --chronicle --out ${VAELEN_ATLAS_A})
add_test(NAME Atlas.RunsAgain
  COMMAND VaelenAtlas --size 48 --prehistory 30 --years 10 --every 5 --colony --chronicle --out ${VAELEN_ATLAS_B})
set_tests_properties(Atlas.Runs PROPERTIES FIXTURES_SETUP AtlasA TIMEOUT 900)
set_tests_properties(Atlas.RunsAgain PROPERTIES FIXTURES_SETUP AtlasB TIMEOUT 900)

if(Python3_Interpreter_FOUND)
  add_test(NAME Atlas.Output
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py ${VAELEN_ATLAS_A})
  set_tests_properties(Atlas.Output PROPERTIES FIXTURES_REQUIRED AtlasA TIMEOUT 300)
  add_test(NAME Atlas.Deterministic
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py
            --same ${VAELEN_ATLAS_A} ${VAELEN_ATLAS_B})
  set_tests_properties(Atlas.Deterministic PROPERTIES FIXTURES_REQUIRED "AtlasA;AtlasB" TIMEOUT 300)
  # The checker's own regression suite: proves every rule still fires.
  add_test(NAME Atlas.OutputSelfTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Tools/check_atlas_output.py --self-test)
  set_tests_properties(Atlas.OutputSelfTest PROPERTIES TIMEOUT 120)
elseif(VAELEN_REQUIRE_PURITY_CHECK)
  message(FATAL_ERROR "Python 3 is required to check what VaelenAtlas writes.")
else()
  message(WARNING "Python3 not found: the Atlas.Output tests are NOT registered")
endif()
