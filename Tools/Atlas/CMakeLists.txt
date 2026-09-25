# VaelenAtlas — AELVOR generated and written out, with no engine anywhere.
# Not a kernel module: it reads the clock and writes a file, which is why it
# lives under Tools/ and not under Source/ (Tools/check_kernel_purity.py).
add_executable(VaelenAtlas Main.cpp)

target_link_libraries(VaelenAtlas PRIVATE
  Vaelen::Run Vaelen::Scene Vaelen::View Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure
  Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population
  Vaelen::Sim Vaelen::Core vaelen_build_flags)
# 17.06: --inspect-dir lists a directory through the host-side store, which
# lives under Tools/Store for the reason its first line gives.
target_include_directories(VaelenAtlas PRIVATE ${PROJECT_SOURCE_DIR}/Tools/Store)
target_compile_features(VaelenAtlas PRIVATE cxx_std_20)
