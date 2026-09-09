// VAELEN - VaelenInfrastructure
// Phase 09.02: what a building does.
//
// STATUS: VALIDATED (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure

#include "Vaelen/Infrastructure/Works.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Infrastructure
{
	namespace
	{
		/// The one rule of this task: a size becomes a per-mille, capped.
		uint32 Worth(uint32 Kept, uint32 PerSize, uint32 Most) noexcept
		{
			const uint64 Raw = uint64{Kept} * uint64{PerSize};
			return static_cast<uint32>(std::min<uint64>(Most, Raw));
		}

		uint32 WorthByKind(const RegionWorks& Held, const WorksRules& Rules, uint32 Kind) noexcept
		{
			switch (static_cast<Work>(Kind))
			{
			case Work::Granary:
				return Worth(Held.Kept[static_cast<uint32>(Work::Granary)], Rules.GranaryPerSize, Rules.GranaryMost);
			case Work::Mill:
				return Worth(Held.Kept[static_cast<uint32>(Work::Mill)], Rules.MillPerSize, Rules.MillMost);
			case Work::Smithy:
				return Worth(Held.Kept[static_cast<uint32>(Work::Smithy)], Rules.SmithyPerSize, Rules.SmithyMost);
			case Work::Wall:
				return Worth(Held.Kept[static_cast<uint32>(Work::Wall)], Rules.WallPerSize, Rules.WallMost);
			case Work::Count:
				break;
			}
			return 0;
		}
	} // namespace

	WorksTypes WorksTypes::Declare(World& W)
	{
		WorksTypes T;
		T.Shops = W.Types().Register<Economy::RegionWorkshops>("RegionWorkshops");
		T.Walls = W.Types().Register<Military::RegionWall>("RegionWall");
		W.Components().CreatePool(T.Shops);
		W.Components().CreatePool(T.Walls);
		return T;
	}

	void WorksSystem::Tick(TickContext& Context)
	{
		(void)Context;
		World& W = *Owner;
		std::vector<EntityHandle> RegionHandles;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= RegionHandles.size())
					{
						RegionHandles.resize(usize{R.Index} + 1u);
					}
					RegionHandles[R.Index] = H;
				});
		for (uint32 R = 1; R < RegionHandles.size(); ++R)
		{
			const EntityHandle H = RegionHandles[R];
			if (H.IsNull())
			{
				continue;
			}
			const RegionWorks* Held = W.Components().GetPool(Buildings.Works).TryGet(H);
			// A region that never built anything is left alone; one whose works
			// have all fallen is written back to nothing, which is why the
			// numbers are recomputed rather than added to.
			const RegionWorks Nothing;
			const RegionWorks& Standing = Held != nullptr ? *Held : Nothing;
			const bool Any = Held != nullptr;

			const uint32 Granary = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Granary));
			const uint32 Fields = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Mill));
			const uint32 Craft = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Smithy));
			const uint32 Wall = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Wall));

			if (HasStores)
			{
				Population::RegionStores* Put = W.Components().GetPool(Stores).TryGet(H);
				if (Put != nullptr)
				{
					Put->BuiltPerMille = Granary;
				}
				else if (Granary != 0)
				{
					Population::RegionStores Fresh;
					Fresh.BuiltPerMille = Granary;
					W.Components().GetPool(Stores).Add(H, Fresh);
				}
			}

			Economy::RegionWorkshops* Shop = W.Components().GetPool(Works.Shops).TryGet(H);
			if (Shop != nullptr)
			{
				Shop->FieldsPerMille = Fields;
				Shop->CraftPerMille = Craft;
			}
			else if (Any && (Fields != 0 || Craft != 0))
			{
				W.Components().GetPool(Works.Shops).Add(H, Economy::RegionWorkshops{Fields, Craft});
			}

			Military::RegionWall* Stone = W.Components().GetPool(Works.Walls).TryGet(H);
			if (Stone != nullptr)
			{
				Stone->Extra = Wall;
			}
			else if (Any && Wall != 0)
			{
				W.Components().GetPool(Works.Walls).Add(H, Military::RegionWall{Wall, 0u});
			}
		}
	}

	uint32 WorthOf(const World& W, const History::PreHistoryTypes& Types, const InfrastructureTypes& Buildings,
				   const WorksRules& Rules, uint32 Region, Work Kind)
	{
		const uint32 K = static_cast<uint32>(Kind);
		if (K >= WorkCount)
		{
			return 0;
		}
		const RegionWorks* Held = WorksOf(W, Types, Buildings, Region);
		return Held != nullptr ? WorthByKind(*Held, Rules, K) : 0u;
	}

	WorksStats MeasureWorks(const World& W, const History::PreHistoryTypes& Types, const InfrastructureTypes& Buildings,
							const WorksTypes& Works, const WorksRules& Rules,
							const ComponentType<Population::RegionStores>* Stores)
	{
		WorksStats S;
		struct Row
		{
			uint32 Region = 0;
			uint32 Granary = 0;
			uint32 Fields = 0;
			uint32 Craft = 0;
			uint32 Wall = 0;
			uint32 WantGranary = 0;
			uint32 WantFields = 0;
			uint32 WantCraft = 0;
			uint32 WantWall = 0;
			bool Written = false;
		};
		std::vector<Row> Rows;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					Row Out;
					Out.Region = R.Index;
					const RegionWorks* Held = W.Components().GetPool(Buildings.Works).TryGet(H);
					const RegionWorks Nothing;
					const RegionWorks& Standing = Held != nullptr ? *Held : Nothing;
					Out.WantGranary = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Granary));
					Out.WantFields = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Mill));
					Out.WantCraft = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Smithy));
					Out.WantWall = WorthByKind(Standing, Rules, static_cast<uint32>(Work::Wall));
					if (Stores != nullptr)
					{
						const Population::RegionStores* Put = W.Components().GetPool(*Stores).TryGet(H);
						if (Put != nullptr)
						{
							Out.Granary = Put->BuiltPerMille;
							Out.Written = true;
						}
					}
					const Economy::RegionWorkshops* Shop = W.Components().GetPool(Works.Shops).TryGet(H);
					if (Shop != nullptr)
					{
						Out.Fields = Shop->FieldsPerMille;
						Out.Craft = Shop->CraftPerMille;
						Out.Written = true;
					}
					const Military::RegionWall* Stone = W.Components().GetPool(Works.Walls).TryGet(H);
					if (Stone != nullptr)
					{
						Out.Wall = Stone->Extra;
						Out.Written = true;
					}
					Rows.push_back(Out);
				});
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Region < B.Region; });

		Hash64 D = HashString("Works");
		for (const Row& R : Rows)
		{
			if (!R.Written)
			{
				// Nothing written is right only where nothing is owed.
				S.Bad += (R.WantFields != 0 || R.WantCraft != 0 || R.WantWall != 0) ? 1u : 0u;
				continue;
			}
			const uint32 Numbers[4] = {R.Granary, R.Fields, R.Craft, R.Wall};
			D = HashCombine(D, HashUInt64(R.Region));
			for (const uint32 N : Numbers)
			{
				D = HashCombine(D, HashUInt64(N));
			}
			// Every number written must be the arithmetic of what stands there.
			if (R.Fields != R.WantFields || R.Craft != R.WantCraft || R.Wall != R.WantWall)
			{
				++S.Bad;
			}
			if (Stores != nullptr && R.Granary != R.WantGranary)
			{
				++S.Bad;
			}
			if (R.Granary > Rules.GranaryMost || R.Fields > Rules.MillMost || R.Craft > Rules.SmithyMost ||
				R.Wall > Rules.WallMost)
			{
				++S.Bad;
			}
			S.Granaries += R.Granary != 0 ? 1u : 0u;
			S.Fields += R.Fields != 0 ? 1u : 0u;
			S.Craft += R.Craft != 0 ? 1u : 0u;
			S.Walls += R.Wall != 0 ? 1u : 0u;
			S.MostGranary = std::max(S.MostGranary, R.Granary);
			S.MostFields = std::max(S.MostFields, R.Fields);
			S.MostCraft = std::max(S.MostCraft, R.Craft);
			S.MostWall = std::max(S.MostWall, R.Wall);
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Infrastructure
