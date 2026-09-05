// VAELEN - VaelenSim
// Byte archives for snapshots: one Serialize function per type works in both
// directions, so save and load can never drift apart.
//
// STATUS: VALIDATED (Phase 01) - unit/deterministic/edge tests in Tests/Sim;
//         integration and long-duration tests arrive with 01.07 / 01.08.
//
// No exceptions: a read past the end sets the error flag, zero-fills the
// destination and every later read also fails; callers check HasError() once
// at the end. The byte order is the host's (little-endian is asserted in
// CoreTypes.h), so images are portable across every supported platform.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/SimApi.h"

#include <cstring>
#include <type_traits>
#include <vector>

namespace Vaelen
{
	class VAELEN_SIM_API IArchive
	{
	public:
		virtual ~IArchive() = default;
		virtual bool IsLoading() const noexcept = 0;
		/// Writes or reads Size raw bytes. Returns false once the archive is in error.
		virtual bool SerializeBytes(void* Data, usize Size) noexcept = 0;
		virtual bool HasError() const noexcept = 0;
		bool IsSaving() const noexcept { return !IsLoading(); }
	};

	/// Appends to a caller-owned byte vector.
	class VAELEN_SIM_API MemoryWriter final : public IArchive
	{
	public:
		explicit MemoryWriter(std::vector<uint8>& InOut) noexcept : Out(&InOut) {}
		bool IsLoading() const noexcept override { return false; }
		bool SerializeBytes(void* Data, usize Size) noexcept override;
		bool HasError() const noexcept override { return false; }
		usize BytesWritten() const noexcept { return Out->size(); }

	private:
		std::vector<uint8>* Out;
	};

	/// Reads from a caller-owned byte range with bounds checking.
	class VAELEN_SIM_API MemoryReader final : public IArchive
	{
	public:
		MemoryReader(const uint8* InBytes, usize InSize) noexcept : Bytes(InBytes), Size(InSize) {}
		bool IsLoading() const noexcept override { return true; }
		bool SerializeBytes(void* Data, usize Count) noexcept override;
		bool HasError() const noexcept override { return Error; }
		usize Position() const noexcept { return Offset; }
		usize Remaining() const noexcept { return Size - Offset; }
		bool AtEnd() const noexcept { return Offset == Size; }

	private:
		const uint8* Bytes;
		usize Size;
		usize Offset = 0;
		bool Error = false;
	};

	/// Arithmetic types and enums are serialised as their raw bytes.
	template <typename T>
	std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, IArchive&> operator<<(IArchive& Ar,
																						 T& Value) noexcept
	{
		Ar.SerializeBytes(&Value, sizeof(T));
		return Ar;
	}

	/// A vector of trivially copyable elements: count, then raw bytes. When
	/// loading, counts above MaxCount are rejected (error flag) so a corrupt
	/// count cannot request an absurd allocation.
	template <typename T>
	bool SerializeVector(IArchive& Ar, std::vector<T>& Values, uint64 MaxCount = uint64{1} << 32) noexcept
	{
		static_assert(std::is_trivially_copyable_v<T>, "SerializeVector requires trivially copyable elements");
		uint64 Count = Values.size();
		Ar << Count;
		if (Ar.IsLoading())
		{
			if (Ar.HasError() || Count > MaxCount)
			{
				Values.clear();
				return false;
			}
			Values.resize(static_cast<usize>(Count));
		}
		if (Count > 0)
		{
			Ar.SerializeBytes(Values.data(), static_cast<usize>(Count) * sizeof(T));
		}
		return !Ar.HasError();
	}
} // namespace Vaelen
