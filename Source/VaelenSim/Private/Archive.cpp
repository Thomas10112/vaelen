// VAELEN - VaelenSim
// Memory-backed archives.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_Archive.cpp
#include "Vaelen/Sim/Archive.h"

namespace Vaelen
{
	bool MemoryWriter::SerializeBytes(void* Data, usize Size) noexcept
	{
		if (Size == 0)
		{
			return true;
		}
		const usize Start = Out->size();
		Out->resize(Start + Size);
		std::memcpy(Out->data() + Start, Data, Size);
		return true;
	}

	bool MemoryReader::SerializeBytes(void* Data, usize Count) noexcept
	{
		if (Error || Count > Size - Offset)
		{
			Error = true;
			if (Count > 0)
			{
				std::memset(Data, 0, Count);
			}
			return false;
		}
		if (Count > 0)
		{
			std::memcpy(Data, Bytes + Offset, Count);
			Offset += Count;
		}
		return true;
	}
} // namespace Vaelen
