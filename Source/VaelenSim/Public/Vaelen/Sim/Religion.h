// VAELEN - VaelenSim
// Phase 03.04: religions.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim
//
// A religion is an entity born from an event: a new era inspires a founder in
// the home region of the largest culture, a culture split may become a schism
// of the faith held there, and later phases (disasters, omens) request a
// founding through ReligionSystem::RequestFounding with the causing event.
// No religion exists without a founding event. Adherents are counted per
// region and per religion (RegionFaith, bounded by the region's population);
// faith spreads yearly inside a region and to the neighbours of a region
// where it is the majority (along the region graph), and travels with
// migration waves. Tenets are eight small integers derived from the
// religion's identity (a schism shifts one or two of them) that later phases
// read. All state is components; every step is integer arithmetic in index
// order.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/EventBus.h"
#include "Vaelen/Sim/Naming.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::History
{
	/// Eight tenet axes, 0-255 each: Authority, Nature, Ancestors, War, Trade,
	/// Mystery, Purity, Tolerance.
	struct Tenets
	{
		static constexpr uint32 Axes = 8;
		uint8 Value[Axes] = {};
	};

	/// Component of a religion entity (ids of kind Religion).
	struct ReligionInfo
	{
		uint32 Index = 0;		  ///< 1-based, in order of founding
		uint32 Culture = 0;		  ///< majority culture of the founding region
		uint32 Parent = 0;		  ///< religion it split from (schism), 0 for a root faith
		uint32 Generation = 0;	  ///< 0 root, parent's + 1 otherwise
		uint32 HomeRegion = 0;	  ///< region index where it was founded
		uint32 Kind = 0;		  ///< FoundingKind
		uint64 Founded = 0;		  ///< tick
		uint64 FoundingEvent = 0; ///< event id that caused the founding (never 0)
		Hash64 Identity = 0;	  ///< derived from the world seed
		Tenets Creed;
	};
	static_assert(sizeof(ReligionInfo) == 56, "ReligionInfo must stay padding free");

	/// Component on every region entity that ever held believers.
	struct RegionFaith
	{
		static constexpr uint32 MaxFaiths = 4;
		uint32 Religion[MaxFaiths] = {};  ///< religion index per slot, 0 = free
		uint32 Adherents[MaxFaiths] = {}; ///< believers per slot
		uint32 Majority = 0;			  ///< religion with the most believers (0 when none)
		uint32 Reserved = 0;
		uint32 SlotOf(uint32 ReligionIndex) const noexcept;
		uint32 Total() const noexcept;
		/// Adds believers; returns false when no slot is free.
		bool Add(uint32 ReligionIndex, uint32 People) noexcept;
		/// Removes believers (clamped); frees the slot at zero.
		uint32 Remove(uint32 ReligionIndex, uint32 People) noexcept;
		void Recount() noexcept;
	};
	static_assert(sizeof(RegionFaith) == 40, "RegionFaith must stay padding free");

	enum class FoundingKind : uint32
	{
		Requested = 0, ///< RequestFounding by a later phase
		Era = 1,	   ///< a new era opened
		Schism = 2,	   ///< a culture split in a region with a faith
	};

	struct FaithRequest
	{
		uint32 Region = 0;
		uint32 Kind = 0;
		uint64 Cause = 0; ///< event id
	};

	/// Singleton component on the faith entity created by InitializeFaith.
	struct FaithState
	{
		static constexpr uint32 MaxPending = 8;
		uint32 ReligionCount = 0;
		uint32 PendingCount = 0;
		uint32 Requested = 0; ///< requests accepted since the start
		uint32 Refused = 0;	  ///< requests dropped: queue full, duplicate region, unsettled region, no slot
		FaithRequest Pending[MaxPending];
	};
	static_assert(sizeof(FaithState) == 16 + 16 * FaithState::MaxPending, "FaithState must stay padding free");

	struct ReligionTypes
	{
		ComponentType<ReligionInfo> Religion;
		ComponentType<RegionFaith> Faith;
		ComponentType<FaithState> State;
		static ReligionTypes Declare(World& W);
	};

	/// Creates the faith entity once (fresh worlds only; a restored world keeps
	/// its own). Returns the null handle when it already exists.
	VAELEN_SIM_API EntityHandle InitializeFaith(World& W, const ReligionTypes& Types);

	struct ReligionRules
	{
		uint32 FoundOnEra = 1;				///< a new era founds a faith in the largest culture's home
		uint32 SchismOnSplit = 1;			///< a culture split may found a schism
		uint32 SchismPerMille = 500;		///< chance of a schism per split (hash of the split event)
		uint32 FoundingSharePerMille = 500; ///< share of the founding region converted at once
		uint32 ConvertPerMille = 60;		///< yearly share of the unconverted joining the majority faith
		uint32 SpreadPerMille = 20;			///< yearly share of a neighbour's unconverted joining it
		uint32 FadeSharePerMille = 10;		///< a faith below this share of a region vanishes there
	};

	struct ReligionPayload
	{
		uint32 Religion = 0;
		uint32 Region = 0;
		uint32 Culture = 0;
		uint32 Parent = 0;
	};
	struct ConversionPayload
	{
		uint32 Region = 0;
		uint32 Religion = 0; ///< new majority (0 when the region lost its faith)
		uint32 Previous = 0;
		uint32 Adherents = 0;
	};

	inline constexpr EventType<ReligionPayload> ReligionFoundedEvent =
		MakeEventType<ReligionPayload>("ReligionFounded");
	inline constexpr EventType<ReligionPayload> SchismEvent = MakeEventType<ReligionPayload>("Schism");
	inline constexpr EventType<ConversionPayload> RegionConvertedEvent =
		MakeEventType<ConversionPayload>("RegionConverted");

	/// Tenets from an identity hash.
	VAELEN_SIM_API Tenets DeriveTenets(Hash64 Identity) noexcept;
	/// The parent's tenets with one or two axes shifted by the salt.
	VAELEN_SIM_API Tenets SchismTenets(const Tenets& Parent, Hash64 Salt) noexcept;

	/// Yearly: clamps believers to the population, founds the requested
	/// religions, spreads every majority faith inside its region and to the
	/// neighbours, publishes RegionConverted when a majority changes, and names
	/// nameless religions when a language type was given.
	class VAELEN_SIM_API ReligionSystem final : public ISystem
	{
	public:
		ReligionSystem(World& InWorld, WorldGen::WorldSetup InSetup, PopulationTypes InPopulation,
					   ReligionTypes InTypes, ReligionRules InRules) noexcept
			: Owner(&InWorld), Setup(InSetup), Population(InPopulation), Types(InTypes), Rules(InRules)
		{
		}
		/// Optional: name religions in their founding culture's language.
		void NameWith(LanguageTypes InLanguages) noexcept
		{
			Languages = InLanguages;
			HasLanguages = true;
		}
		const char* GetName() const noexcept override { return "Religions"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Population"}; }
		void Tick(TickContext& Context) override;

		/// Queues a founding in a region caused by an event; the first request
		/// per region wins until the yearly tick. False when refused (no faith
		/// state, queue full, duplicate region, null cause).
		bool RequestFounding(uint32 Region, PersistentId Cause, FoundingKind Kind = FoundingKind::Requested);

	private:
		World* Owner;
		WorldGen::WorldSetup Setup;
		PopulationTypes Population;
		ReligionTypes Types;
		ReligionRules Rules;
		LanguageTypes Languages;
		bool HasLanguages = false;
		Hash64 GraphDigest = 0;		 ///< derived cache, not state
		WorldGen::RegionGraph Graph; ///< derived cache, not state
	};

	/// Listener: migration waves carry believers with them; culture splits and
	/// era openings request foundings according to the rules.
	class VAELEN_SIM_API FaithListener final : public IEventListener
	{
	public:
		FaithListener(World& InWorld, WorldGen::WorldSetup InSetup, PopulationTypes InPopulation, ReligionTypes InTypes,
					  ReligionRules InRules, ReligionSystem* InSystem) noexcept
			: Owner(&InWorld), Setup(InSetup), Population(InPopulation), Types(InTypes), Rules(InRules),
			  System(InSystem)
		{
		}
		/// Subscribes to MigrationWave, CultureSplit and EraOpened.
		void Listen(EventBus& Bus);
		const char* GetListenerName() const noexcept override { return "FaithListener"; }
		void OnEvent(const Event& E) override;

	private:
		World* Owner;
		WorldGen::WorldSetup Setup;
		PopulationTypes Population;
		ReligionTypes Types;
		ReligionRules Rules;
		ReligionSystem* System;
	};

	struct FaithStats
	{
		uint32 Religions = 0;
		uint32 Schisms = 0;
		uint64 Adherents = 0;
		uint64 People = 0;
		uint32 RegionsWithFaith = 0; ///< regions with any believer
		uint32 ConvertedRegions = 0; ///< regions with a majority faith
		uint32 Regions = 0;
		uint32 LargestReligion = 0; ///< index
		uint64 LargestAdherents = 0;
		uint32 Pending = 0;
		uint32 Refused = 0;
	};
	VAELEN_SIM_API FaithStats MeasureFaith(const World& W, const PopulationTypes& Population,
										   const ReligionTypes& Types);

	/// Region picture with the majority faith's glyph ('.' settled without a
	/// faith, ' ' unsettled land, '~' sea).
	VAELEN_SIM_API void ExportFaithAscii(const World& W, const WorldGen::WorldSetup& Setup,
										 const PopulationTypes& Population, const ReligionTypes& Types, uint32 Columns,
										 std::string& Out);
} // namespace Vaelen::History
