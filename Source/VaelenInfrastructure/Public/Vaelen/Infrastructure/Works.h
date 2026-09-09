// VAELEN - VaelenInfrastructure
// Phase 09.02: what a building does - the granary of 05.05, the harvest of
// 06.02, the wall of 08.04.
//
// STATUS: VALIDATED (Phase 09) - unit/integration/deterministic/edge tests in Tests/Infrastructure
//
// A building is never a number the simulation reads instead of the world
// (ADR-0074). So nothing here computes anything about famine, harvest or siege:
// every kind of work is turned into the one number an earlier phase already
// reads, and that phase goes on doing exactly what it did.
//
//   granary -> RegionStores::BuiltPerMille, and 04.04 softens the drought
//   mill    -> RegionWorkshops::FieldsPerMille, and 06.02 reaps more
//   smithy  -> RegionWorkshops::CraftPerMille, and 06.02 makes more
//   wall    -> RegionWall::Extra, and 08.04 takes longer to bring it down
//
// Each of those four is a field no other writer touches, so a region can hold
// a council's decision and a granary at once without either undoing the other.
// The numbers are recomputed from what stands every year, so a work that falls
// (09.05) takes its effect with it the same year, without anybody remembering
// to subtract it.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Infrastructure/Buildings.h"
#include "Vaelen/Infrastructure/InfrastructureApi.h"
#include "Vaelen/Military/Siege.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"

#include <string>
#include <string_view>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Infrastructure
{
	/// The types this task writes. RegionStores is not among them: it belongs to
	/// 04.04 and is registered by whoever needs it first (05.05 where a council
	/// exists), so the system observes it rather than declaring it.
	struct WorksTypes
	{
		ComponentType<Economy::RegionWorkshops> Shops;
		ComponentType<Military::RegionWall> Walls;
		static VAELEN_INFRASTRUCTURE_API WorksTypes Declare(World& W);
	};

	struct WorksRules
	{
		uint32 GranaryPerSize = 120; ///< per mille of a drought's cut a granary of one absorbs
		uint32 GranaryMost = 600;	 ///< and no granary ever takes more of it than this
		uint32 MillPerSize = 30;	 ///< per mille added to the harvest
		uint32 MillMost = 150;
		uint32 SmithyPerSize = 40; ///< per mille added to the cloth and the tools
		uint32 SmithyMost = 200;
		uint32 WallPerSize = 300; ///< per mille; a host before a wall of one comes on at 1000/1300
		uint32 WallMost = 1500;
	};

	/// Yearly, after Buildings: what stands, written where the layers below
	/// already read. Writes nothing else and publishes nothing - the buildings
	/// are the event, this is only their arithmetic.
	class VAELEN_INFRASTRUCTURE_API WorksSystem final : public ISystem
	{
	public:
		WorksSystem(World& InWorld, const History::PreHistoryTypes& InTypes, InfrastructureTypes InBuildings,
					WorksTypes InWorks, WorksRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Buildings(InBuildings), Works(InWorks), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Works"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out{"Buildings"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: where a council's grain is recorded (05.05). Without it a
		/// granary still stands and still costs, and softens nothing.
		void ObserveStores(ComponentType<Population::RegionStores> InStores) noexcept
		{
			Stores = InStores;
			HasStores = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		InfrastructureTypes Buildings;
		WorksTypes Works;
		WorksRules Rules;
		ComponentType<Population::RegionStores> Stores;
		bool HasStores = false;
	};

	/// What one kind of work is worth in a region, by the rules - the same
	/// arithmetic the system writes, for a caller that wants to ask rather than
	/// to read the component.
	VAELEN_INFRASTRUCTURE_API uint32 WorthOf(const World& W, const History::PreHistoryTypes& Types,
											 const InfrastructureTypes& Buildings, const WorksRules& Rules,
											 uint32 Region, Work Kind);

	struct WorksStats
	{
		uint32 Granaries = 0; ///< regions whose drought is softened by something built
		uint32 Fields = 0;	  ///< regions whose harvest is lifted
		uint32 Craft = 0;	  ///< regions whose craft is lifted
		uint32 Walls = 0;	  ///< regions whose walls are harder to bring down
		uint32 MostGranary = 0;
		uint32 MostFields = 0;
		uint32 MostCraft = 0;
		uint32 MostWall = 0;
		uint32 Bad = 0;	   ///< a number past its cap, or written where nothing stands
		Hash64 Digest = 0; ///< every written number, in region order
	};

	/// Counts what the works are worth and checks that every number written is
	/// the arithmetic of what actually stands there, within its cap.
	VAELEN_INFRASTRUCTURE_API WorksStats MeasureWorks(const World& W, const History::PreHistoryTypes& Types,
													  const InfrastructureTypes& Buildings, const WorksTypes& Works,
													  const WorksRules& Rules,
													  const ComponentType<Population::RegionStores>* Stores = nullptr);
} // namespace Vaelen::Infrastructure
