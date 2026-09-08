// VAELEN - VaelenInfrastructure
// Phase 09.05: decay and ruins - everything built falls down unless somebody
// keeps it, and what falls stays on the ground.
//
// STATUS: PROTOTYPE (Phase 09) - unit/long-duration/edge tests in Tests/Infrastructure
//
// 09.01 raises things and 09.02 makes them matter. Neither takes anything away,
// which means a world that runs long enough is a world where every region has
// every work at its cap and nothing has ever been lost. That is not a living
// world; it is an inventory.
//
// A work wears at a rate set by what it is - a granary of wood goes faster than
// a wall of stone - and faster still in a year the weather struck it (a flood or
// an eruption of 03.05, never a drought or a plague: those kill people, not
// walls) or a war stood on it (a host on the region, or a siege before its
// seat). Against that, the region pays a little of its common stock every year
// to mend what it has. What it cannot pay for wears; what wears to nothing
// falls.
//
// A fallen work is not deleted. It stays on the ground as a ruin, and the
// region remembers it: raising that kind again on ground that already holds its
// ruin costs less, because the stone is already there. That is the whole of
// what a ruin does, and it is enough to make where a world has already been
// matter to where it goes next.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Infrastructure/Buildings.h"
#include "Vaelen/Infrastructure/InfrastructureApi.h"
#include "Vaelen/Military/Armies.h"
#include "Vaelen/Military/Siege.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Sim/Disasters.h"
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
	struct DecayRules
	{
		/// Per mille of repair a work loses in a quiet year, by Work.
		uint32 WearPerYear[8] = {70, 60, 60, 25, 0, 0, 0, 0};
		/// Timber a year, per size, to keep one sound, by Work.
		uint32 UpkeepPerSize[8] = {3, 3, 3, 2, 0, 0, 0, 0};
		uint32 MendPerYear = 220;			  ///< per mille regained in a year it is kept
		uint32 StormWear[3] = {90, 200, 400}; ///< extra per mille by the severity of a flood or an eruption
		uint32 WarWear = 250;				  ///< extra in a year a foreign host stood on the region
		uint32 SiegeWear = 400;				  ///< extra in a year a host sat before the seat
	};

	/// A work wore down to nothing and fell (Amount = the size it had).
	inline constexpr EventType<WorksPayload> BuildingFellEvent = MakeEventType<WorksPayload>("BuildingFell");

	/// Yearly, before Buildings: what the year took off everything standing, and
	/// what that finished. It runs first so that the region sees what it actually
	/// has before it decides what to raise or enlarge.
	class VAELEN_INFRASTRUCTURE_API DecaySystem final : public ISystem
	{
	public:
		DecaySystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::FamilyTypes InFamilies,
					Economy::EconomyTypes InEconomy, InfrastructureTypes InBuildings, DecayRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Families(InFamilies), Economy(InEconomy), Buildings(InBuildings),
			  Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Decay"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			// After the disasters of the year, so that a flood is felt by the walls
			// in the year it struck rather than a year late.
			std::vector<std::string_view> Out{"Production", "Disasters"};
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		/// Optional: the hosts of 08.01 and who rules what (07.01). A foreign host
		/// standing on a region wears everything on it.
		void ObserveWar(Politics::PolityTypes InPolities, Military::ArmyTypes InArmies,
						Military::SiegeTypes InSieges) noexcept
		{
			Polities = InPolities;
			Armies = InArmies;
			Sieges = InSieges;
			HasWar = true;
		}
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::FamilyTypes Families;
		Economy::EconomyTypes Economy;
		InfrastructureTypes Buildings;
		DecayRules Rules;
		Politics::PolityTypes Polities;
		Military::ArmyTypes Armies;
		Military::SiegeTypes Sieges;
		bool HasWar = false;
	};

	/// Ruins of one kind on a region, oldest first.
	VAELEN_INFRASTRUCTURE_API void RuinsIn(const World& W, const InfrastructureTypes& Buildings, uint32 Region,
										   Work Kind, std::vector<uint32>& Out);
	/// True when a region holds a ruin of that kind to build back on.
	VAELEN_INFRASTRUCTURE_API bool HasRuin(const World& W, const InfrastructureTypes& Buildings, uint32 Region,
										   Work Kind);

	struct DecayStats
	{
		uint32 Standing = 0; ///< works still up
		uint32 Sound = 0;	 ///< of those, in full repair
		uint32 Worn = 0;	 ///< of those, worn at all
		uint32 Fallen = 0;	 ///< ruins on the ground
		uint32 Rebuilt = 0;	 ///< works raised on ground that already held a ruin of their kind
		uint32 Least = 1000; ///< the worst repair still standing
		uint32 Falls = 0;	 ///< events, from the log
		uint32 Bad = 0;		 ///< incoherent: a ruin in repair, a standing work at nothing
		Hash64 Digest = 0;	 ///< every repair, in building order
	};

	/// Counts what the years have taken and checks the two rules a fall must
	/// keep: a work that fell has no repair left and a tick that says when, and a
	/// work still standing has some.
	VAELEN_INFRASTRUCTURE_API DecayStats MeasureDecay(const World& W, const InfrastructureTypes& Buildings,
													  const DecayRules& Rules);
} // namespace Vaelen::Infrastructure
