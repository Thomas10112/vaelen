// VAELEN - VaelenSim
// The world map state block: generation config, grid and named tile layers.
//
// STATUS: VALIDATED (Phase 02) - unit/deterministic/edge tests in Tests/Sim
//
// Layers are declared by setup code (AddLayer<T>) like component types: the set
// of layers is CODE and must be identical on both sides of a snapshot; the
// config, the grid and the layer contents are STATE. Generation stages (02.02+)
// fill the layers from the seed; Phase 02 does not tick them.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/TileGrid.h"

#include <memory>
#include <string_view>
#include <vector>

namespace Vaelen
{
	/// Plain data: everything that parameterises world generation besides the seed.
	struct WorldGenConfig
	{
		uint32 Width = 256;
		uint32 Height = 256;
		int64 SeaLevel = 0;		 ///< Q32.32 elevation of the sea (02.03)
		uint32 Reserved[4] = {}; ///< stage parameters arrive with their stages; zero until then

		constexpr bool IsValid() const noexcept { return WorldGrid{Width, Height}.IsValid(); }
		constexpr WorldGrid Grid() const noexcept { return WorldGrid{Width, Height}; }
		constexpr bool operator==(const WorldGenConfig&) const noexcept = default;
	};
	static_assert(IsPlainData<WorldGenConfig>, "WorldGenConfig is serialised as raw bytes");

	template <typename T>
	struct TileLayerId
	{
		uint16 Index = 0xffff;
		constexpr bool IsValid() const noexcept { return Index != 0xffff; }
	};

	class VAELEN_SIM_API WorldMap
	{
	public:
		static constexpr uint32 MaxLayers = 64;

		WorldMap() = default;
		WorldMap(const WorldMap&) = delete;
		WorldMap& operator=(const WorldMap&) = delete;

		// ── Setup (code) ─────────────────────────────────────────────────────
		/// Declares a layer under a unique name. Sized on Reset. Returns an
		/// invalid id (with a report) on a duplicate name or too many layers.
		template <typename T>
		TileLayerId<T> AddLayer(std::string_view Name)
		{
			const Hash64 NameHash = HashString(Name);
			if (!CanAddLayer(NameHash))
			{
				return TileLayerId<T>{};
			}
			Layers.push_back(std::make_unique<TileLayer<T>>(NameHash));
			Layers.back()->Reset(Grid().IsValid() ? Grid().TileCount() : 0);
			return TileLayerId<T>{static_cast<uint16>(Layers.size() - 1)};
		}
		uint32 LayerCount() const noexcept { return static_cast<uint32>(Layers.size()); }
		ITileLayer* GetLayerBase(uint32 Index) noexcept
		{
			return Index < Layers.size() ? Layers[Index].get() : nullptr;
		}
		const ITileLayer* GetLayerBase(uint32 Index) const noexcept
		{
			return Index < Layers.size() ? Layers[Index].get() : nullptr;
		}
		/// Digest of the declared layer set (names, sizes, order): code identity.
		Hash64 LayoutDigest() const noexcept;

		// ── State ────────────────────────────────────────────────────────────
		/// Adopts a config and resizes every layer to zero-filled values.
		/// Returns false (with a report, map untouched) on an invalid config.
		bool Reset(const WorldGenConfig& InConfig);
		const WorldGenConfig& Config() const noexcept { return Configuration; }
		const WorldGrid& Grid() const noexcept { return Bounds; }
		bool IsReady() const noexcept { return Bounds.IsValid(); }

		template <typename T>
		TileLayer<T>& GetLayer(TileLayerId<T> Id) noexcept
		{
			return static_cast<TileLayer<T>&>(*CheckedLayer(Id.Index, static_cast<uint32>(sizeof(T))));
		}
		template <typename T>
		const TileLayer<T>& GetLayer(TileLayerId<T> Id) const noexcept
		{
			return static_cast<const TileLayer<T>&>(*CheckedLayer(Id.Index, static_cast<uint32>(sizeof(T))));
		}

		/// Digest of the whole state block (config, grid, every layer).
		Hash64 StateDigest() const noexcept;

		/// Snapshot section (symmetric). Returns false when loading failed; the
		/// caller reports why through the archive's state and its own checks.
		bool Serialize(IArchive& Ar);

	private:
		bool CanAddLayer(Hash64 NameHash) const noexcept;
		ITileLayer* CheckedLayer(uint32 Index, uint32 ElementSize) noexcept;
		const ITileLayer* CheckedLayer(uint32 Index, uint32 ElementSize) const noexcept;

		WorldGenConfig Configuration;
		WorldGrid Bounds;
		std::vector<std::unique_ptr<ITileLayer>> Layers;
		/// Scratch layer for failed lookups, never serialised.
		TileLayer<uint8> Scratch{0};
	};
} // namespace Vaelen
