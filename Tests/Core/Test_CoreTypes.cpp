// VAELEN - CoreTypes.h contracts.
//
// STATUS: VALIDATED (Phase 00)
#include "VaelenTest.h"

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Ids.h"

#include <bit>
#include <cstdint>
#include <type_traits>

namespace
{
	struct Owner : Vaelen::NonCopyable
	{
		int Value = 0;
	};
} // namespace

// Fixed widths and the Unreal-compatible 64-bit spelling.
static_assert(sizeof(Vaelen::int8) == 1 && sizeof(Vaelen::uint8) == 1);
static_assert(sizeof(Vaelen::int16) == 2 && sizeof(Vaelen::uint16) == 2);
static_assert(sizeof(Vaelen::int32) == 4 && sizeof(Vaelen::uint32) == 4);
static_assert(sizeof(Vaelen::int64) == 8 && sizeof(Vaelen::uint64) == 8);
static_assert(std::is_same_v<Vaelen::int64, long long>);		   // PURITY-ALLOW(R7): the point of the test
static_assert(std::is_same_v<Vaelen::uint64, unsigned long long>); // PURITY-ALLOW(R7): the point of the test
static_assert(std::is_signed_v<Vaelen::int64> && std::is_unsigned_v<Vaelen::uint64>);
static_assert(std::is_same_v<Vaelen::usize, std::size_t>);
static_assert(std::endian::native == std::endian::little);

// NonCopyable: copies deleted, moves defaulted, usable as a base.
static_assert(!std::is_copy_constructible_v<Owner>);
static_assert(!std::is_copy_assignable_v<Owner>);
static_assert(std::is_nothrow_move_constructible_v<Owner>);
static_assert(std::is_nothrow_move_assignable_v<Owner>);
static_assert(std::is_nothrow_default_constructible_v<Owner>);

// ArrayCount and ToUnderlying are constant expressions with the right types.
constexpr int Seven[7] = {};
static_assert(Vaelen::ArrayCount(Seven) == 7);
static_assert(std::is_same_v<decltype(Vaelen::ArrayCount(Seven)), Vaelen::usize>);
static_assert(std::is_same_v<decltype(Vaelen::ToUnderlying(Vaelen::IdKind::Map)), Vaelen::uint8>);
static_assert(Vaelen::ToUnderlying(Vaelen::IdKind::Map) == 51);

// The export macro expands to nothing in the headless static build: neither
// switch may be defined, and the macro must be usable on a declaration.
#if defined(VAELEN_CORE_EXPORTS) || defined(VAELEN_CORE_IMPORTS)
#	error "the headless static build must not define VAELEN_CORE_EXPORTS / VAELEN_CORE_IMPORTS"
#endif
VAELEN_CORE_API constexpr int ExportMacroIsUsableOnDeclarations = 1;
static_assert(ExportMacroIsUsableOnDeclarations == 1);

VAELEN_TEST(CoreTypes, MoveOnlyOwnerBehaves)
{
	Owner A;
	A.Value = 5;
	Owner B(static_cast<Owner&&>(A));
	VT_CHECK_EQ(B.Value, 5);
	Owner C;
	C = static_cast<Owner&&>(B);
	VT_CHECK_EQ(C.Value, 5);
	VT_CHECK_EQ(Vaelen::ArrayCount(Seven), Vaelen::usize{7});
	VT_CHECK_EQ(Vaelen::ToUnderlying(Vaelen::IdKind::Person), Vaelen::uint8{23});
}
