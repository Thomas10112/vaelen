// VAELEN - VaelenSociety
// Phase 05.04: the state of a bound person - shared by the bondage system
// that writes it and the standing system that reads it.
//
// STATUS: VALIDATED (Phase 05) - covered by Tests/Society/Test_Bondage.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"

namespace Vaelen::Society
{
	enum class BondKind : uint8
	{
		Free = 0,
		Bonded = 1,
		Enslaved = 2,
	};
	enum class BondEntry : uint8
	{
		None = 0,
		Debt = 1,
		Birth = 2,
		Capture = 3, ///< Phase 08
	};
	enum class BondExit : uint8
	{
		None = 0,
		Manumission = 1,
		Flight = 2,
		HolderDied = 3,
		Death = 4,
	};
	/// Component on a person while it is not free.
	struct BondState
	{
		uint8 Kind = 0;	 ///< BondKind
		uint8 Entry = 0; ///< BondEntry
		uint8 Reserved[2] = {};
		uint32 Holder = 0; ///< person index of the holder (0 = the region itself)
		uint64 Since = 0;  ///< tick of the entry
	};
	static_assert(sizeof(BondState) == 16, "BondState must stay padding free");

} // namespace Vaelen::Society
