// VAELEN - VaelenSim
// World map state block.
//
// STATUS: VALIDATED (Phase 02) - covered by Tests/Sim/Test_WorldMap.cpp
#include "Vaelen/Sim/WorldMap.h"
#include "Vaelen/Core/Assert.h"

namespace Vaelen
{
	bool WorldMap::CanAddLayer(Hash64 NameHash) const noexcept
	{
		if (Layers.size() >= MaxLayers)
		{
			VAELEN_CHECKF(false, "WorldMap: more than %u layers", MaxLayers);
			return false;
		}
		for (const std::unique_ptr<ITileLayer>& L : Layers)
		{
			if (L->NameHash() == NameHash)
			{
				VAELEN_CHECKF(false, "WorldMap: duplicate layer name hash %016llx",
							  static_cast<unsigned long long>(NameHash));
				return false;
			}
		}
		return true;
	}

	Hash64 WorldMap::LayoutDigest() const noexcept
	{
		Hash64 Digest = HashString("VaelenWorldMapLayout");
		for (const std::unique_ptr<ITileLayer>& L : Layers)
		{
			Digest = HashCombine(Digest, L->NameHash());
			Digest = HashCombine(Digest, HashUInt64(L->ElementSize()));
		}
		return Digest;
	}

	bool WorldMap::Reset(const WorldGenConfig& InConfig)
	{
		if (!InConfig.IsValid())
		{
			VAELEN_CHECKF(false, "WorldMap::Reset: invalid config %u x %u", InConfig.Width, InConfig.Height);
			return false;
		}
		Configuration = InConfig;
		Bounds = InConfig.Grid();
		for (std::unique_ptr<ITileLayer>& L : Layers)
		{
			L->Reset(Bounds.TileCount());
		}
		return true;
	}

	ITileLayer* WorldMap::CheckedLayer(uint32 Index, uint32 ElementSize) noexcept
	{
		const bool Ok = Index < Layers.size() && Layers[Index]->ElementSize() == ElementSize;
		VAELEN_CHECKF(Ok, "WorldMap: layer %u unknown or of another element size", Index);
		return Ok ? Layers[Index].get() : &Scratch;
	}

	const ITileLayer* WorldMap::CheckedLayer(uint32 Index, uint32 ElementSize) const noexcept
	{
		const bool Ok = Index < Layers.size() && Layers[Index]->ElementSize() == ElementSize;
		VAELEN_CHECKF(Ok, "WorldMap: layer %u unknown or of another element size", Index);
		return Ok ? Layers[Index].get() : &Scratch;
	}

	Hash64 WorldMap::StateDigest() const noexcept
	{
		Hash64 Digest = HashBytes(reinterpret_cast<const char*>(&Configuration), sizeof(Configuration));
		Digest = HashCombine(Digest, HashUInt64((uint64{Bounds.Width} << 32) | Bounds.Height));
		for (const std::unique_ptr<ITileLayer>& L : Layers)
		{
			Digest = HashCombine(Digest, L->Hash());
		}
		return Digest;
	}

	bool WorldMap::Serialize(IArchive& Ar)
	{
		// Config and grid (state). An unset map serialises as an all-zero
		// config with a 0 x 0 grid and empty layers.
		WorldGenConfig Config = Configuration;
		uint32 Width = Bounds.Width;
		uint32 Height = Bounds.Height;
		Ar.SerializeBytes(&Config, sizeof(Config));
		Ar << Width << Height;
		uint32 Count = LayerCount();
		Ar << Count;
		if (Ar.IsLoading())
		{
			if (Ar.HasError() || Count != LayerCount())
			{
				Ar.Fail();
				return false;
			}
			const bool Unset = Width == 0 && Height == 0;
			if (Unset)
			{
				Configuration = Config;
				Bounds = WorldGrid{};
				for (std::unique_ptr<ITileLayer>& L : Layers)
				{
					L->Reset(0);
				}
			}
			else if (!Config.IsValid() || Config.Grid() != WorldGrid{Width, Height} || !Reset(Config))
			{
				Ar.Fail();
				return false;
			}
		}
		for (std::unique_ptr<ITileLayer>& L : Layers)
		{
			Hash64 Name = L->NameHash();
			uint32 ElementSize = L->ElementSize();
			Ar << Name << ElementSize;
			if (Ar.IsLoading() && (Ar.HasError() || Name != L->NameHash() || ElementSize != L->ElementSize()))
			{
				Ar.Fail();
				return false;
			}
			if (!L->Serialize(Ar))
			{
				return false;
			}
			if (Ar.IsLoading() && L->Count() != Bounds.TileCount())
			{
				Ar.Fail();
				return false;
			}
		}
		return !Ar.HasError();
	}
} // namespace Vaelen
