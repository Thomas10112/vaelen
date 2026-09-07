# VaelenEconomy — engine-agnostic economy module (Phase 06).
# Every file listed here is ALSO compiled by UBT (Source/VaelenEconomy/*.Build.cs).
# VaelenEconomyModule.cpp is the only Unreal-facing file and is intentionally absent.
set(VAELEN_ECONOMY_PUBLIC_HEADERS
  Public/Vaelen/Economy/EconomyApi.h
  Public/Vaelen/Economy/Stocks.h
  Public/Vaelen/Economy/Production.h
  Public/Vaelen/Economy/Markets.h
  Public/Vaelen/Economy/Trade.h
  Public/Vaelen/Economy/Wealth.h
)

set(VAELEN_ECONOMY_SOURCES
  Private/Stocks.cpp
  Private/Production.cpp
  Private/Markets.cpp
  Private/Trade.cpp
  Private/Wealth.cpp
)

add_library(VaelenEconomy STATIC ${VAELEN_ECONOMY_SOURCES} ${VAELEN_ECONOMY_PUBLIC_HEADERS})
add_library(Vaelen::Economy ALIAS VaelenEconomy)

target_include_directories(VaelenEconomy
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private)

target_link_libraries(VaelenEconomy PUBLIC Vaelen::Society Vaelen::Population Vaelen::Sim Vaelen::Core vaelen_build_flags)
target_compile_features(VaelenEconomy PUBLIC cxx_std_20)
