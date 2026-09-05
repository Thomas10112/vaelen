// VAELEN - VaelenCore
// PersistentId kind names and the deterministic IdAllocator.
//
// STATUS: VALIDATED (Phase 00) - covered by Tests/Core/Test_Ids.cpp
#include "Vaelen/Core/Ids.h"
#include "Vaelen/Core/Assert.h"

namespace Vaelen
{
	const char* IdKindToString(IdKind Kind) noexcept
	{
		switch (Kind)
		{
		case IdKind::None:
			return "None";
		case IdKind::Entity:
			return "Entity";
		case IdKind::Event:
			return "Event";
		case IdKind::Region:
			return "Region";
		case IdKind::Tile:
			return "Tile";
		case IdKind::River:
			return "River";
		case IdKind::ResourceDeposit:
			return "ResourceDeposit";
		case IdKind::Culture:
			return "Culture";
		case IdKind::Language:
			return "Language";
		case IdKind::Religion:
			return "Religion";
		case IdKind::Person:
			return "Person";
		case IdKind::Family:
			return "Family";
		case IdKind::Organization:
			return "Organization";
		case IdKind::Item:
			return "Item";
		case IdKind::Building:
			return "Building";
		case IdKind::Settlement:
			return "Settlement";
		case IdKind::Market:
			return "Market";
		case IdKind::Route:
			return "Route";
		case IdKind::Polity:
			return "Polity";
		case IdKind::Law:
			return "Law";
		case IdKind::Army:
			return "Army";
		case IdKind::War:
			return "War";
		case IdKind::Document:
			return "Document";
		case IdKind::Map:
			return "Map";
		case IdKind::MaxValue:
			return "MaxValue";
		}
		return "Unknown";
	}

	IdAllocator::IdAllocator() noexcept
	{
		Reset();
	}

	void IdAllocator::Reset() noexcept
	{
		for (uint64& Next : Current.NextSerial)
		{
			Next = 1; // serial 0 is reserved for "invalid"
		}
	}

	void IdAllocator::SetState(const State& InState) noexcept
	{
		Current = InState;
		for (uint64& Next : Current.NextSerial)
		{
			if (Next == 0)
			{
				Next = 1;
			}
			else if (!VAELEN_ENSURE(Next <= PersistentId::MaxSerial + 1))
			{
				// Beyond the 56-bit serial space: only a corrupt save can
				// produce this. Clamp to the exhausted sentinel so that no
				// masked, already-used serial can ever be handed out.
				Next = PersistentId::MaxSerial + 1;
			}
		}
	}

	PersistentId IdAllocator::Allocate(IdKind Kind) noexcept
	{
		VAELEN_CHECKF(Kind != IdKind::None, "Cannot allocate an id of kind None");
		if (Kind == IdKind::None)
		{
			return PersistentId::Invalid();
		}
		uint64& Next = Current.NextSerial[ToUnderlying(Kind)];
		VAELEN_CHECKF(Next <= PersistentId::MaxSerial, "PersistentId serial space exhausted for kind %s",
					  IdKindToString(Kind));
		if (Next > PersistentId::MaxSerial)
		{
			// Never wrap around: a reused id would silently corrupt history.
			// Reached only when assertions are disabled or the handler returns.
			return PersistentId::Invalid();
		}
		const PersistentId Id = PersistentId::Make(Kind, Next);
		++Next;
		return Id;
	}

	uint64 IdAllocator::GetAllocatedCount(IdKind Kind) const noexcept
	{
		const uint64 Next = Current.NextSerial[ToUnderlying(Kind)];
		return Next > PersistentId::MaxSerial ? PersistentId::MaxSerial : Next - 1;
	}

	PersistentId IdAllocator::PeekNext(IdKind Kind) const noexcept
	{
		const uint64 Next = Current.NextSerial[ToUnderlying(Kind)];
		if (Kind == IdKind::None || Next > PersistentId::MaxSerial)
		{
			return PersistentId::Invalid();
		}
		return PersistentId::Make(Kind, Next);
	}

	bool IdAllocator::ReserveUpTo(IdKind Kind, uint64 Serial) noexcept
	{
		VAELEN_CHECK(Serial <= PersistentId::MaxSerial);
		if (Serial > PersistentId::MaxSerial)
		{
			// A serial outside the 56-bit space (corrupt import) must not poison
			// the counter. Reached only when assertions are disabled or the
			// handler returns.
			return false;
		}
		uint64& Next = Current.NextSerial[ToUnderlying(Kind)];
		if (Serial + 1 > Next)
		{
			Next = Serial + 1;
			return true;
		}
		return false;
	}
} // namespace Vaelen
