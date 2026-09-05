// VAELEN - VaelenSim
// Square tile grid and dense typed tile layers: the world's terrain state.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// Tiles are not entities (ROADMAP section 6): a grid of Width x Height tiles is
// addressed by TileCoord or by row-major index, and every per-tile value lives
// in a TileLayer<T> - one dense vector per layer, plain data, hashable and
// serialisable as raw bytes. Neighbour order is fixed (N, NE, E, SE, S, SW, W,
// NW) so every algorithm that walks neighbours is deterministic by construction.
#pragma once

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Archive.h"
#include "Vaelen/Sim/PlainData.h"
#include "Vaelen/Sim/SimApi.h"

#include <cstring>
#include <vector>

namespace Vaelen
{
	struct TileCoord
	{
		int32 X = 0;
		int32 Y = 0;
		constexpr bool operator==(const TileCoord&) const noexcept = default;
	};

	/// Fixed neighbour order: N, NE, E, SE, S, SW, W, NW (Y grows southwards).
	inline constexpr TileCoord NeighbourOffsets[8] = {{0, -1}, {1, -1}, {1, 0},	 {1, 1},
													  {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}};
	inline constexpr uint32 CardinalNeighbourIndices[4] = {0, 2, 4, 6};

	struct WorldGrid
	{
		static constexpr uint32 MaxDimension = 4096;

		uint32 Width = 0;
		uint32 Height = 0;

		constexpr bool IsValid() const noexcept
		{
			return Width > 0 && Height > 0 && Width <= MaxDimension && Height <= MaxDimension;
		}
		constexpr uint32 TileCount() const noexcept { return Width * Height; }
		constexpr bool Contains(TileCoord C) const noexcept
		{
			return C.X >= 0 && C.Y >= 0 && static_cast<uint32>(C.X) < Width && static_cast<uint32>(C.Y) < Height;
		}
		/// Row-major index of an in-bounds coordinate (Check failure otherwise; returns 0).
		uint32 IndexOf(TileCoord C) const noexcept
		{
			const bool Inside = Contains(C);
			VAELEN_CHECKF(Inside, "tile (%d, %d) outside a %u x %u grid", C.X, C.Y, Width, Height);
			return Inside ? static_cast<uint32>(C.Y) * Width + static_cast<uint32>(C.X) : 0;
		}
		constexpr TileCoord CoordOf(uint32 Index) const noexcept
		{
			return TileCoord{static_cast<int32>(Index % Width), static_cast<int32>(Index / Width)};
		}
		constexpr bool operator==(const WorldGrid&) const noexcept = default;

		/// Calls Visit(TileCoord, uint32 NeighbourSlot) for every in-bounds
		/// neighbour, in the fixed order; Count is 4 (cardinal) or 8.
		template <typename Visitor>
		void ForEachNeighbour(TileCoord C, uint32 Count, Visitor&& Visit) const
		{
			VAELEN_CHECKF(Count == 4 || Count == 8, "neighbour count must be 4 or 8, got %u", Count);
			for (uint32 i = 0; i < 8; ++i)
			{
				if (Count == 4 && (i & 1u) != 0)
				{
					continue;
				}
				const TileCoord N{C.X + NeighbourOffsets[i].X, C.Y + NeighbourOffsets[i].Y};
				if (Contains(N))
				{
					Visit(N, i);
				}
			}
		}
	};

	/// Type-erased view of a layer, for the map's registry and the snapshot.
	class VAELEN_SIM_API ITileLayer
	{
	public:
		virtual ~ITileLayer() = default;
		virtual Hash64 NameHash() const noexcept = 0;
		virtual uint32 ElementSize() const noexcept = 0;
		virtual uint32 Count() const noexcept = 0;
		virtual void Reset(uint32 TileCount) = 0;
		virtual Hash64 Hash() const noexcept = 0;
		virtual bool Serialize(IArchive& Ar) = 0;
	};

	template <typename T>
	class TileLayer final : public ITileLayer
	{
	public:
		VAELEN_PLAIN_DATA_CHECK(T, "tile layer values");

		explicit TileLayer(Hash64 InNameHash) noexcept : Name(InNameHash) {}

		Hash64 NameHash() const noexcept override { return Name; }
		uint32 ElementSize() const noexcept override { return static_cast<uint32>(sizeof(T)); }
		uint32 Count() const noexcept override { return static_cast<uint32>(Values.size()); }
		void Reset(uint32 TileCount) override { Values.assign(TileCount, T{}); }
		void Fill(const T& Value) { std::fill(Values.begin(), Values.end(), Value); }

		T& operator[](uint32 Index) noexcept
		{
			VAELEN_CHECKF(Index < Values.size(), "tile index %u out of %zu", Index, Values.size());
			return Values[Index < Values.size() ? Index : 0];
		}
		const T& operator[](uint32 Index) const noexcept
		{
			VAELEN_CHECKF(Index < Values.size(), "tile index %u out of %zu", Index, Values.size());
			return Values[Index < Values.size() ? Index : 0];
		}
		T& At(const WorldGrid& Grid, TileCoord C) noexcept { return (*this)[Grid.IndexOf(C)]; }
		const T& At(const WorldGrid& Grid, TileCoord C) const noexcept { return (*this)[Grid.IndexOf(C)]; }

		const std::vector<T>& Data() const noexcept { return Values; }
		std::vector<T>& Data() noexcept { return Values; }

		/// Order-sensitive digest of every byte.
		Hash64 Hash() const noexcept override
		{
			return HashBytes(reinterpret_cast<const char*>(Values.data()), Values.size() * sizeof(T), Name);
		}

		bool Serialize(IArchive& Ar) override
		{
			return SerializeVector(Ar, Values, uint64{WorldGrid::MaxDimension} * WorldGrid::MaxDimension);
		}

	private:
		Hash64 Name;
		std::vector<T> Values;
	};
} // namespace Vaelen
