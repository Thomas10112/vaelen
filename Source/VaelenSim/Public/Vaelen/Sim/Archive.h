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
#include "Vaelen/Core/Hash.h"
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
		/// Bytes still readable (loading) or no limit (saving). Lets loaders
		/// refuse element counts that the image cannot possibly contain.
		virtual usize RemainingBytes() const noexcept = 0;
		/// Puts a loading archive into its sticky error state (a refused count,
		/// a failed consistency check). No effect while saving.
		virtual void Fail() noexcept = 0;
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
		usize RemainingBytes() const noexcept override { return ~usize{0}; }
		void Fail() noexcept override {}
		usize BytesWritten() const noexcept { return Out->size(); }

	private:
		std::vector<uint8>* Out;
	};

	/// 16.13: FOLDS THE BYTES AND KEEPS NONE OF THEM.
	///
	/// `ComputeStateDigest` used to build an entire snapshot in memory and read
	/// the eight-byte trailer off the end of it. At AELVOR 128 with four
	/// centuries behind it that is 2.37 GB produced and thrown away per call,
	/// and a std::vector that doubles as it grows needs about twice that
	/// transient - which is why the 300+120 gate cell could not be COMPARED on
	/// a 16 GB machine at all. The digest is a number; producing two gigabytes
	/// to read eight bytes off the end of it was the cost of not having this
	/// class.
	///
	/// IT IS BIT-IDENTICAL TO THE TRAILER BY CONSTRUCTION, and that is a
	/// property of FNV-1a rather than a promise anybody has to keep. The
	/// trailer is `HashBytes` over every byte of the image; `HashBytes` takes
	/// the running value as its seed; so folding chunk by chunk and hashing the
	/// whole buffer at once are the same arithmetic in the same order. Had the
	/// project hashed with anything block-structured, this class could not have
	/// existed without changing every frozen digest in the repository.
	///
	/// It allocates nothing and it never fails: there is no buffer to run out
	/// of. A caller that wants to know whether the image could be WRITTEN must
	/// still ask `SaveSnapshot`.
	class VAELEN_SIM_API HashingWriter final : public IArchive
	{
	public:
		bool IsLoading() const noexcept override { return false; }
		bool SerializeBytes(void* Data, usize Size) noexcept override
		{
			H = HashBytes(static_cast<const char*>(Data), Size, H);
			Count += Size;
			return true;
		}
		bool HasError() const noexcept override { return false; }
		usize RemainingBytes() const noexcept override { return ~usize{0}; }
		void Fail() noexcept override {}
		/// The hash of everything written so far - the value a trailer over
		/// those same bytes would hold.
		Hash64 Digest() const noexcept { return H; }
		usize BytesWritten() const noexcept { return Count; }

	private:
		Hash64 H = HashConstants::Fnv1a64Offset;
		usize Count = 0;
	};

	/// Reads from a caller-owned byte range with bounds checking.
	class VAELEN_SIM_API MemoryReader final : public IArchive
	{
	public:
		MemoryReader(const uint8* InBytes, usize InSize) noexcept : Bytes(InBytes), Size(InSize) {}
		bool IsLoading() const noexcept override { return true; }
		bool SerializeBytes(void* Data, usize Count) noexcept override;
		bool HasError() const noexcept override { return Error; }
		usize RemainingBytes() const noexcept override { return Size - Offset; }
		void Fail() noexcept override { Error = true; }
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
	/// loading, a count above MaxCount or larger than the remaining bytes can
	/// hold is rejected before any allocation (error flag set).
	template <typename T>
	bool SerializeVector(IArchive& Ar, std::vector<T>& Values, uint64 MaxCount = uint64{1} << 32) noexcept
	{
		static_assert(std::is_trivially_copyable_v<T>, "SerializeVector requires trivially copyable elements");
		uint64 Count = Values.size();
		Ar << Count;
		if (Ar.IsLoading())
		{
			if (Ar.HasError() || Count > MaxCount || Count > Ar.RemainingBytes() / sizeof(T))
			{
				Ar.Fail();
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
