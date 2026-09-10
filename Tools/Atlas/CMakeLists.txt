# VaelenAtlas — AELVOR generated and written out, with no engine anywhere.
# Not a kernel module: it reads the clock and writes a file, which is why it
# lives under Tools/ and not under Source/ (Tools/check_kernel_purity.py).
add_executable(VaelenAtlas Main.cpp)

target_link_libraries(VaelenAtlas PRIVATE
  Vaelen::View Vaelen::Gameplay Vaelen::Player Vaelen::Colony Vaelen::Infrastructure
  Vaelen::Military Vaelen::Politics Vaelen::Economy Vaelen::Society Vaelen::Population
  Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenAtlas PRIVATE cxx_std_20)
