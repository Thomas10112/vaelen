// VAELEN - VaelenSim
// Explicit component type registry.
//
// STATUS: VALIDATED (Phase 01) - covered by Tests/Sim/Test_ComponentType.cpp
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Core/Assert.h"

namespace Vaelen
{
	namespace
	{
		const ComponentTypeInfo GInvalidInfo{};
	}

	ComponentTypeId ComponentTypeRegistry::RegisterRaw(const char* Name, uint32 Size, uint32 Alignment)
	{
		VAELEN_CHECKF(Name != nullptr && Name[0] != '\0', "component type names must not be empty");
		if (Name == nullptr || Name[0] == '\0')
		{
			return InvalidComponentTypeId;
		}
		const Hash64 NameHash = HashString(Name);
		VAELEN_CHECKF(FindByHash(NameHash) == InvalidComponentTypeId, "component type '%s' is already registered",
					  Name);
		if (FindByHash(NameHash) != InvalidComponentTypeId)
		{
			return InvalidComponentTypeId;
		}
		VAELEN_CHECKF(Infos.size() < MaxComponentTypes, "component type registry is full");
		if (Infos.size() >= MaxComponentTypes)
		{
			return InvalidComponentTypeId;
		}
		ComponentTypeInfo Info;
		Info.Name = Name;
		Info.NameHash = NameHash;
		Info.Size = Size;
		Info.Alignment = Alignment;
		Infos.push_back(Info);
		return static_cast<ComponentTypeId>(Infos.size() - 1);
	}

	ComponentTypeId ComponentTypeRegistry::FindByName(std::string_view Name) const noexcept
	{
		return FindByHash(HashString(Name));
	}

	ComponentTypeId ComponentTypeRegistry::FindByHash(Hash64 NameHash) const noexcept
	{
		for (usize i = 0; i < Infos.size(); ++i)
		{
			if (Infos[i].NameHash == NameHash)
			{
				return static_cast<ComponentTypeId>(i);
			}
		}
		return InvalidComponentTypeId;
	}

	const ComponentTypeInfo& ComponentTypeRegistry::GetInfo(ComponentTypeId Id) const noexcept
	{
		return Id < Infos.size() ? Infos[Id] : GInvalidInfo;
	}

	Hash64 ComponentTypeRegistry::LayoutDigest() const noexcept
	{
		Hash64 Digest = HashString("VaelenComponentLayout");
		for (const ComponentTypeInfo& Info : Infos)
		{
			Digest = HashCombine(Digest, Info.NameHash);
			Digest = HashCombine(Digest, static_cast<uint64>(Info.Size) << 32 | Info.Alignment);
		}
		return Digest;
	}
} // namespace Vaelen
