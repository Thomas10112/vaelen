// VAELEN - VaelenRun
// Phase 14 task 14.03: one wiring of a played AELVOR.
//
// Until this file the world was wired in three places - the headless atlas,
// the game module's actor, the presentation actor - by copying a constructor,
// and ADR-0135 is what that cost. This is the fourth wiring and the one a
// host is meant to use: the Atlas wiring VERBATIM (fifteen types in Atlas
// order, fifteen systems; not Phases 07-09, because the Atlas does not wire
// them and the frozen frame digest is of a world without them), behind
// Options that ADD and never reorder. Colony adds the colony after Polity, as
// the Atlas does. Play adds the six Phase 10 type sets and three systems after
// those, so a world nobody plays carries no trace of a player and its digests
// are the digests it had. Lively adds Phase 12's living, repute and fame after
// those again, and implies Play.
//
// What this class is NOT: a save, a scene, or a place for a rule. It owns a
// World and the systems that run it, it can take one person up and let them
// go, and it can be measured. Time moves through Day(), and only a Door
// (Door.h) should call it, because a day turned is an input and the Door is
// what records inputs (ADR-0138).
//
// STATUS: PROTOTYPE (Phase 14) - Tests/Run/Test_Aelvor.cpp, Tests/Run/Test_Door.cpp; the fourth wiring
// Tools/check_world_wiring.py compares
#pragma once

#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Gameplay/Living.h"
#include "Vaelen/Gameplay/Repute.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Intent.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerHistory.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Player/Stream.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Run/Attention.h"
#include "Vaelen/Run/RunApi.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"
#include "Vaelen/View/Take.h"

#include <memory>
#include <vector>
#include <string>

namespace Vaelen::Run
{
	/// The seed AELVOR has had since Tools/Atlas: the letters of its name.
	inline constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// What a Run is asked to be. The defaults are the Atlas defaults, which
	/// is what makes the frozen digests of ADR-0135 the digests of this class
	/// with nothing set.
	struct Options
	{
		uint32 Size = 128;		 ///< map side in tiles
		uint32 PreHistory = 300; ///< years before detail is requested
		uint32 Years = 120;		 ///< years run after it, before play
		uint64 Seed = AelvorSeed;
		bool Colony = false; ///< ColonyTypes and MiningSystem after Polity, as the Atlas
		bool Play = false;	 ///< the six Phase 10 type sets and three systems after those
		bool Lively = false; ///< Phase 12's living, repute, fame and judgement; implies Play
		/// Phase 15: what is detailed is decided every DAY rather than every
		/// YEAR, by a DetailSystem beside the bridge (LodRules::DecideElsewhere).
		///
		/// It is an option and not the new behaviour because it changes what is
		/// detailed WHEN, and everything a detailed region does follows from
		/// that: a world with this set is a different world from the same seed,
		/// and every frozen digest this project has belongs to a world without
		/// it. Turning it on for the played world is 15.10's business, together
		/// with a month replayed against it.
		bool Stream = false;
	};

	/// The component type sets of the wiring, for a test or a subsystem that
	/// measures. Handles only: nothing here can move the world.
	struct Wired
	{
		Population::PersonTypes Persons;
		Population::FamilyTypes Families;
		Population::NeedTypes Needs;
		Population::TraitTypes Traits;
		Population::LodTypes Lod;
		Society::OrganizationTypes Organizations;
		Society::StandingTypes Standing;
		Society::NormTypes Norms;
		Society::BondageTypes Bondage;
		Economy::EconomyTypes Economy_;
		Economy::ProductionTypes Production;
		Economy::MarketTypes Markets;
		Economy::TradeTypes Trade;
		Economy::WealthTypes Wealth;
		Politics::PolityTypes Polities;
		Colony::ColonyTypes Pit;	///< only with Options::Colony
		Player::PlayerTypes Played; ///< only with Options::Play, and the five below
		Player::StartTypes Start;
		Player::HourTypes Hour;
		Player::OrderTypes Order;
		Player::RegardTypes Regard;
		Player::LifeChronicleTypes LifeRecords;
		Gameplay::LivingTypes Living; ///< only with Options::Lively, and the two below
		Gameplay::ReputeTypes Repute;
		Gameplay::FameTypes Fame;
	};

	class VAELEN_RUN_API Aelvor
	{
	public:
		explicit Aelvor(const Options& In);
		~Aelvor();
		Aelvor(const Aelvor&) = delete;
		Aelvor& operator=(const Aelvor&) = delete;

		/// Generates the map, runs the pre-history, requests detail on the
		/// busiest region (the busiest with ore under it when a colony is
		/// asked for), founds the colony, marks the region lively, and runs
		/// the years - the Atlas sequence, in the Atlas order, because the
		/// frozen digests depend on detail being asked for after exactly
		/// PreHistory years. False when the map cannot be generated or when
		/// called twice.
		bool Begin();
		bool Begun() const noexcept { return Run_.Begun; }

		World& Instance() noexcept;
		const World& Instance() const noexcept;
		SimTick Now() const noexcept;
		const Options& Given() const noexcept { return Given_; }
		const Wired& Handles() const noexcept;
		const History::PreHistoryTypes& Ages() const noexcept;
		/// The region simulated person by person, 0 before Begin().
		uint32 Detail() const noexcept { return Run_.Detail; }
		/// The region the colony was founded on, 0 when none was.
		uint32 Founded() const noexcept { return Run_.Dug; }

		/// Told where the host is looking. 15.06 remembers it and no more; the
		/// warden of 15.07 is what turns attention into detail requests, and it
		/// will do it from here so that a replay and a live host go through one
		/// path. Kept on the Run and not in the world: a replay rebuilds it by
		/// applying the same looks in the same order, so nothing here enters a
		/// digest.
		/// 15.07: and, with Options::Stream, the warden runs - a deterministic
		/// function from attention to detail REQUESTS. It writes requests and
		/// never promotions, so ADR-0037 holds by construction rather than by
		/// anybody remembering it; the bridge is still the only thing that
		/// promotes, and it still decides on its own cadence.
		void LookAt(const Attention& At);
		/// The last attention this Run was told about.
		const Attention& Attending() const noexcept { return Run_.Eyes; }
		/// What the warden currently asks to have detailed, ascending. Empty
		/// without Options::Stream, and host-side either way.
		const std::vector<uint16>& Watching() const noexcept { return Run_.Watched; }

		/// What a stream of this world is headed with (Stream.h).
		Player::StreamHeader Header() const;
		/// What the view needs to be told to look at this world.
		View::ViewSources Sources() const;

		// ── The played person. Every call is a no-op returning 0/false/NoPlayer
		// on a Run without Play, so a host can be written once.

		/// BeginEnslaved (Start.h) then BeginOrders: somebody bound to be, or
		/// whoever fits the rules. 0 when the world offers nobody, when
		/// somebody is already played (Release first), or before Begin().
		uint32 TakeUp(const Player::StartRules& Rules);
		/// The mark comes off and every Phase 10 component with it, in the
		/// order the gates settled. False when nobody was played.
		bool Release();
		uint32 Played() const;
		bool PlayedAlive() const;
		/// Player::Submit on the played queue. The door's answer only: the
		/// world's refusals arrive later, in Orders(). Nothing is stamped: a
		/// command with Issued left at 0 is Stale (OrderRules::StaleAfter) by
		/// the time the day reads it - Door::Mean stamps Now() for the caller.
		Player::Refusal Submit(const Player::PlayerCommand& C);
		/// TickMany(24), and nothing before Begin(). A Door records it; nothing
		/// else should call it.
		uint64 Day();

		// ── Measures.
		Hash64 StateDigest() const;
		Hash64 LogDigest() const;
		/// The played life in words (ExportLife), empty without Play.
		std::string Life() const;
		Player::OrderStats Orders() const;
		Player::HourStats Hours() const;

		struct Kernel; ///< the wiring, in Aelvor.cpp where the wiring guard reads it

		/// EVERYTHING THE RUN KNOWS THAT THE WORLD DOES NOT, in one struct, and
		/// the reason it is one struct rather than six members is the whole of
		/// task 16.05: these live OUTSIDE the image, so a field added here and
		/// forgotten in the RUN section is a world that restores and then
		/// quietly diverges. Making them a single object means the thing that
		/// is saved IS the thing that is kept - a new field is carried by
		/// construction, not by remembering.
		///
		/// Measured, because the divergence is not visible where you would look
		/// for it: a restored world with Watched empty has the SAME state digest
		/// as its source, and goes on having it for as many LOOKS as you care to
		/// make. With Options::Stream the warden runs on a DAY TURN, so it takes
		/// a day turn for the absence to bite - 431f81069619ca49 against
		/// 35d4827934886778 at 32 tiles. A test that only looked would pass
		/// against the unfixed code, which is why Run.Checkpoint carries both
		/// arms.
		///
		/// Ways_ IS DELIBERATELY NOT HERE, and the reason is asserted rather
		/// than assumed: WorldMap::Reset does ++Replaced (WorldMap.cpp:53, and
		/// the Serialize path at :109), and RegionGraphCache keys on
		/// HashCombine(HashUInt64(Map.Revision()), ...) (Regions.cpp:421), so a
		/// load invalidates it by construction. Saving it would be saving a
		/// cache that the act of loading has already thrown away.
		struct RunState
		{
			bool Begun = false;
			uint32 Detail = 0;
			uint32 Dug = 0;
			/// Written and never read, and it rides along anyway: it costs
			/// twelve bytes, and the next person to read it should find it
			/// right rather than find it stale.
			Attention Eyes;
			std::vector<uint16> Near;
			std::vector<uint16> Watched;
		};

		/// What this run would need to be restored into another Aelvor.
		VAELEN_RUN_API RunState GetRunState() const;
		/// Replaces it. False when the state is not one this wiring can hold -
		/// a region past the end of the map, or a detail request the world has
		/// no region for.
		VAELEN_RUN_API bool SetRunState(const RunState& In);

	private:
		Options Given_;
		std::unique_ptr<Kernel> K;
		RunState Run_;
		/// ADR-0139: after a taking, ask for the neighbours of the played
		/// person's region, so that a Move has somewhere the world will let it
		/// go. See Aelvor.cpp for why it lives here and not in a host.
		void NearDetail(uint32 Who);

		/// Derived, and rebuilt only when the map it came from is replaced.
		WorldGen::RegionGraphCache Ways_;
		/// The warden's decision, out of LookAt so it can be read on its own.
		void Reside(const Attention& At);
	};
} // namespace Vaelen::Run
