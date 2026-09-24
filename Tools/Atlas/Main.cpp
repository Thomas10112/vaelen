// VAELEN - Tools/Atlas
// Phase 13 task 13.07a: AELVOR without an engine.
//
// This is the atlas actor's job, minus Unreal. It builds a world with the same
// systems, generates it, runs its centuries, takes a WorldView and a MapView,
// and writes both out as JSON. Nothing here draws anything: it produces the
// numbers, and whatever draws them is somebody else's problem - which is the
// whole point, and is only possible because a view holds no pointer back into
// the world (Vaelen/View/Frame.h).
//
// Why it exists. Thirteen phases of simulation have been seen exactly once, in
// an editor, on a machine that fights the editor. This runs anywhere a compiler
// runs, it is compiled by the nine CI jobs, and what it writes can be drawn by
// anything - which makes the loop between changing the world and looking at it
// short enough to be used every day.
//
// NOT kernel: this file is a tool. It reads the clock and writes a file, and
// neither is allowed inside Source/Vaelen* (Tools/check_kernel_purity.py).
//
// STATUS: PROTOTYPE (Phase 13) - run by the CTest entries Atlas.Runs and Atlas.Output
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Core/Log.h"
#include "Vaelen/Economy/Markets.h"
#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Economy/Trade.h"
#include "Vaelen/Economy/Wealth.h"
#include "Vaelen/Player/Stream.h"
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Run/Aelvor.h"
#include "StdioCheckpointStore.h"
#include "Vaelen/Run/Checkpoint.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Core/Version.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Causality.h"
#include "Vaelen/Sim/EventTypeNames.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/SocietyHistory.h"
#include "Vaelen/Society/Standing.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Panel.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Net.h"
#include "Vaelen/View/Take.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Colony;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Politics;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::View;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogAtlas);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// Everything the kernel needs to run a world, in the order the Phase 06
	/// gate settled (ADR-0055). This is deliberately the SAME wiring as
	/// AVaelenAtlasActor's, with bondage added: the player of VAELEN starts
	/// owned, and a view that cannot say who is bound is missing the one number
	/// the premise of the game turns on.
	struct KernelRun
	{
		KernelRun(uint64 InSeed, bool InWithColony, bool InWithChronicle)
			: WithColony(InWithColony), WithChronicle(InWithChronicle), Instance(Config(InSeed)),
			  Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Markets = MarketTypes::Declare(Instance);
			Trade = TradeTypes::Declare(Instance);
			Wealth = WealthTypes::Declare(Instance);
			Polities = PolityTypes::Declare(Instance);
			// Declared only when asked. ColonyTypes::Declare also declares
			// Economy::RegionMined, so a world that is not told to have a colony
			// carries no trace of one and its digests are the digests it had
			// before this file knew what a colony was.
			if (WithColony)
			{
				Pit = ColonyTypes::Declare(Instance);
			}
			// The chronicle turns events into RecordInfo entities. Declared only
			// when asked, so a world nobody asked to remember carries no memory
			// and its digests are the digests it had before.
			if (WithChronicle)
			{
				PersonRecords = PersonChronicleTypes::Declare(Instance);
				SocietyRecords = SocietyChronicleTypes::Declare(Instance);
				EconomyRecords = EconomyChronicleTypes::Declare(Instance);
			}

			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Trade,
												  ProductionRules{}, MarketRules{}, TradeRules{});
			Purses = std::make_unique<WealthSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Markets, Norms,
													Wealth, WealthRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage,
													BondageRules{});
			Rulers =
				std::make_unique<PolitySystem>(Instance, Ages.Types(), Persons, Organizations, Polities, PolityRules{});
			if (WithColony)
			{
				Rock = std::make_unique<MiningSystem>(Instance, Ages.Types(), Persons, Families, Economy_, Pit,
													  MiningRules{});
				Rock->ObserveTraits(Traits.Traits);
			}
			if (WithChronicle)
			{
				// Three listeners, one describer. The economy's text speaks for
				// the society under it and the persons under that (06.07): the
				// topmost layer describes every layer below, or a world's
				// chronicle loses the words of its middle. Politics would be the
				// layer above, and needs seven more type sets than this tool
				// declares - so the roads and the towns get their own sentences
				// and a polity's rise gets the plainer one.
				Society_ = SocietyContext{Persons, Families, Organizations};
				Trades = EconomyContext{Persons, Families, Trade, Markets, MarketRules{}, &Society_};
				PersonScribe = std::make_unique<PersonChronicle>(Instance, Ages.Types(), Persons, Families,
																 PersonRecords, PersonChronicleRules{});
				SocietyScribe = std::make_unique<SocietyChronicle>(Instance, Ages.Types(), Society_, SocietyRecords,
																   SocietyChronicleRules{});
				EconomyScribe = std::make_unique<EconomyChronicle>(Instance, Ages.Types(), Trades, EconomyRecords,
																   EconomyChronicleRules{});
			}

			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Orgs->RunAfter("Needs");
			Ranks->RunAfter("Wealth");
			Ranks->ObserveWealth(Wealth.Wealth);
			Stocks->RunAfter("Lod");
			Stocks->ObserveHeirs(Wealth.Heir);
			Harvest->ObserveTraits(Traits.Traits);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Rulers->RunAfter("Lod");

			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Fair.get());
			Instance.Systems().Add(Roads.get());
			Instance.Systems().Add(Purses.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Rulers.get());
			if (Rock != nullptr)
			{
				Instance.Systems().Add(Rock.get());
			}
			if (WithChronicle)
			{
				PersonScribe->Attach();
				SocietyScribe->Attach();
				EconomyScribe->Attach();
			}
			Instance.Build();
		}

		static WorldConfig Config(uint64 InSeed)
		{
			WorldConfig C;
			C.Seed = InSeed;
			return C;
		}

		ViewSources Sources() const
		{
			ViewSources S;
			S.Types = Ages.Types();
			S.Persons = Persons;
			S.HasBondage = true;
			S.Bondage = Bondage;
			S.HasTrade = true;
			S.Trade = Trade;
			S.HasColony = WithColony;
			S.Colony_ = Pit;
			return S;
		}

		/// The region with the most people: the one worth simulating person by
		/// person. Ties go to the lower index, so the choice does not ride on
		/// pool order.
		uint32 Busiest() const
		{
			uint32 Best = 0;
			uint32 People = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && (P->Total > People || (P->Total == People && R.Index < Best)))
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}

		bool WithColony = false;
		bool WithChronicle = false;
		/// The peopled region with the most people that has ORE under it.
		///
		/// A colony is people put on rock. Put on ground with no seam it lifts
		/// nothing and puts nobody on anything, which is exactly what the first
		/// run of --colony reported: one colony, region 42, zero hands, zero
		/// lifted. The predicate is copied from Colony/Mining.cpp and
		/// Player/Start.cpp, which each keep their own; a tool duplicating a
		/// five-line rule is cheaper than a public API nobody else wants.
		uint32 BusiestWithOre() const
		{
			std::vector<uint32> Ore;
			Instance.Components()
				.GetPool(Ages.Types().World.DepositTypes_.Deposit)
				.ForEach(
					[&](EntityHandle, const WorldGen::DepositInfo& D)
					{
						const bool Seam = D.Kind == static_cast<uint32>(WorldGen::ResourceKind::IronOre) ||
										  D.Kind == static_cast<uint32>(WorldGen::ResourceKind::CopperOre);
						if (D.Region != 0 && Seam)
						{
							Ore.push_back(D.Region);
						}
					});
			std::sort(Ore.begin(), Ore.end());
			Ore.erase(std::unique(Ore.begin(), Ore.end()), Ore.end());
			if (Ore.empty())
			{
				return 0;
			}
			uint32 Best = 0;
			uint32 People = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (!std::binary_search(Ore.begin(), Ore.end(), R.Index))
						{
							return;
						}
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && (P->Total > People || (P->Total == People && R.Index < Best)))
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}

		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		NeedTypes Needs;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		EconomyTypes Economy_;
		ProductionTypes Production;
		MarketTypes Markets;
		TradeTypes Trade;
		WealthTypes Wealth;
		PolityTypes Polities;
		ColonyTypes Pit;
		PersonChronicleTypes PersonRecords;
		SocietyChronicleTypes SocietyRecords;
		EconomyChronicleTypes EconomyRecords;
		// Trades holds a pointer to Society_, so the two must not be reordered
		// and this struct must not be copied. It is neither.
		SocietyContext Society_;
		EconomyContext Trades;
		std::unique_ptr<PersonChronicle> PersonScribe;
		std::unique_ptr<SocietyChronicle> SocietyScribe;
		std::unique_ptr<EconomyChronicle> EconomyScribe;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		std::unique_ptr<MarketSystem> Fair;
		std::unique_ptr<TradeSystem> Roads;
		std::unique_ptr<WealthSystem> Purses;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<PolitySystem> Rulers;
		std::unique_ptr<MiningSystem> Rock;
	};

	// ── JSON, written by hand ────────────────────────────────────────────────
	// A dependency would have to be vendored, audited and kept; what is needed
	// here is numbers, arrays and a handful of strings none of which come from
	// outside this program. Written into one string and flushed once, because
	// a quarter of a million fprintf calls is most of the tool's runtime.
	struct Json
	{
		std::string Text;

		void Reserve(usize Bytes) { Text.reserve(Bytes); }
		void Put(const char* S) { Text.append(S); }
		void Number(int64 V)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%lld", static_cast<long long>(V));
			Text.append(Buffer);
		}
		void Unsigned(uint64 V)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(V));
			Text.append(Buffer);
		}
		void Hex(uint64 V)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "\"0x%016llx\"", static_cast<unsigned long long>(V));
			Text.append(Buffer);
		}
		/// A JSON string. The chronicle is written by the world, so it can hold
		/// anything the namer of a place put in it: quotes, backslashes and any
		/// byte of UTF-8. Escaped by the rules rather than by hope.
		void Str(const std::string& Words)
		{
			Text.push_back('"');
			for (const char C : Words)
			{
				const unsigned char U = static_cast<unsigned char>(C);
				if (C == '"' || C == '\\')
				{
					Text.push_back('\\');
					Text.push_back(C);
				}
				else if (U < 0x20)
				{
					char Buffer[8];
					std::snprintf(Buffer, sizeof(Buffer), "\\u%04x", static_cast<unsigned>(U));
					Text.append(Buffer);
				}
				else
				{
					Text.push_back(C);
				}
			}
			Text.push_back('"');
		}

		void Field(const char* Name, uint64 V)
		{
			Text.push_back('"');
			Text.append(Name);
			Text.append("\":");
			Unsigned(V);
		}
		void Signed(const char* Name, int64 V)
		{
			Text.push_back('"');
			Text.append(Name);
			Text.append("\":");
			Number(V);
		}
	};

	/// One moment of the world, kept so the page can be scrubbed through it. The
	/// ground is NOT here: it does not change, and a copy of 65536 tiles a frame
	/// would be the whole file.
	struct Kept
	{
		WorldView Frame;
		NetView Net;
	};

	struct Options
	{
		uint32 Size = 128;
		uint32 PreHistory = 300;
		uint32 Years = 120;
		uint64 Seed = AelvorSeed;
		std::string Out = "aelvor.json";
		bool Tiles = true;
		bool Colony = false;
		bool Chronicle = false;
		bool Why = false;
		uint32 Every = 0;	///< years between kept frames; 0 = keep only the last
		std::string Replay; ///< 14.03: a stream to replay into a fresh played Run; the world is the stream's
		bool Empty = false; ///< 14.03: the empty play - the Play wiring, nobody taken up, no stream
		bool Panel = false; ///< 14.06: print the first screen of the world the replay came to
		/// 14.10: the host's StartRules::WantBound. The rules are the host's
		/// configuration and deliberately NOT in the stream (Door.h), so a
		/// replay must be told which ones the stream was recorded under. The
		/// engine host takes whoever the world offers (VaelenWorldSubsystem.cpp
		/// sets 0); the kernel's own default is 1, and so is this one.
		uint32 WantBound = 1;
		/// 14.10: write a stream of a month played by nobody, to this path.
		std::string Stand;
		/// 15.10's headless half: a WALK, written with the streaming cadence on
		/// and a Looked record for every day of it.
		std::string Walk;
		/// Run::Options::Stream for a replay. NOT in the stream, for the same
		/// reason --want-bound is not: the cadence a world decides its detail on
		/// is the host's configuration, and a replay is told it rather than
		/// reading it. Replaying a walk without it is replaying it into a world
		/// that pays attention on a different schedule, and the digests say so.
		bool Stream = false;
		/// 15.10's gate: a walk recorded elsewhere, replayed here and asked the
		/// six clauses of the phase gate. The streaming cadence is forced on
		/// for it - RunGate says why.
		std::string Gate;
		/// 16.01: write the golden corpus to this directory.
		std::string Golden;
		/// 17.01: write the container corpus to this directory.
		std::string Containers;
		/// 17.05: the cause census over a container read from this file. The
		/// world is adopted from the container's own HOST section, so nothing
		/// about its wiring has to be spelled out on the command line and
		/// nothing can be spelled out wrongly.
		std::string Causes;
		/// 17.05: the same census over a world GENERATED from --size and the
		/// rest, for the figure over a fresh AELVOR the roadmap records.
		bool Census = false;
		/// 17.06: describe a container WITHOUT generating or adopting a world.
		std::string Inspect;
		/// 17.06: list a directory of containers through the host-side store,
		/// from a process that wrote none of them.
		std::string InspectDir;
		/// The four digests the host printed, as `state %016llx, log %016llx,
		/// life %016llx, panel %016llx` - the tail of the line
		/// Vaelen.Stream.Write logs. Given, the gate JUDGES clause (b)'s other
		/// half; withheld, it says it is not judging it. It does not guess.
		std::string Expect;
		/// 16.04: build a checkpoint of the wired run and print what is in it.
		/// It writes no file - the container is bytes, and where they go is the
		/// host's business, not this tool's.
		bool Save = false;
		/// With --save, print each section's share of the container.
		bool Sections = false;
		/// Phase 16 gate clause (k): write the container to a FILE, and load one
		/// back in a SECOND PROCESS. A save that only ever lives in one
		/// process's memory has not been proven to survive the one journey it
		/// exists to make.
		std::string SaveTo;
		std::string LoadFrom;
		uint32 ThenDays = 0u;
		/// 16.12(b): a recorded walk, saved at K seeded points along its day
		/// turns, and every save asked the questions a save has to answer.
		std::string SaveFuzz;
		uint32 Points = 4u;
		/// A CONTROL, and the tool refuses to call it a pass. Restores without
		/// the RUN section; the run must then report mismatches.
		bool WithholdRun = false;
		/// A CONTROL. Flips one byte of the container before reading it back;
		/// the read must REFUSE, by a section digest and not by luck.
		long CorruptByte = -1;
		/// 16.12(c): the start window. It is the HOST's configuration exactly
		/// as --want-bound is, so a replay must be given the same one: a stream
		/// recorded from an eighty-five-year-old replayed with the default 16
		/// to 40 takes up somebody else entirely, and every taking after that
		/// is a different person's.
		uint32 FromAge = 16u;
		uint32 ToAge = 40u;
		/// 16.12(c): write a walk in which the played person DIES. Refuses to
		/// write one in which nobody did, for the reason --walk refuses a walk
		/// that misses a clause.
		std::string DeathWalk;
		uint32 DeathDays = 1200u;
		/// 16.12(b) fix: the state digest the uninterrupted walk MUST reach.
		///
		/// Straight.Wrong catches a wiring that makes the world offer different
		/// PEOPLE, but not one that changes what the world CONTAINS while the
		/// person indices happen to survive - which is exactly what dropping
		/// --lively does. Measured: the death walk without it replays with
		/// 0 wrong and lands on a973fafaa72d6be5 instead of 18aec14e39a68a8c,
		/// in containers of 2 MB instead of 208 MB.
		///
		/// A .stream file carries no digest, so the only external reference is
		/// one the caller pins - the same way Replay.Lived pins four of them.
		/// Without it this tool can only agree with itself.
		std::string ExpectState;
		/// TOLD, NOT READ, exactly as Options::Stream is. Player::StreamHeader
		/// carries Seed, Size, PreHistory, Years and a version and NO wiring
		/// bits, so a walk recorded in a lively world and replayed without this
		/// flag is replayed in a DIFFERENT world - which is the defect 16.10
		/// and 16.11 are about, met again here from the tool's side. A save
		/// carries its Options in a HOST section; a `.stream` file cannot.
		bool Lively = false;
	};

	/// Runs Years years, keeping a frame every Opt.Every of them. Taking a view
	/// is const: the world does not know it happened, so a run with --every
	/// simulates the same world as a run without it - which the digests check.
	void Step(KernelRun& Run, std::vector<Kept>& Timeline, const Options& Opt, uint32 Years)
	{
		if (Years == 0)
		{
			return;
		}
		if (Opt.Every == 0)
		{
			Run.Ages.Run(Years);
			return;
		}
		for (uint32 Done = 0; Done < Years;)
		{
			const uint32 Slice = Opt.Every < (Years - Done) ? Opt.Every : (Years - Done);
			Run.Ages.Run(Slice);
			Done += Slice;
			Kept K;
			TakeView(Run.Instance, Run.Sources(), K.Frame);
			TakeNetView(Run.Instance, Run.Sources(), K.Net);
			Timeline.push_back(std::move(K));
		}
	}

	bool ParseUnsigned(const char* Text, uint64& Out)
	{
		if (Text == nullptr || *Text == '\0')
		{
			return false;
		}
		char* End = nullptr;
		const int Base = (Text[0] == '0' && (Text[1] == 'x' || Text[1] == 'X')) ? 16 : 10;
		const unsigned long long Value = std::strtoull(Text, &End, Base);
		if (End == nullptr || *End != '\0')
		{
			return false;
		}
		Out = static_cast<uint64>(Value);
		return true;
	}

	void Usage()
	{
		std::fprintf(stderr,
					 "VaelenAtlas - generates AELVOR and writes what can be drawn of it.\n"
					 "  --size N        map side in tiles (32..512, default 128)\n"
					 "  --prehistory N  years run by the pre-history (default 300)\n"
					 "  --years N       years run after it (default 120)\n"
					 "  --seed V        decimal or 0x hex (default 0x41454c564f52)\n"
					 "  --out PATH      where to write the JSON (default aelvor.json)\n"
					 "  --no-tiles      write the regions only, not the ground\n"
					 "  --colony        found a mining colony on the busiest region\n"
					 "  --every N       also keep a frame every N years, for a timeline\n"
					 "  --chronicle     remember what happened, and write it out in words\n"
					 "  --why           and why: the cause of each thing, back to its root\n"
					 "  --replay FILE   replay a vaelen-stream into a fresh played Run and write what it came to\n"
					 "  --empty         the empty play: the Play wiring with nobody taken up, no stream\n"
					 "  --panel         with --replay or --empty: print the first screen it came to (14.06)\n"
					 "  --want-bound N  StartRules::WantBound for a replay (0 or 1, default 1; the engine host "
					 "uses 0)\n"
					 "  --stand FILE    write a stand-in stream: thirty days played by nobody (14.10)\n"
					 "  --walk FILE     write a stand-in WALK: looks, takings and the streaming cadence (15.10)\n"
					 "  --golden DIR    write the golden save corpus of 16.01 to this directory\n"
					 "  --containers DIR  write the container corpus of 17.01 to this directory\n"
					 "  --causes FILE   17.05: the cause census over a container, per event type\n"
					 "  --census        the same over a world generated from --size and the rest\n"
					 "  --inspect FILE  17.06: header, sections, host wiring, run shape and stream of a\n"
					 "                  container, without generating a world\n"
					 "  --inspect-dir DIR  list every container in a directory through the store\n"
					 "  --gate FILE     replay a walk and report the six clauses of the 15.10 gate\n"
					 "  --expect \"...\"  with --gate: the four digests the host printed, judged rather than "
					 "printed\n");
	}

	bool ParseOptions(int Argc, char** Argv, Options& Out)
	{
		for (int I = 1; I < Argc; ++I)
		{
			const char* Arg = Argv[I];
			const bool HasValue = (I + 1) < Argc;
			uint64 Value = 0;
			if (std::strcmp(Arg, "--no-tiles") == 0)
			{
				Out.Tiles = false;
			}
			else if (std::strcmp(Arg, "--colony") == 0)
			{
				Out.Colony = true;
			}
			else if (std::strcmp(Arg, "--chronicle") == 0)
			{
				Out.Chronicle = true;
			}
			else if (std::strcmp(Arg, "--why") == 0)
			{
				// The why is read out of the chronicle's records, so asking for
				// one asks for the other.
				Out.Chronicle = true;
				Out.Why = true;
			}
			else if (std::strcmp(Arg, "--help") == 0 || std::strcmp(Arg, "-h") == 0)
			{
				return false;
			}
			else if (std::strcmp(Arg, "--out") == 0 && HasValue)
			{
				Out.Out = Argv[++I];
			}
			else if (std::strcmp(Arg, "--replay") == 0 && HasValue)
			{
				Out.Replay = Argv[++I];
			}
			else if (std::strcmp(Arg, "--empty") == 0)
			{
				Out.Empty = true;
			}
			else if (std::strcmp(Arg, "--panel") == 0)
			{
				Out.Panel = true;
			}
			else if (std::strcmp(Arg, "--walk") == 0 && HasValue)
			{
				Out.Walk = Argv[++I];
			}
			else if (std::strcmp(Arg, "--stream") == 0)
			{
				Out.Stream = true;
			}
			else if (std::strcmp(Arg, "--stand") == 0 && HasValue)
			{
				Out.Stand = Argv[++I];
			}
			else if (std::strcmp(Arg, "--gate") == 0 && HasValue)
			{
				Out.Gate = Argv[++I];
			}
			else if (std::strcmp(Arg, "--expect") == 0 && HasValue)
			{
				Out.Expect = Argv[++I];
			}
			else if (std::strcmp(Arg, "--save") == 0)
			{
				Out.Save = true;
			}
			else if (std::strcmp(Arg, "--savefuzz") == 0 && HasValue)
			{
				Out.SaveFuzz = Argv[++I];
			}
			else if (std::strcmp(Arg, "--points") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				Out.Points = static_cast<uint32>(Value);
				++I;
			}
			else if (std::strcmp(Arg, "--withhold-run") == 0)
			{
				Out.WithholdRun = true;
			}
			else if (std::strcmp(Arg, "--corrupt-byte") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				Out.CorruptByte = static_cast<long>(Value);
				++I;
			}
			else if (std::strcmp(Arg, "--save-to") == 0 && HasValue)
			{
				Out.SaveTo = Argv[++I];
			}
			else if (std::strcmp(Arg, "--load-from") == 0 && HasValue)
			{
				Out.LoadFrom = Argv[++I];
			}
			else if (std::strcmp(Arg, "--then-days") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				Out.ThenDays = static_cast<uint32>(Value);
				++I;
			}
			else if (std::strcmp(Arg, "--expect-state") == 0 && HasValue)
			{
				Out.ExpectState = Argv[++I];
			}
			else if (std::strcmp(Arg, "--lively") == 0)
			{
				Out.Lively = true;
			}
			else if (std::strcmp(Arg, "--from-age") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				Out.FromAge = static_cast<uint32>(Value);
				++I;
			}
			else if (std::strcmp(Arg, "--to-age") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				Out.ToAge = static_cast<uint32>(Value);
				++I;
			}
			else if (std::strcmp(Arg, "--deathwalk") == 0 && HasValue)
			{
				Out.DeathWalk = Argv[++I];
			}
			else if (std::strcmp(Arg, "--death-days") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				Out.DeathDays = static_cast<uint32>(Value);
				++I;
			}
			else if (std::strcmp(Arg, "--sections") == 0)
			{
				Out.Sections = true;
			}
			else if (std::strcmp(Arg, "--golden") == 0 && HasValue)
			{
				Out.Golden = Argv[++I];
			}
			else if (std::strcmp(Arg, "--containers") == 0 && HasValue)
			{
				Out.Containers = Argv[++I];
			}
			else if (std::strcmp(Arg, "--causes") == 0 && HasValue)
			{
				Out.Causes = Argv[++I];
			}
			else if (std::strcmp(Arg, "--census") == 0)
			{
				Out.Census = true;
			}
			else if (std::strcmp(Arg, "--inspect") == 0 && HasValue)
			{
				Out.Inspect = Argv[++I];
			}
			else if (std::strcmp(Arg, "--inspect-dir") == 0 && HasValue)
			{
				Out.InspectDir = Argv[++I];
			}
			else if (std::strcmp(Arg, "--want-bound") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				++I;
				Out.WantBound = Value != 0 ? 1u : 0u;
			}
			else if (std::strcmp(Arg, "--size") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				++I;
				// The kernel's own limit is 4096 a side; 512 is where a JSON of
				// the ground stops being a thing a browser opens.
				Out.Size = static_cast<uint32>(Value < 32 ? 32 : (Value > 512 ? 512 : Value));
			}
			else if (std::strcmp(Arg, "--prehistory") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				++I;
				Out.PreHistory = static_cast<uint32>(Value > 100000 ? 100000 : Value);
			}
			else if (std::strcmp(Arg, "--years") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				++I;
				Out.Years = static_cast<uint32>(Value > 100000 ? 100000 : Value);
			}
			else if (std::strcmp(Arg, "--every") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				++I;
				// A frame a year on a five-hundred-year run is five hundred
				// frames of every region: legible in a browser and nowhere near
				// what the kernel can produce, so the ceiling is on the count.
				Out.Every = static_cast<uint32>(Value > 100000 ? 100000 : Value);
			}
			else if (std::strcmp(Arg, "--seed") == 0 && HasValue && ParseUnsigned(Argv[I + 1], Value))
			{
				++I;
				Out.Seed = Value;
			}
			else
			{
				std::fprintf(stderr, "unrecognised argument: %s\n", Arg);
				return false;
			}
		}
		return true;
	}
	int RunAtlas(int Argc, char** Argv);
} // namespace

int main(int Argc, char** Argv)
{
	// No sink is installed by default (Vaelen/Core/Log.h), and a tool whose run
	// cannot be read is a tool nobody can check. Installed before anything that
	// might report, removed after the last line: the sink is not owned by the
	// log and must outlive its own registration, which a stack object at the
	// top of main does by construction.
	StdioLogSink Console;
	Log::AddSink(&Console);
	const int Code = RunAtlas(Argc, Argv);
	Log::RemoveSink(&Console);
	return Code;
}

namespace
{
	/// Reads a whole file; false when it cannot be read.
	bool ReadFile(const std::string& Path, std::string& Out)
	{
		std::FILE* File = std::fopen(Path.c_str(), "rb");
		if (File == nullptr)
		{
			return false;
		}
		char Buffer[65536];
		for (;;)
		{
			const usize Got = std::fread(Buffer, 1, sizeof(Buffer), File);
			if (Got == 0)
			{
				break;
			}
			Out.append(Buffer, Got);
		}
		const bool Ok = std::ferror(File) == 0;
		std::fclose(File);
		return Ok;
	}

	/// 14.10: a month played by nobody, so the replay has something to replay.
	///
	/// The owner's stream is thirty days through the KEYS in the editor, and
	/// this is not it. What this writes is a stream of the same SHAPE - thirty
	/// recorded day turns, every one of the eight verbs at least once, one Move
	/// the world took, at least one intent the WORLD refused (not the door) -
	/// so that Tools/Atlas --replay --panel, the CTest entry and the four
	/// digests are all exercised before the engine half exists. The file it
	/// writes is named stand-in and Tests/Run/Streams/README.md says the rest.
	///
	/// Every command goes through View::Press first, exactly as a key does in
	/// 14.09, so this walks the same path the engine walks: the page answers
	/// what it foresees, and only what the page offers is handed to the door.
	/// Phase 15 task 15.10, headless half. Writes a stand-in WALK: a world with
	/// Options::Stream, somebody taken up four times over, and a camera that
	/// moves one region along the graph every day - the shape the engine half
	/// will record for real.
	///
	/// It REFUSES to write a walk that does not satisfy the gate's clauses, for
	/// the reason 14.10's --stand gives and then proved twice: a stand-in that
	/// silently misses a clause is worse than none, and the last one's refusal
	/// to write a month without a Move is how ADR-0139's defect was found.
	int RunWalk(const Options& Opt)
	{
		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Play = true;
		RO.Stream = true; // the whole point: the daily cadence and the warden
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::fprintf(stderr, "AELVOR: generation failed at %u x %u\n", RO.Size, RO.Size);
			return 1;
		}
		Player::StartRules Rules;
		Rules.WantBound = Opt.WantBound;
		Rules.FromAge = Opt.FromAge;
		Rules.ToAge = Opt.ToAge;
		// The window is the host's configuration exactly as --want-bound is,
		// and three modes plumbed it while four accepted it and threw it away.
		Rules.FromAge = Opt.FromAge;
		Rules.ToAge = Opt.ToAge;
		Vaelen::Run::Door D(A, Rules);

		const WorldGen::RegionGraph Graph = WorldGen::BuildRegionGraph(A.Instance().Map(), A.Ages().World.Regions);
		const uint32 Regions = Graph.RegionCount() > 1u ? Graph.RegionCount() : 1u;

		const uint32 Takings = 4;  // the clause ADR-0139 could not keep at one
		const uint32 PerLife = 25; // days lived before the next one is taken up
		uint32 WorstWait = 0;
		uint32 WorstPromotions = 0;
		uint32 Looks = 0;
		WorldGen::RegionGraphCache Ways;
		LifeView Life;
		uint32 Before = Population::MeasureLod(A.Instance(), A.Ages(), A.Handles().Persons, A.Handles().Lod).Promotions;
		for (uint32 Life_ = 0; Life_ < Takings; ++Life_)
		{
			// LOOK BEFORE TAKING UP, because that is the order the stream's
			// encoder writes at equal ticks and therefore the order a replay
			// applies them in: somebody looks, and then acts on what they saw.
			// Recording them the other way round made three takings of four
			// pick a different person on replay - a look changes what is
			// detailed, and a taking chooses among the detailed.
			D.Look(Vaelen::Run::Attention{1u + (Life_ * PerLife * 7u) % Regions, 1u, 2u});
			++Looks;
			if (A.Played() != 0)
			{
				A.Release();
			}
			if (D.TakeUp() == 0)
			{
				std::fprintf(stderr, "AELVOR: the world offered nobody to take up\n");
				return 1;
			}
			uint32 Waited = 0xFFFFFFFFu;
			for (uint32 Day = 0; Day < PerLife; ++Day)
			{
				// HOW LONG somebody waits for somewhere to walk, rather than
				// whether they ever wait. Two measured facts put those in
				// tension and the guard found it: NearDetail asks for the
				// neighbours as REQUESTS, the bridge answers on its next daily
				// pass (ADR-0141), and 15.08 caps that pass at ONE promotion a
				// day. A taking asks for three neighbours, so the third of them
				// cannot be detailed before the third day whatever anybody
				// wants. "Never empty" was not a clause any implementation
				// could meet; "empty for at most N days after a taking" is the
				// same guarantee stated at a rate the world can keep.
				if (Waited == 0xFFFFFFFFu)
				{
					TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
					if (Life.NearCount > 0)
					{
						Waited = Day;
					}
				}
				// A camera that keeps moving, deterministically: one step along
				// the region indices every day, so the warden keeps being asked
				// for places the world does not have yet.
				D.Day();
				// AT MOST ONE LOOK PER TICK, and the last day of a life has
				// none, so the next life's look is alone at the tick it shares
				// with that taking. The stream cannot represent look-take-look
				// at one tick: EncodeStream groups by kind at equal ticks and
				// writes every look first, so a recording that interleaves them
				// replays in a different order - which is a different world,
				// because a look changes what is detailed and a taking chooses
				// among the detailed. The round trip below is what found this;
				// it refused to write three walks before this shape.
				if (Day + 1u < PerLife)
				{
					D.Look(Vaelen::Run::Attention{1u + ((Life_ * PerLife + Day) * 7u) % Regions, 1u, 2u});
					++Looks;
				}
				const uint32 Now =
					Population::MeasureLod(A.Instance(), A.Ages(), A.Handles().Persons, A.Handles().Lod).Promotions;
				WorstPromotions = (Now - Before) > WorstPromotions ? (Now - Before) : WorstPromotions;
				Before = Now;
			}
			WorstWait = Waited > WorstWait ? (Waited == 0xFFFFFFFFu ? PerLife : Waited) : WorstWait;
		}

		const Population::AuditReport Books = Population::Audit(A.Instance(), A.Ages(), A.Handles().Persons);
		const Player::InputStream& Tape = D.Stream();
		bool Shaped = true;
		const auto Want = [&Shaped](bool Held, const char* What)
		{
			if (!Held)
			{
				std::fprintf(stderr, "AELVOR: the stand-in walk is not shaped like the gate asks: %s\n", What);
				Shaped = false;
			}
		};
		Want(Tape.Looks.size() == Looks, "a Looked record for every day walked");
		Want(Tape.Takings.size() == Takings, "four takings, which is what ADR-0139 could not keep");
		Want(Tape.Days.size() == Takings * PerLife, "a day turn for every day");
		Want(WorstWait <= 4, "somewhere to walk within four days of every taking");
		Want(WorstPromotions <= 1, "no day turn promoted twice");
		Want(Books.Disagreeing == 0, "both grains agree everywhere");
		Want(Books.FaithsOverHeads == 0, "no region where the believers outnumber the people");
		Want(Books.SlotSumWrong == 0, "every region's slots sum to its total");
		if (!Shaped)
		{
			return 1;
		}

		// THE ROUND TRIP, before a byte is written. A stand-in that is pinned
		// against its own replay proves the replay is stable and nothing else;
		// this replays the walk into a fresh world and refuses to write it
		// unless the two agree. The Phase 15 review found the first version of
		// Replay.Walked pinning exactly that way - the expectations came from
		// the replay, so a recording that diverged would have been written and
		// the test would have been green over it. Two real defects were hiding
		// under that: Attention::Most was dropped by the replay, and the replay
		// applied a same-tick taking before the look it followed.
		{
			Vaelen::Run::Aelvor Fresh(RO);
			if (!Fresh.Begin())
			{
				std::fprintf(stderr, "AELVOR: the round trip could not generate its world\n");
				return 1;
			}
			const Vaelen::Run::ReplayReport Back = Vaelen::Run::Replay(Fresh, Tape, Rules);
			const Hash64 Was = A.StateDigest();
			const Hash64 Now = Fresh.StateDigest();
			Want(Back.Refused == 0, "the walk replays into its own world at all");
			Want(Back.Wrong == 0, "no taking or answer differed on replay");
			Want(Back.Looks == Tape.Looks.size(), "every look applied on replay");
			Want(Back.Days == Tape.Days.size(), "every day turned on replay");
			if (Was != Now)
			{
				std::fprintf(stderr,
							 "AELVOR: the stand-in walk does not replay to itself: recorded %016llx, replayed "
							 "%016llx\n",
							 static_cast<unsigned long long>(Was), static_cast<unsigned long long>(Now));
				Shaped = false;
			}
			if (!Shaped)
			{
				return 1;
			}
			std::printf("walk round trip: state %016llx == %016llx, %u looks, %u days, 0 wrong\n",
						static_cast<unsigned long long>(Was), static_cast<unsigned long long>(Now), Back.Looks,
						Back.Days);
		}

		const std::string Text = Player::EncodeStream(Tape);
		std::FILE* File = std::fopen(Opt.Walk.c_str(), "wb");
		if (File == nullptr)
		{
			std::fprintf(stderr, "AELVOR: cannot write %s\n", Opt.Walk.c_str());
			return 1;
		}
		const usize Wrote = std::fwrite(Text.data(), 1, Text.size(), File);
		const bool Closed = std::fclose(File) == 0;
		if (Wrote != Text.size() || !Closed)
		{
			std::fprintf(stderr, "AELVOR: could not write all of %s\n", Opt.Walk.c_str());
			return 1;
		}
		std::printf("walk: %u looks, %u takings, %u days, worst day turn %u promotion(s), longest wait for somewhere "
					"to walk %u day(s), audit clean\n",
					Looks, Takings, static_cast<unsigned>(Tape.Days.size()), WorstPromotions, WorstWait);
		return 0;
	}

	/// 15.10's gate, asked of a walk somebody else recorded.
	///
	/// RunWalk checks the same clauses on a walk it writes ITSELF, which proves
	/// the clauses are meetable by this world and says nothing about the
	/// engine's. This reads a stream from disk, replays it into a fresh world
	/// and asks the clauses of THAT - which is the gate as the roadmap words
	/// it: "a walk, recorded on the engine machine and replayed here".
	///
	/// Clauses (c), (d) and (e) are about EVERY day of the walk and not about
	/// its end, so they are measured through Run::DayWatch. The alternative was
	/// to replay the stream a second time here, by hand, which is the one thing
	/// a replay must not have: the second one drifts, and the drift reads as a
	/// property of the walk.
	///
	/// The streaming cadence is forced on rather than taken from --stream. A
	/// walk carrying Looked records is a walk recorded with it (the engine host
	/// turns it on with `Vaelen.Play <size> <years> 1`), and a replay told
	/// otherwise would be replaying into a world that decides its detail on a
	/// different schedule - every clause below would fail for one reason that
	/// has nothing to do with the walk.
	struct Walked
	{
		/// Since != this at the end of a walk means a taking is STILL waiting.
		/// A taking on the LAST day turn leaves WorstWait at 0 and has been
		/// observed for no time at all, which is not the same as somewhere to
		/// walk arriving at once - so clause (e) refuses a walk that ends
		/// mid-wait rather than crediting it with the 0 it never earned.
		static constexpr uint32 NotWaiting = 0xFFFFFFFFu;

		const Player::InputStream* Tape = nullptr;
		WorldGen::RegionGraphCache Ways;
		LifeView Life;
		/// The world's promotion count when the walk began, NOT zero. MeasureLod
		/// counts promotions since the world was made, and Begin runs four
		/// hundred years before a single record is replayed: starting this at 0
		/// charges the first day of the walk with every promotion of the
		/// pre-history and reports a day turn that promoted twice when it
		/// promoted once. That is what the first version of this did, and
		/// --walk's own figure on the same walk is what caught it.
		uint32 Promotions = 0;
		uint32 WorstDay = 0; ///< the most any one day turn promoted (clause d)
		/// Takings are counted from the RECORDS and not by watching who is
		/// played, because a release followed by a taking under the same rules
		/// can offer the same person back: two of the stand-in walk's four
		/// takings do exactly that, and counting person changes saw two.
		usize NextTaking = 0;
		uint32 Takings = 0;
		uint32 Since = NotWaiting; ///< day turns since the last taking, while it still waits
		uint32 WorstWait = 0;	   ///< the longest any taking waited (clause e)
		uint32 Disagreeing = 0;	   ///< the worst audit gap over the walk (clause c)
		uint32 FaithsOverHeads = 0;
		uint32 SlotSumWrong = 0;
		uint32 Days = 0;
	};

	/// (e) How long after a taking somewhere to walk arrives, at ONE sample
	/// point: after every record due at this tick has been applied. Replay
	/// calls the watcher at exactly two such moments - before the first day
	/// turn and after each one - and this runs at both, because a walk's first
	/// taking happens at the first and a watcher that only saw the second would
	/// credit it with a wait of 0 whatever it waited.
	///
	/// Turned counts the day turns: the wait is in DAY TURNS since the taking,
	/// so it does not advance at the moment before the first one.
	void Waits(const Vaelen::Run::Aelvor& W, Walked& S, bool Turned)
	{
		TakeLifeView(W.Instance(), W.Sources(), S.Ways, S.Life);
		const bool Arrived = S.Life.NearCount > 0;
		if (S.Since != Walked::NotWaiting)
		{
			if (Turned)
			{
				++S.Since;
			}
			if (Arrived)
			{
				S.WorstWait = S.Since > S.WorstWait ? S.Since : S.WorstWait;
				S.Since = Walked::NotWaiting;
			}
			else
			{
				// A taking still waiting is counted at what it has waited so
				// far, so a walk that ends mid-wait is not forgiven for it.
				S.WorstWait = S.Since > S.WorstWait ? S.Since : S.WorstWait;
			}
		}
		// The takings that became due at this tick. Counted from the RECORDS -
		// see Walked::NextTaking for why not from the played person.
		bool Took = false;
		while (S.Tape != nullptr && S.NextTaking < S.Tape->Takings.size() &&
			   S.Tape->Takings[S.NextTaking].Tick <= W.Now())
		{
			++S.Takings;
			++S.NextTaking;
			Took = true;
		}
		if (Took)
		{
			// Several on one tick are one wait, and it starts here: nothing to
			// record when there is already somewhere to walk (a wait of zero
			// cannot be the worst one), counting from zero when there is not.
			S.Since = Arrived ? Walked::NotWaiting : 0u;
		}
	}

	void AtDawn(const Vaelen::Run::Aelvor& W, void* User)
	{
		Waits(W, *static_cast<Walked*>(User), false);
	}

	void EachDay(const Vaelen::Run::Aelvor& W, uint32, void* User)
	{
		Walked& S = *static_cast<Walked*>(User);
		++S.Days;

		// (d) What this one day turn promoted. MeasureLod counts promotions
		// since the world began, so the day's own figure is the difference.
		const uint32 Now =
			Population::MeasureLod(W.Instance(), W.Ages(), W.Handles().Persons, W.Handles().Lod).Promotions;
		const uint32 Today = Now >= S.Promotions ? Now - S.Promotions : 0u;
		S.WorstDay = Today > S.WorstDay ? Today : S.WorstDay;
		S.Promotions = Now;

		Waits(W, S, true);

		// (c) Both grains, every day. The audit at the END of a walk cannot see
		// a gap that opened on day three and closed on day four, and the clause
		// is about the walk and not about its last frame.
		const Population::AuditReport Books = Population::Audit(W.Instance(), W.Ages(), W.Handles().Persons);
		S.Disagreeing = Books.Disagreeing > S.Disagreeing ? Books.Disagreeing : S.Disagreeing;
		S.FaithsOverHeads = Books.FaithsOverHeads > S.FaithsOverHeads ? Books.FaithsOverHeads : S.FaithsOverHeads;
		S.SlotSumWrong = Books.SlotSumWrong > S.SlotSumWrong ? Books.SlotSumWrong : S.SlotSumWrong;
	}

	int RunGate(const Options& Opt)
	{
		std::string Text;
		if (!ReadFile(Opt.Gate, Text))
		{
			std::fprintf(stderr, "AELVOR: cannot read %s\n", Opt.Gate.c_str());
			return 1;
		}
		Player::InputStream S;
		Player::StreamReport Report;
		if (!Player::DecodeStream(Text, S, Report))
		{
			std::fprintf(stderr,
						 "AELVOR: %s is not a stream this build reads (header bad %u, version bad %u, %u lines)\n",
						 Opt.Gate.c_str(), Report.HeaderBad, Report.VersionBad, Report.Lines);
			return 1;
		}
		Vaelen::Run::Options RO;
		RO.Size = S.Header.Size;
		RO.PreHistory = S.Header.PreHistory;
		RO.Years = S.Header.Years;
		RO.Seed = S.Header.Seed;
		RO.Play = true;
		RO.Stream = true; // see the note above: forced, not asked
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::fprintf(stderr, "AELVOR: generation failed at %u x %u\n", RO.Size, RO.Size);
			return 1;
		}
		// SAID OUT LOUD, because neither of these is in the file and both
		// change what the walk replays to. A reader who does not know which
		// they were given cannot know what the clauses below are about.
		std::printf("gate: %s, want-bound %u, daily cadence forced on (a walk carrying looks was recorded with it)\n",
					Opt.Gate.c_str(), Opt.WantBound);
		Player::StartRules Rules;
		Rules.WantBound = Opt.WantBound;
		Rules.FromAge = Opt.FromAge;
		Rules.ToAge = Opt.ToAge;
		Walked Seen;
		Seen.Tape = &S;
		// The baseline, read here and not assumed: see Walked::Promotions.
		Seen.Promotions =
			Population::MeasureLod(A.Instance(), A.Ages(), A.Handles().Persons, A.Handles().Lod).Promotions;
		const Vaelen::Run::DayWatch Watching{.Begun = &AtDawn, .After = &EachDay, .User = &Seen};
		const Vaelen::Run::ReplayReport R = Vaelen::Run::Replay(A, S, Rules, Watching);

		// (a) The fence fired rather than merely existed: a pin is published
		// when a demotion the bridge wanted is refused because somebody is held
		// there, and a walk that never triggered one did not exercise 15.01.
		uint32 Pins = 0;
		const std::vector<Event>& Log = A.Instance().Log().All();
		for (usize i = 0; i < Log.size(); ++i)
		{
			Pins += Log[i].Is(Population::RegionPinnedEvent) ? 1u : 0u;
		}

		WorldGen::RegionGraphCache Ways;
		WorldView Frame;
		LifeView Life;
		ChronicleView Told;
		PanelView Page;
		TakeView(A.Instance(), A.Sources(), Frame);
		TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
		TakeChronicleView(A.Instance(), A.Sources(), Told);
		TakePanel(Frame, Life, Told, Page);
		const std::string Story = A.Life();

		// THE LINE THE OWNER COMPARES, in the shape Vaelen.Stream.Write prints
		// it, so that clause (b) is read by putting the two side by side. Not
		// checked here, because this build has no way to know what the engine
		// printed and inventing one would be inventing the answer.
		std::printf("AELVOR %u seed %012llx: played %s (person %u, region %u) %u days, %u intents, "
					"state %016llx, log %016llx, life %016llx, panel %016llx\n",
					static_cast<unsigned>(S.Header.Size), static_cast<unsigned long long>(S.Header.Seed), Life.Name,
					static_cast<unsigned>(Life.Person), static_cast<unsigned>(Life.Region), R.Days, R.Answered,
					static_cast<unsigned long long>(R.State), static_cast<unsigned long long>(R.Log),
					static_cast<unsigned long long>(HashBytes(Story.data(), Story.size())),
					static_cast<unsigned long long>(MeasurePanel(Page).Digest));

		bool Held = true;
		const auto Clause = [&Held](char Which, bool Kept, const char* What, const char* Saw)
		{
			std::printf("  (%c) %s  %s%s%s\n", Which, Kept ? "PASS" : "FAIL", What, Saw[0] == '\0' ? "" : ": ", Saw);
			Held = Held && Kept;
		};
		char Says[192];

		// A WALK WITH NO DAY TURNS IS NOT A WALK, and every clause below would
		// otherwise be kept by one: nothing disagreed, nothing promoted twice,
		// nobody waited. A gate that passes an empty file is not a gate. Said
		// once, here, rather than repeated as a guard inside each clause.
		if (R.Days == 0)
		{
			std::printf("  (-) FAIL  a walk with no day turns is not a walk: %u day turns, %u looks, %u takings\n",
						R.Days, R.Looks, static_cast<unsigned>(S.Takings.size()));
			std::printf("gate: REFUSED\n");
			return 1;
		}

		std::snprintf(Says, sizeof(Says), "%u Looked records, %u pins published while held", R.Looks, Pins);
		Clause('a', R.Looks > 0 && Pins > 0, "the stream carries looks and the fence fired", Says);

		std::snprintf(Says, sizeof(Says), "%u wrong (%u of them takings), %u records left unreached", R.Wrong,
					  R.WrongTakings, R.Left);
		Clause('b', R.Refused == 0 && R.Wrong == 0 && R.Left == 0 && R.Looks == S.Looks.size(),
			   "every record replayed as it was recorded", Says);

		// The other half of clause (b): the four digests. This build cannot
		// know what the engine printed, so it is JUDGED only when told, and
		// says plainly that it is not judged when it is not. A PASS over a
		// property nothing measured is the failure mode this whole command
		// exists to avoid.
		if (!Opt.Expect.empty())
		{
			char Four[128];
			std::snprintf(Four, sizeof(Four), "state %016llx, log %016llx, life %016llx, panel %016llx",
						  static_cast<unsigned long long>(R.State), static_cast<unsigned long long>(R.Log),
						  static_cast<unsigned long long>(HashBytes(Story.data(), Story.size())),
						  static_cast<unsigned long long>(MeasurePanel(Page).Digest));
			std::snprintf(Says, sizeof(Says), "%s", Four);
			Clause('B', Opt.Expect == Four, "the four digests are the ones the host printed", Says);
		}
		else
		{
			std::printf("  (B) NOT JUDGED  the four digests: pass --expect \"state ..., log ..., life ..., panel "
						"...\" from what Vaelen.Stream.Write printed, or compare the line above by eye\n");
		}

		std::snprintf(Says, sizeof(Says), "worst over %u days: heads %u, slots %u, faiths %u", Seen.Days,
					  Seen.Disagreeing, Seen.SlotSumWrong, Seen.FaithsOverHeads);
		Clause('c', Seen.Disagreeing == 0 && Seen.SlotSumWrong == 0 && Seen.FaithsOverHeads == 0,
			   "both grains agree on every day of the walk", Says);

		std::snprintf(Says, sizeof(Says), "worst day turn promoted %u", Seen.WorstDay);
		Clause('d', Seen.WorstDay <= 1, "no day turn promoted twice", Says);

		// A taking still waiting when the records run out has been watched for
		// no time at all. Crediting it with the 0 it is sitting on would let a
		// walk meet this clause by ending immediately after its last taking.
		const bool Finished = Seen.Since == Walked::NotWaiting;
		std::snprintf(Says, sizeof(Says), "%u takings, longest wait %u day turn(s)%s", Seen.Takings, Seen.WorstWait,
					  Finished ? "" : ", and the last one was still waiting when the walk ended");
		Clause('e', Seen.Takings >= 4 && Seen.WorstWait <= 4 && Finished,
			   "somewhere to walk within four days of each taking", Says);

		std::printf("  (f) the frozen constants of Phases 00-14 are the CI suite's business, not this command's\n");
		std::printf("gate: %s\n", Held ? "every clause this command can ask is kept" : "REFUSED");
		return Held ? 0 : 1;
	}

	/// 16.01: the corpus the migration chain will be measured against.
	///
	/// A golden is an image written by a build that no longer exists. Phase 16
	/// changes what a refusal means (16.08) and puts a container around the
	/// image (16.09); after those, no build in this repository can write an
	/// honest v3 file again, and there is nothing older than this phase for a
	/// migration to migrate. So these are written FIRST, from the tree as it
	/// stands, and never regenerated afterwards except to prove that today's
	/// build still produces them byte for byte.
	///
	/// WHY THE WORLDS ARE SO SMALL, and it is not modesty. A golden lives in
	/// git forever. Measured on this tree: AELVOR 16 with ten years of
	/// pre-history and one of history weighs 78 KB, and the SAME world with
	/// thirty and three weighs 11.4 MB - one hundred and forty-six times more
	/// for twenty more years. The event log is 75% of any image that has any
	/// history at all (76356 events at 112 bytes each), and it does not stop
	/// growing. The planning proposed a third golden of "32 tiles, 5 years,
	/// about 104 KB"; that world is 22,568,335 bytes. Two hundred and sixteen
	/// times the estimate, in a repository whose largest file is 539 KB.
	///
	/// So the corpus buys format coverage, not world coverage: every section
	/// of the image, the full type set, on two map sizes. It does NOT contain
	/// a world with a long history, and 16.09's forge exists because of that.
	struct Golden
	{
		const char* Name;
		const char* What;
		uint32 Size;
		uint32 PreHistory;
		uint32 Years;
		bool Full;
	};

	constexpr Golden Goldens[] = {
		{"bare-16.snapshot", "the wiring without the player: fifteen type sets, no Phase 10-12", 16, 10, 1, false},
		{"full-16.snapshot", "every type set this tree declares, on the smallest map that generates", 16, 10, 1, true},
		{"full-32.snapshot", "the same wiring on a different map, so a size cannot hide in the layout", 32, 10, 1,
		 true},
	};

	/// 16.04: build the container and say what is in it. No file is written -
	/// the whole point of Checkpoint.h is that it deals in bytes and leaves the
	/// question of WHERE to the host.
	int RunSave(const Options& Opt)
	{
		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Colony = Opt.Colony;
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::printf("save: the world could not be begun\n");
			return 1;
		}

		std::vector<Vaelen::uint8> Bytes;
		const Vaelen::Run::CheckpointResult R = Vaelen::Run::BuildCheckpoint(A, Bytes);
		if (R != Vaelen::Run::CheckpointResult::Ok)
		{
			std::printf("save: refused, %s\n", Vaelen::Run::CheckpointResultToString(R));
			return 1;
		}

		Vaelen::Run::CheckpointView View;
		const Vaelen::Run::CheckpointRefusal Read = Vaelen::Run::ReadCheckpoint(Bytes.data(), Bytes.size(), View);
		if (Read.Result != Vaelen::Run::CheckpointResult::Ok)
		{
			std::printf("save: written and then refused by its own reader, %s\n",
						Vaelen::Run::CheckpointResultToString(Read.Result));
			return 1;
		}

		std::printf("save: container v%u, image v%u, seed %llx, tick %llu, %zu bytes\n", View.Version, View.InnerFormat,
					static_cast<unsigned long long>(View.Seed), static_cast<unsigned long long>(View.Tick),
					Bytes.size());
		std::printf("save: log %llu events, %llu bytes - %.1f%% of the container\n",
					static_cast<unsigned long long>(View.LogEvents), static_cast<unsigned long long>(View.LogBytes),
					Bytes.empty() ? 0.0
								  : 100.0 * static_cast<double>(View.LogBytes) / static_cast<double>(Bytes.size()));
		if (Opt.Sections)
		{
			for (const Vaelen::Run::SectionEntry& E : View.Sections)
			{
				const char* Name = E.Kind == 1u	  ? "STATE"
								   : E.Kind == 2u ? "RUN"
								   : E.Kind == 3u ? "HOST"
								   : E.Kind == 4u ? "STREAM"
												  : "?";
				std::printf("save: section %-6s %10llu bytes  %5.1f%%  digest %016llx\n", Name,
							static_cast<unsigned long long>(E.Length),
							100.0 * static_cast<double>(E.Length) / static_cast<double>(Bytes.size()),
							static_cast<unsigned long long>(E.Digest));
			}
		}
		return 0;
	}

	/// 16.12(b): the seeded save points, and the fixed point they have to be.
	///
	/// A save is not a picture of a world - it is a CLAIM that the world can be
	/// put back. This walks a recorded stream, stops at K points chosen by the
	/// seed rather than by whoever wrote the test, and at each one asks four
	/// things that a save which merely LOOKS right would fail:
	///
	///   1. RE-SAVE IS A FIXED POINT. Adopt the container, save the adopted
	///      world again, and the bytes must be identical. A save that cannot
	///      reproduce itself is carrying something it did not restore.
	///   2. ADOPTING THE RE-SAVE lands on the same state digest.
	///   3. THE REST OF THE WALK still leads where it led. The remainder of the
	///      stream is replayed into the adopted world and must reach the
	///      digests the uninterrupted replay reached.
	///   4. A SECOND ADOPT INTO THE SAME WORLD IS REFUSED, by name. Re-adopting
	///      over a live world would be a silent half-restore of everything the
	///      container does not carry (16.06).
	///
	/// --withhold-run and --corrupt-byte are the controls, and they are what
	/// make the four above evidence rather than a green light. See where each
	/// is handled below.
	/// A SAVE BOUNDARY IS TWO CUTS AND NOT ONE, and this walk taught it. Replay
	/// submits the commands due BEFORE it turns the day and applies the looks
	/// and takings due AFTER it:
	///
	///     SubmitDue()   commands with Tick <= Now   <- Now is still yesterday
	///     Day()                                     <- Now becomes today
	///     LookDue() TakeUpDue()                     <- against today
	///     the save is taken here
	///
	/// So at a save taken at the end of day d, a LOOK at today's tick has
	/// already been applied and a COMMAND at today's tick has NOT. Cutting both
	/// at the same tick loses that command, and the walk resumes without an
	/// intent the original executed.
	///
	/// Measured on the owner's lived walk: its two Speak intents sit at ticks
	/// 3458520 and 3458544, which are exactly the ticks of days 105 and 106.
	/// A save at day 105 cut with one tick dropped the first of them, and the
	/// restored walk parted from the uninterrupted one - three save points of
	/// four landed, and the fourth was the only one that had an intent sitting
	/// on its boundary. A stream with no commands would never have shown it.
	struct FuzzPoint
	{
		uint32 Day = 0;
		/// The world's tick when the save was taken: the cut for looks and
		/// takings, which have been applied up to and including it.
		uint64 Tick = 0;
		/// The tick before this day was turned: the cut for commands, which
		/// have been submitted only up to and including THAT.
		uint64 Before = 0;
		std::vector<Vaelen::uint8> Bytes;
	};

	struct FuzzCatch
	{
		const std::vector<uint32>* Wanted = nullptr;
		std::vector<FuzzPoint>* Points = nullptr;
		uint32* Refused = nullptr;
		uint64 Prev = 0;
	};

	void FuzzBegun(const Vaelen::Run::Aelvor& W, void* User)
	{
		// Before the first day turn: the command cut for a save taken at the
		// end of day 0.
		static_cast<FuzzCatch*>(User)->Prev = W.Now();
	}

	void FuzzAfterDay(const Vaelen::Run::Aelvor& W, Vaelen::uint32 Day, void* User)
	{
		FuzzCatch& C = *static_cast<FuzzCatch*>(User);
		for (Vaelen::uint32 Want : *C.Wanted)
		{
			if (Want != Day)
			{
				continue;
			}
			FuzzPoint P;
			P.Day = Day;
			P.Tick = W.Now();
			P.Before = C.Prev;
			const Vaelen::Run::CheckpointResult R = Vaelen::Run::BuildCheckpoint(W, P.Bytes);
			if (R == Vaelen::Run::CheckpointResult::Ok)
			{
				C.Points->push_back(std::move(P));
			}
			else
			{
				// Said out loud rather than dropped: a point that vanishes
				// silently is a point nothing downstream can count.
				std::fprintf(stderr, "savefuzz: day %u REFUSED its container, %s\n", Day,
							 Vaelen::Run::CheckpointResultToString(R));
				++*C.Refused;
			}
		}
		// After it is used, never before: this is tomorrow's yesterday.
		C.Prev = W.Now();
	}

	/// What is left of a walk after a given tick: the same records, the same
	/// header, starting where the save was taken. Records at or before the
	/// tick have already been applied to the world being restored - the save
	/// was taken at the END of that day, after its looks and takings.
	Player::InputStream RestOf(const Player::InputStream& S, uint64 Tick, uint64 Before, Vaelen::usize AfterDay)
	{
		Player::InputStream R;
		R.Header = S.Header;
		for (const Player::Recorded& C : S.Commands)
		{
			// Before, not Tick: see FuzzPoint. A command on the boundary tick
			// has not been submitted yet.
			if (C.Tick > Before)
			{
				R.Commands.push_back(C);
			}
		}
		for (const Player::TakenUp& T : S.Takings)
		{
			if (T.Tick > Tick)
			{
				R.Takings.push_back(T);
			}
		}
		for (const Player::Looked& L : S.Looks)
		{
			if (L.Tick > Tick)
			{
				R.Looks.push_back(L);
			}
		}
		for (Vaelen::usize d = AfterDay + 1u; d < S.Days.size(); ++d)
		{
			R.Days.push_back(S.Days[d]);
		}
		return R;
	}

	/// Deterministic, and from the seed the caller gave: which day turns get a
	/// save. Chosen this way so that a run is reproducible from its printed
	/// seed, and so that the points are not the three a person would pick.
	std::vector<uint32> WhichDays(uint64 Seed, uint32 Points, uint32 Days)
	{
		std::vector<uint32> Picked;
		if (Days == 0u || Points == 0u)
		{
			return Picked;
		}
		uint64 X = Seed + 0x9E3779B97F4A7C15ull;
		for (uint32 i = 0; i < Points * 8u && Picked.size() < Points; ++i)
		{
			X += 0x9E3779B97F4A7C15ull;
			uint64 Z = X;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			Z ^= Z >> 31;
			// Never the last day: a save taken there is never CONTINUED, and
			// continuing is most of what these points are for.
			const uint32 Day = static_cast<uint32>(Z % (Days > 1u ? Days - 1u : 1u));
			bool Had = false;
			for (uint32 P : Picked)
			{
				Had = Had || P == Day;
			}
			if (!Had)
			{
				Picked.push_back(Day);
			}
		}
		std::sort(Picked.begin(), Picked.end());
		return Picked;
	}

	int RunSaveFuzz(const Options& Opt)
	{
		std::string Text;
		if (!ReadFile(Opt.SaveFuzz, Text))
		{
			std::fprintf(stderr, "savefuzz: cannot read %s\n", Opt.SaveFuzz.c_str());
			return 1;
		}
		Player::InputStream S;
		Player::StreamReport Report;
		if (!Player::DecodeStream(Text, S, Report))
		{
			std::fprintf(stderr, "savefuzz: %s is not a stream this build reads\n", Opt.SaveFuzz.c_str());
			return 1;
		}
		if (S.Days.empty())
		{
			std::fprintf(stderr, "savefuzz: %s turns no days, so it has no save points\n", Opt.SaveFuzz.c_str());
			return 1;
		}

		Vaelen::Run::Options RO;
		RO.Size = S.Header.Size;
		RO.PreHistory = S.Header.PreHistory;
		RO.Years = S.Header.Years;
		RO.Seed = S.Header.Seed;
		RO.Colony = Opt.Colony;
		RO.Play = true;
		RO.Stream = Opt.Stream;
		RO.Lively = Opt.Lively;

		const std::vector<uint32> Wanted = WhichDays(Opt.Seed, Opt.Points, static_cast<uint32>(S.Days.size()));
		if (Wanted.empty())
		{
			std::fprintf(stderr, "savefuzz: no save points chosen\n");
			return 1;
		}

		Vaelen::Run::Aelvor Whole(RO);
		if (!Whole.Begin())
		{
			std::fprintf(stderr, "savefuzz: generation failed at %u\n", RO.Size);
			return 1;
		}
		Player::StartRules Host;
		Host.WantBound = Opt.WantBound;
		Host.FromAge = Opt.FromAge;
		Host.ToAge = Opt.ToAge;

		std::vector<FuzzPoint> Points;
		uint32 CouldNotBuild = 0;
		FuzzCatch Catch;
		Catch.Wanted = &Wanted;
		Catch.Points = &Points;
		Catch.Refused = &CouldNotBuild;
		Vaelen::Run::DayWatch Watching;
		Watching.Begun = &FuzzBegun;
		Watching.After = &FuzzAfterDay;
		Watching.User = &Catch;
		const Vaelen::Run::ReplayReport Straight = Vaelen::Run::Replay(Whole, S, Host, Watching);
		if (Straight.Refused != 0u)
		{
			std::fprintf(stderr, "savefuzz: the walk was refused by its own replay\n");
			return 1;
		}
		// THE BASELINE HAS TO BE THE RECORDED WALK, AND THIS CHECK WAS MISSING.
		//
		// Refused covers only a precondition - not begun, no Play, or a header
		// SameWorld rejects. It is NOT set when the replay reaches different
		// PEOPLE, which is Wrong/WrongTakings, and that is exactly what a wrong
		// host wiring produces. The baseline, every container and every resumed
		// run are all built from the same RO, so a wrong RO is wrong
		// identically everywhere and every save point still "lands": the tool
		// was a self-consistency check that could not notice it had been handed
		// the wrong world.
		//
		// Measured: the death walk replayed WITHOUT --lively reported
		// "0 of 3 save point(s) bad" and exited 0, on containers of 2 MB rather
		// than 208 MB and a state of a973fafaa72d6be5 rather than
		// 18aec14e39a68a8c. Tests/Run/Streams/README.md said those flags
		// mattered; the tool enforced not one of them.
		if (Straight.Wrong != 0u || Straight.Left != 0u)
		{
			std::fprintf(stderr,
						 "savefuzz: THE WALK DOES NOT REPLAY IN THIS WORLD - %u wrong (%u of them takings), %u "
						 "record(s) never reached. The wiring given on the command line is not the one it was "
						 "recorded with, and a .stream file cannot carry it.\n",
						 Straight.Wrong, Straight.WrongTakings, Straight.Left);
			return 1;
		}
		std::printf("savefuzz: %s, %zu day turns, %zu save point(s) from seed %llx, wiring stream=%d\n",
					Opt.SaveFuzz.c_str(), S.Days.size(), Points.size(), static_cast<unsigned long long>(Opt.Seed),
					Opt.Stream ? 1 : 0);
		if (!Opt.ExpectState.empty())
		{
			char Got[32];
			std::snprintf(Got, sizeof Got, "%016llx", static_cast<unsigned long long>(Straight.State));
			if (Opt.ExpectState != Got)
			{
				std::fprintf(stderr,
							 "savefuzz: THE UNINTERRUPTED WALK REACHED %s, NOT %s. The world built from this "
							 "command line is not the one the walk was recorded in.\n",
							 Got, Opt.ExpectState.c_str());
				return 1;
			}
			std::printf("savefuzz: the uninterrupted walk reaches %s, as pinned\n", Got);
		}
		std::printf("savefuzz: uninterrupted: state %016llx, log %016llx, life %016llx\n",
					static_cast<unsigned long long>(Straight.State), static_cast<unsigned long long>(Straight.Log),
					static_cast<unsigned long long>(Straight.Life));

		// THE CORRUPTION CONTROL, taken on the first save point because one
		// container is enough to answer it: a byte is flipped and the reader
		// must REFUSE. A save format whose reader accepts a flipped byte is a
		// save format that will one day hand a host a world nobody simulated.
		if (Opt.CorruptByte >= 0 && !Points.empty())
		{
			std::vector<Vaelen::uint8> Hurt = Points[0].Bytes;
			const Vaelen::usize At = static_cast<Vaelen::usize>(Opt.CorruptByte) % Hurt.size();
			Hurt[At] = static_cast<Vaelen::uint8>(Hurt[At] ^ 0x40u);
			Vaelen::Run::CheckpointView View;
			const Vaelen::Run::CheckpointRefusal R = Vaelen::Run::ReadCheckpoint(Hurt.data(), Hurt.size(), View);
			const bool Refused = R.Result != Vaelen::Run::CheckpointResult::Ok;
			std::printf("savefuzz: byte %zu of %zu flipped -> %s\n", At, Hurt.size(),
						Vaelen::Run::CheckpointResultToString(R.Result));
			if (!Refused)
			{
				std::printf("savefuzz: FAIL a flipped byte was accepted\n");
				return 1;
			}
			std::printf("savefuzz: PASS a flipped byte is refused, not loaded\n");
			return 0;
		}

		uint32 Bad = 0;
		uint32 Parted = 0;
		long FirstParted = -1;
		for (const FuzzPoint& P : Points)
		{
			// (1) and (2): adopt, re-save, and the fixed point.
			Vaelen::Run::Aelvor Back(RO);
			const Vaelen::Run::Aelvor::AdoptResult A = Back.Adopt(P.Bytes.data(), P.Bytes.size());
			if (A != Vaelen::Run::Aelvor::AdoptResult::Ok)
			{
				std::printf("savefuzz: day %u REFUSED, %s\n", P.Day, Vaelen::Run::Aelvor::AdoptResultToString(A));
				++Bad;
				continue;
			}
			// (4), and it is asked before anything else touches this world: a
			// second adopt must be refused BY NAME, not merely fail somehow.
			const Vaelen::Run::Aelvor::AdoptResult Again = Back.Adopt(P.Bytes.data(), P.Bytes.size());
			const bool RefusedAgain = Again == Vaelen::Run::Aelvor::AdoptResult::AlreadyBegun;

			std::vector<Vaelen::uint8> Twice;
			const bool Resaved = Vaelen::Run::BuildCheckpoint(Back, Twice) == Vaelen::Run::CheckpointResult::Ok;
			const bool FixedPoint = Resaved && Twice == P.Bytes;

			Vaelen::Run::Aelvor Third(RO);
			const bool ThirdOk =
				Resaved && Third.Adopt(Twice.data(), Twice.size()) == Vaelen::Run::Aelvor::AdoptResult::Ok;
			const bool SameAgain = ThirdOk && Third.StateDigest() == Back.StateDigest();

			// (3): the rest of the walk, from where the save was taken.
			const Player::InputStream Rest = RestOf(S, P.Tick, P.Before, P.Day);
			const Vaelen::Run::ReplayReport Went = Vaelen::Run::Replay(Back, Rest, Host);
			const bool Landed = Went.Refused == 0u && Went.State == Straight.State && Went.Log == Straight.Log &&
								Went.Life == Straight.Life;

			const bool Cell = FixedPoint && SameAgain && RefusedAgain && Landed;
			if (!Cell)
			{
				++Bad;
			}
			if (!Landed)
			{
				++Parted;
				if (FirstParted < 0)
				{
					FirstParted = static_cast<long>(P.Day);
				}
			}
			std::printf("savefuzz: day %3u  %zu bytes  resave %s  re-adopt %s  second adopt %s  rest %s"
						"  state %016llx\n",
						P.Day, P.Bytes.size(), FixedPoint ? "identical" : "DIFFERS", SameAgain ? "same" : "DIFFERS",
						RefusedAgain ? "refused" : "ACCEPTED", Landed ? "lands" : "PARTS",
						static_cast<unsigned long long>(Went.State));
		}

		// THE WITHHELD-RUN CONTROL. Every save point is restored again through
		// Begin() + LoadSnapshot, which is the one path that can leave the RUN
		// section behind, and the walk continued from there. With the daily
		// cadence on, this MUST report mismatches: the warden reads what the
		// run holds. A savefuzz that came back clean here would be reporting
		// that its own subject does not matter.
		if (Opt.WithholdRun)
		{
			uint32 Mismatched = 0;
			long FirstDay = -1;
			uint64 FirstTick = 0;
			for (const FuzzPoint& P : Points)
			{
				Vaelen::Run::CheckpointView View;
				if (Vaelen::Run::ReadCheckpoint(P.Bytes.data(), P.Bytes.size(), View).Result !=
					Vaelen::Run::CheckpointResult::Ok)
				{
					continue;
				}
				Vaelen::uint64 Length = 0;
				const Vaelen::uint8* State = View.Find(Vaelen::Run::SectionKind::State, Length);
				if (State == nullptr)
				{
					continue;
				}
				Vaelen::Run::Aelvor Without(RO);
				if (!Without.Begin() ||
					Vaelen::LoadSnapshot(Without.Instance(), State, static_cast<Vaelen::usize>(Length)) !=
						Vaelen::SnapshotResult::Ok)
				{
					continue;
				}
				const Player::InputStream Rest = RestOf(S, P.Tick, P.Before, P.Day);
				const Vaelen::Run::ReplayReport Went = Vaelen::Run::Replay(Without, Rest, Host);
				if (Went.State != Straight.State || Went.Log != Straight.Log || Went.Life != Straight.Life)
				{
					++Mismatched;
					if (FirstDay < 0)
					{
						FirstDay = static_cast<long>(P.Day);
						FirstTick = P.Tick;
					}
				}
			}
			const uint64 FirstLook = S.Looks.empty() ? 0ull : S.Looks[0].Tick;
			std::printf("savefuzz: WITHOUT the run, %u of %zu save point(s) mismatch, first on day %ld "
						"(tick %llu); the walk's first look is at tick %llu\n",
						Mismatched, Points.size(), FirstDay, static_cast<unsigned long long>(FirstTick),
						static_cast<unsigned long long>(FirstLook));
			if (Mismatched == 0u)
			{
				std::printf("savefuzz: FAIL withholding the run changed nothing, so this run measures nothing\n");
				return 1;
			}
			// THIS GUARD CANNOT FIRE, AND SAYING SO IS THE POINT.
			//
			// FirstTick is a save point's P.Tick, set at DayWatch::After, i.e.
			// AFTER a day turn. Both checked-in walks look on their very first
			// day - the death walk's first record of all is a look at tick
			// 777600 - so FirstLook is always at or before the first save
			// point's tick and FirstTick < FirstLook is false for every walk
			// this repository has. It was written to catch a restore broken
			// from tick zero, and it would not catch one.
			//
			// It is kept, and kept honest: the comparison is right, the walks
			// are simply never shaped to trip it. What actually guards that
			// case now is --expect-state above, which pins the uninterrupted
			// walk's digest and fails before any of this is reached.
			if (FirstLook != 0ull && FirstTick < FirstLook)
			{
				std::printf("savefuzz: FAIL the first mismatch is at tick %llu, BEFORE the walk's first look "
							"at %llu - that is not the run being withheld\n",
							static_cast<unsigned long long>(FirstTick), static_cast<unsigned long long>(FirstLook));
				return 1;
			}
			std::printf("savefuzz: PASS withholding the run is what parts the walk, and not before the first "
						"look\n");
			return 0;
		}

		// NO POINTS, OR A POINT THAT COULD NOT BE BUILT, IS A FAILURE.
		// FuzzAfterDay used to drop a container it could not build, so a build
		// that cannot save at all collected nothing, counted nothing bad and
		// returned 0 - the fuzzer reporting success for the one outcome it
		// exists to catch.
		if (Points.empty() || CouldNotBuild != 0u)
		{
			std::fprintf(stderr,
						 "savefuzz: %zu save point(s) built and %u REFUSED their container, so this run did not "
						 "measure what it was asked to\n",
						 Points.size(), CouldNotBuild);
			return 1;
		}
		std::printf("savefuzz: %u of %zu save point(s) bad, %u parted (first on day %ld)\n", Bad, Points.size(), Parted,
					FirstParted);
		return Bad == 0u ? 0 : 1;
	}

	/// 16.12(c): a walk in which the played person DIES, written out.
	///
	/// IT REFUSES TO WRITE A WALK WITHOUT A DEATH IN IT, and that refusal is
	/// the whole value of the mode. A stream named for a death that contains
	/// none would be checked in, replayed by a CTest entry, come back clean,
	/// and pin nothing at all - the same failure --walk refuses, and the same
	/// failure Run.SaveDeath's precondition exists to prevent.
	///
	/// WHY --walk COULD NOT DO THIS. That mode enforces the Phase 15 gate's
	/// clauses on what it writes - somewhere to walk within four days of each
	/// taking, no day turn promoting twice - and a horizon long enough for
	/// somebody to die of old age is not a horizon shaped like that. The two
	/// modes want opposite things from a walk, so they are two modes.
	///
	/// A world only holds people as old as its history, so the defaults here
	/// are sixty years of pre-history and thirty more lived, and a start window
	/// from eighty-five up. Tests/Run/Test_SaveDeath.cpp records the search
	/// that found them.
	int RunDeathWalk(const Options& Opt)
	{
		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Play = true;
		RO.Stream = true;
		// Declared by the caller and printed below, because whoever replays
		// this walk has to be told the same thing: the file cannot carry it.
		RO.Lively = Opt.Lively;
		// --colony was accepted and silently ignored here while --savefuzz and
		// --replay both honoured it. A type declared in a different position is
		// a different world, so that was a world-identity bit taken and dropped.
		RO.Colony = Opt.Colony;
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::fprintf(stderr, "deathwalk: generation failed at %u x %u\n", RO.Size, RO.Size);
			return 1;
		}
		Player::StartRules Rules;
		Rules.WantBound = Opt.WantBound;
		Rules.FromAge = Opt.FromAge;
		Rules.ToAge = Opt.ToAge;
		Rules.FromAge = Opt.FromAge;
		Rules.ToAge = Opt.ToAge;
		Vaelen::Run::Door D(A, Rules);
		const uint32 First = D.TakeUp();
		if (First == 0u)
		{
			std::fprintf(stderr,
						 "deathwalk: a world of %u+%u years offers nobody aged %u to %u - a world holds only "
						 "people as old as its history\n",
						 RO.PreHistory, RO.Years, Opt.FromAge, Opt.ToAge);
			return 1;
		}

		uint32 Deaths = 0;
		long DiedOn = -1;
		for (uint32 Day = 0; Day < Opt.DeathDays; ++Day)
		{
			const uint32 Playing = A.Played();
			Vaelen::Run::Attention At;
			At.Region = static_cast<uint32>(1u + (Day % 7u));
			At.Reach = 1u;
			D.Look(At);
			D.Day();
			if (A.Played() != Playing)
			{
				++Deaths;
				if (DiedOn < 0)
				{
					DiedOn = static_cast<long>(Day);
				}
			}
		}

		if (Deaths == 0u)
		{
			std::fprintf(stderr,
						 "deathwalk: NOBODY DIED in %u day turns, so this walk is not what its name says and it "
						 "will not be written. Person %u was taken up and outlived the horizon.\n",
						 Opt.DeathDays, First);
			return 1;
		}
		if (D.Stream().Takings.size() < 2u)
		{
			std::fprintf(stderr, "deathwalk: somebody died but nobody was taken up after them, so the tape carries one "
								 "taking. That is a different horizon - see Door.h - and it is not written here.\n");
			return 1;
		}

		const std::string Text = Player::EncodeStream(D.Stream());
		std::FILE* File = std::fopen(Opt.DeathWalk.c_str(), "wb");
		if (File == nullptr)
		{
			std::fprintf(stderr, "deathwalk: cannot write %s\n", Opt.DeathWalk.c_str());
			return 1;
		}
		const usize Wrote = std::fwrite(Text.data(), 1, Text.size(), File);
		const bool Closed = std::fclose(File) == 0;
		if (Wrote != Text.size() || !Closed)
		{
			std::fprintf(stderr, "deathwalk: could not write all of %s\n", Opt.DeathWalk.c_str());
			return 1;
		}
		std::printf("deathwalk: AELVOR %u, %u+%u years, ages %u-%u, lively=%d: person %u taken up, died on day "
					"%ld, %u death(s), %zu takings, %zu day turns, %zu bytes\n",
					RO.Size, RO.PreHistory, RO.Years, Opt.FromAge, Opt.ToAge, RO.Lively ? 1 : 0, First, DiedOn, Deaths,
					D.Stream().Takings.size(), D.Stream().Days.size(), Text.size());
		// EVERY FLAG SUBSTITUTED, none baked in. The first version hardcoded
		// --lively into this format string while honouring Opt.Lively when
		// recording, so a walk written WITHOUT it printed a recipe that builds
		// a different world - the precise defect this line exists to prevent,
		// committed inside the line itself.
		std::printf("deathwalk: REPLAY IT WITH --stream%s%s --from-age %u --to-age %u --want-bound %u; the file "
					"carries none of that\n",
					RO.Lively ? " --lively" : "", RO.Colony ? " --colony" : "", Opt.FromAge, Opt.ToAge, Opt.WantBound);
		return 0;
	}

	/// Phase 16 gate clause (k). Two halves, two PROCESSES, one file.
	///
	/// --save-to writes a container and prints the digest the world was at
	/// when it was written, plus the digest it reaches after --then-days more
	/// day turns IN THIS PROCESS. --load-from reads that file in a fresh
	/// process, adopts it, turns the same number of days and prints what it
	/// reaches. The two must agree.
	///
	/// Run.Store already proves the WRITE is atomic and that a full disk
	/// refuses without destroying what was there. This proves the other half:
	/// that the bytes which reached the disk are a world, and that continuing
	/// from them in a process that never saw the original lands where the
	/// original landed. Nothing in the tree did that before - every save test
	/// held both worlds in one address space.
	int RunSaveTo(const Options& Opt)
	{
		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Colony = Opt.Colony;
		RO.Play = true;
		RO.Stream = Opt.Stream;
		RO.Lively = Opt.Lively;
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::fprintf(stderr, "save-to: generation failed at %u\n", RO.Size);
			return 1;
		}
		Player::StartRules Rules;
		Rules.WantBound = Opt.WantBound;
		Rules.FromAge = Opt.FromAge;
		Rules.ToAge = Opt.ToAge;
		A.TakeUp(Rules);
		for (uint32 d = 0; d < 6u; ++d)
		{
			Vaelen::Run::Attention At;
			At.Region = 1u + (d % 5u);
			At.Reach = 1u;
			A.LookAt(At);
			A.Day();
		}

		std::vector<Vaelen::uint8> Bytes;
		if (Vaelen::Run::BuildCheckpoint(A, Bytes) != Vaelen::Run::CheckpointResult::Ok)
		{
			std::fprintf(stderr, "save-to: the container was refused\n");
			return 1;
		}
		std::FILE* F = std::fopen(Opt.SaveTo.c_str(), "wb");
		if (F == nullptr)
		{
			std::fprintf(stderr, "save-to: cannot write %s\n", Opt.SaveTo.c_str());
			return 1;
		}
		const usize Wrote = std::fwrite(Bytes.data(), 1, Bytes.size(), F);
		const bool Closed = std::fclose(F) == 0;
		if (Wrote != Bytes.size() || !Closed)
		{
			std::fprintf(stderr, "save-to: short write to %s\n", Opt.SaveTo.c_str());
			return 1;
		}
		std::printf("save-to: %zu bytes to %s, saved at %016llx\n", Bytes.size(), Opt.SaveTo.c_str(),
					static_cast<unsigned long long>(Vaelen::ComputeStateDigest(A.Instance())));
		// The same continuation the other process will make, in THIS one.
		for (uint32 d = 0; d < Opt.ThenDays; ++d)
		{
			Vaelen::Run::Attention At;
			At.Region = 2u + (d % 4u);
			At.Reach = 1u;
			A.LookAt(At);
			A.Day();
		}
		std::printf("save-to: %u more day(s) in one process reach %016llx\n", Opt.ThenDays,
					static_cast<unsigned long long>(Vaelen::ComputeStateDigest(A.Instance())));
		return 0;
	}

	int RunLoadFrom(const Options& Opt)
	{
		std::FILE* F = std::fopen(Opt.LoadFrom.c_str(), "rb");
		if (F == nullptr)
		{
			std::fprintf(stderr, "load-from: cannot read %s\n", Opt.LoadFrom.c_str());
			return 1;
		}
		std::fseek(F, 0, SEEK_END);
		const long Len = std::ftell(F);
		std::fseek(F, 0, SEEK_SET);
		std::vector<Vaelen::uint8> Bytes(static_cast<usize>(Len > 0 ? Len : 0));
		const usize Got = Bytes.empty() ? 0u : std::fread(Bytes.data(), 1, Bytes.size(), F);
		std::fclose(F);
		if (Bytes.empty() || Got != Bytes.size())
		{
			std::fprintf(stderr, "load-from: short read of %s\n", Opt.LoadFrom.c_str());
			return 1;
		}

		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Colony = Opt.Colony;
		RO.Play = true;
		RO.Stream = Opt.Stream;
		RO.Lively = Opt.Lively;
		// CONSTRUCTED, NOT BEGUN: this process never generates the world.
		Vaelen::Run::Aelvor A(RO);
		const Vaelen::Run::Aelvor::AdoptResult R = A.Adopt(Bytes.data(), Bytes.size());
		if (R != Vaelen::Run::Aelvor::AdoptResult::Ok)
		{
			std::fprintf(stderr, "load-from: REFUSED, %s\n", Vaelen::Run::Aelvor::AdoptResultToString(R));
			return 1;
		}
		std::printf("load-from: %zu bytes, Generations %u, adopted at %016llx\n", Bytes.size(), A.Generations(),
					static_cast<unsigned long long>(Vaelen::ComputeStateDigest(A.Instance())));
		for (uint32 d = 0; d < Opt.ThenDays; ++d)
		{
			Vaelen::Run::Attention At;
			At.Region = 2u + (d % 4u);
			At.Reach = 1u;
			A.LookAt(At);
			A.Day();
		}
		std::printf("load-from: %u more day(s) in a second process reach %016llx\n", Opt.ThenDays,
					static_cast<unsigned long long>(Vaelen::ComputeStateDigest(A.Instance())));
		return 0;
	}

	int RunGolden(const Options& Opt)
	{
		std::string Where = Opt.Golden;
		if (!Where.empty() && Where.back() != '/')
		{
			Where += '/';
		}
		std::string Lines;
		for (const Golden& G : Goldens)
		{
			Vaelen::Run::Options RO;
			RO.Size = G.Size;
			RO.PreHistory = G.PreHistory;
			RO.Years = G.Years;
			if (G.Full)
			{
				RO.Play = true;
				RO.Stream = true;
				RO.Lively = true;
				RO.Colony = true;
			}
			Vaelen::Run::Aelvor A(RO);
			if (!A.Begin())
			{
				std::fprintf(stderr, "AELVOR: %s would not generate at %u\n", G.Name, G.Size);
				return 1;
			}
			std::vector<uint8> Image;
			SaveSnapshot(A.Instance(), Image);

			const std::string Path = Where + G.Name;
			std::FILE* File = std::fopen(Path.c_str(), "wb");
			if (File == nullptr)
			{
				std::fprintf(stderr, "AELVOR: cannot write %s\n", Path.c_str());
				return 1;
			}
			const usize Wrote = std::fwrite(Image.data(), 1, Image.size(), File);
			const bool Closed = std::fclose(File) == 0;
			if (Wrote != Image.size() || !Closed)
			{
				std::fprintf(stderr, "AELVOR: could not write all of %s\n", Path.c_str());
				return 1;
			}

			// EVERY NUMBER A LOADER WILL LATER CHECK, recorded beside the file
			// rather than left to be rediscovered. The layout digest is the one
			// LoadSnapshot compares, so a corpus that did not carry it could
			// not tell a refusal from a regression.
			const Hash64 Layout = HashCombine(A.Instance().Types().LayoutDigest(), A.Instance().Map().LayoutDigest());
			char Row[512];
			std::snprintf(Row, sizeof(Row), "| `%s` | %u | %u | %u | %s | %u | `%016llx` | `%016llx` | %zu |\n", G.Name,
						  G.Size, G.PreHistory, G.Years, G.Full ? "Play+Stream+Lively+Colony" : "none",
						  static_cast<unsigned>(VAELEN_SAVE_FORMAT_VERSION), static_cast<unsigned long long>(Layout),
						  static_cast<unsigned long long>(A.StateDigest()), Image.size());
			Lines += Row;
			std::printf("golden %-18s %8zu bytes, layout %016llx, state %016llx - %s\n", G.Name, Image.size(),
						static_cast<unsigned long long>(Layout), static_cast<unsigned long long>(A.StateDigest()),
						G.What);
		}
		std::printf("golden: %zu images written to %s\n", sizeof(Goldens) / sizeof(Goldens[0]), Where.c_str());
		std::printf("%s", Lines.c_str());
		return 0;
	}

	// ------------------------------------------------------------------
	// 17.01: THE CONTAINER CORPUS.
	//
	// Phase 16 built VAELENCP and closed without a single one checked in.
	// `Tests/Run/Golden/` holds `.snapshot` IMAGES, which is the inner format
	// and a different thing: a reader handed one says BadMagic. So every tool
	// Phase 17 plans - the inspector, the store's cold listing, the census -
	// was written against files that do not exist.
	//
	// THREE CONTAINERS, chosen so that the SHAPE differs and not only the
	// contents, because a corpus where every file has the same section table
	// cannot catch a reader that assumes one:
	//   bare-16     three sections, from a world with no play wiring at all
	//   played-16   FOUR sections - the four-argument build carries the tape
	//   full-32     three sections from a world that WAS played, saved through
	//               the two-argument form. A container with no STREAM section
	//               is not the same as a world that was never played, and the
	//               corpus has to be able to tell a reader so.
	//
	// AND THE RECORD IS READ BACK, not asserted. Every number the README
	// carries comes out of `ReadCheckpoint` over the bytes that were just
	// written, so what is recorded is what a reader SEES rather than what the
	// writer meant. The two differing is exactly the class of defect 17.03
	// found in the store.
	struct Container
	{
		const char* Name;
		const char* What;
		uint32 Size;
		uint32 PreHistory;
		uint32 Years;
		bool Play;	 ///< the play wiring, so a person can be taken up
		bool Played; ///< a Door walks it before the save
		bool Tape;	 ///< the four-argument build, so a STREAM section exists
	};

	constexpr Container Containers[] = {
		{"bare-16.container", "no play wiring at all: three sections, and nobody was ever offered", 16, 10, 1, false,
		 false, false},
		{"played-16.container", "walked six days and carrying its own tape: four sections", 16, 10, 1, true, true,
		 true},
		{"full-32.container",
		 "walked six days and saved WITHOUT its tape: three sections, and the difference from the"
		 " one above is the whole point",
		 32, 10, 1, true, true, false},
	};

	const char* KindName(uint16 Kind) noexcept
	{
		switch (static_cast<Vaelen::Run::SectionKind>(Kind))
		{
		case Vaelen::Run::SectionKind::State:
			return "STATE";
		case Vaelen::Run::SectionKind::Run:
			return "RUN";
		case Vaelen::Run::SectionKind::Host:
			return "HOST";
		case Vaelen::Run::SectionKind::Stream:
			return "STREAM";
		default:
			return "?";
		}
	}

	/// The image's own trailer: the last eight bytes of the STATE section,
	/// which is what `ComputeStateDigest` returns and what every frozen digest
	/// in this repository is. NOT the section digest beside it in the table,
	/// which is a different number over the same bytes - confusing the two is
	/// defect 17.03.
	bool TrailerOf(const Vaelen::Run::CheckpointView& View, Vaelen::Hash64& Out) noexcept
	{
		// The reading is the kernel's since 16.14 (Run::ImageTrailer); this
		// keeps the "is there one" answer the callers here want.
		uint64 Length = 0;
		if (View.Find(Vaelen::Run::SectionKind::State, Length) == nullptr || Length < sizeof(uint64))
		{
			return false;
		}
		Out = Vaelen::Run::ImageTrailer(View);
		return true;
	}

	int RunContainers(const Options& Opt)
	{
		std::string Where = Opt.Containers;
		if (!Where.empty() && Where.back() != '/')
		{
			Where += '/';
		}
		std::string Rows;
		std::string Tables;
		for (const Container& C : Containers)
		{
			Vaelen::Run::Options RO;
			RO.Size = C.Size;
			RO.PreHistory = C.PreHistory;
			RO.Years = C.Years;
			if (C.Play)
			{
				RO.Play = true;
				RO.Stream = true;
				RO.Lively = true;
				RO.Colony = true;
			}
			Vaelen::Run::Aelvor A(RO);
			if (!A.Begin())
			{
				std::fprintf(stderr, "containers: %s would not generate at %u\n", C.Name, C.Size);
				return 1;
			}

			// THE RULES ARE ALL FOUR NON-DEFAULT, deliberately, and two reasons
			// meet here.
			//
			// The first is measured, and it is a fact about a young world
			// rather than about the rules. Sweeping the window at 16 and at 32
			// tiles, ten years of pre-history and one of history:
			//
			//   bound 1, any window at all ...... nobody
			//   bound 0, ages 0-10 .............. person 1
			//   bound 0, ages 12-120 ............ nobody
			//
			// NOBODY IN A TEN-YEAR WORLD IS TWELVE. Everyone alive was born
			// inside it, and nobody in it is bound to anything yet. So
			// `StartRules{}`, which asks for a bound life aged 16 to 40, is
			// offered nobody - which is why `full-16.snapshot` next door was
			// never played, and why this corpus could not have a played
			// container until the window was opened. The window is 0-45 and
			// not 0-11 on purpose: a superset outlives the day these ages
			// move, and pinning the boundary would make the corpus a test of
			// demography.
			//
			// The second is 17.07's: a round trip that compares two
			// default-constructed structs passes even when the reader wrote
			// nothing, and this phase committed exactly that assertion. A
			// corpus whose STREAM section carries `StartRules{}` would hand
			// every later test the same vacuous comparison. These four values
			// are in the README, and a reader that drops the section cannot
			// agree with them.
			//
			// The walk itself is the SAME for both played containers, so that
			// played-16 and full-32 differ only in map size and in whether the
			// tape travels.
			Player::StartRules Rules;
			Rules.FromAge = 0u;
			Rules.ToAge = 45u;
			Rules.WantBound = 0u;
			Rules.PreferOre = 0u;
			Player::InputStream Tape;
			if (C.Played)
			{
				Vaelen::Run::Door D(A, Rules);
				if (D.TakeUp() == 0)
				{
					std::fprintf(stderr, "containers: %s offered nobody to take up\n", C.Name);
					return 1;
				}
				for (uint32 Day = 0; Day < 6u; ++Day)
				{
					Vaelen::Run::Attention At;
					At.Region = 1u + (Day % 5u);
					At.Reach = 1u;
					D.Look(At);
					D.Day();
				}
				Tape = D.Stream();
			}

			std::vector<uint8> Bytes;
			const Vaelen::Run::CheckpointResult Built =
				C.Tape ? Vaelen::Run::BuildCheckpoint(A, Tape, Rules, Bytes) : Vaelen::Run::BuildCheckpoint(A, Bytes);
			if (Built != Vaelen::Run::CheckpointResult::Ok)
			{
				std::fprintf(stderr, "containers: %s was refused - %s\n", C.Name,
							 Vaelen::Run::CheckpointResultToString(Built));
				return 1;
			}

			const std::string Path = Where + C.Name;
			std::FILE* File = std::fopen(Path.c_str(), "wb");
			if (File == nullptr)
			{
				std::fprintf(stderr, "containers: cannot write %s\n", Path.c_str());
				return 1;
			}
			const usize Wrote = std::fwrite(Bytes.data(), 1, Bytes.size(), File);
			const bool Closed = std::fclose(File) == 0;
			if (Wrote != Bytes.size() || !Closed)
			{
				std::fprintf(stderr, "containers: could not write all of %s\n", Path.c_str());
				return 1;
			}

			// READ BACK, and every recorded number comes from the view.
			Vaelen::Run::CheckpointView View;
			const Vaelen::Run::CheckpointRefusal Read = Vaelen::Run::ReadCheckpoint(Bytes.data(), Bytes.size(), View);
			if (Read.Result != Vaelen::Run::CheckpointResult::Ok)
			{
				std::fprintf(stderr, "containers: %s did not read back - %s\n", C.Name,
							 Vaelen::Run::CheckpointResultToString(Read.Result));
				return 1;
			}
			Vaelen::Hash64 Trailer = 0;
			if (!TrailerOf(View, Trailer))
			{
				std::fprintf(stderr, "containers: %s has no STATE section to take a trailer from\n", C.Name);
				return 1;
			}
			// The corpus is worth nothing if the trailer it records is not the
			// digest the rest of the repository means by one.
			const Vaelen::Hash64 Live = Vaelen::ComputeStateDigest(A.Instance());
			if (Trailer != Live)
			{
				std::fprintf(stderr, "containers: %s trailer %016llx is not ComputeStateDigest %016llx\n", C.Name,
							 static_cast<unsigned long long>(Trailer), static_cast<unsigned long long>(Live));
				return 1;
			}

			char Row[640];
			std::snprintf(
				Row, sizeof(Row), "| `%s` | %u | %u | %u | %s | %u | %u | %llu | %llu | %llu | %zu | `%016llx` |\n",
				C.Name, C.Size, C.PreHistory, C.Years, C.Play ? "Play+Stream+Lively+Colony" : "none", View.Version,
				View.InnerFormat, static_cast<unsigned long long>(View.Tick),
				static_cast<unsigned long long>(View.LogEvents), static_cast<unsigned long long>(View.LogBytes),
				Bytes.size(), static_cast<unsigned long long>(Trailer));
			Rows += Row;

			// 640 and CHECKED, because the first cut of this printed a table
			// header sliced in half at `|---|---|--` and the corpus looked
			// fine: the files were right and only the record was truncated,
			// which is the same class of defect as a README that agrees with
			// nothing. snprintf returns what it WOULD have written.
			char Head[640];
			const int Want = std::snprintf(Head, sizeof(Head),
										   "\n### `%s`\n\n%s\n\nSeed `%016llx`, flags `%08x`, %zu sections.\n\n"
										   "| kind | offset | length | section digest |\n|---|---|---|---|\n",
										   C.Name, C.What, static_cast<unsigned long long>(View.Seed), View.Flags,
										   View.Sections.size());
			if (Want < 0 || static_cast<usize>(Want) >= sizeof(Head))
			{
				std::fprintf(stderr, "containers: %s - the record does not fit in %zu bytes\n", C.Name, sizeof(Head));
				return 1;
			}
			Tables += Head;
			for (const Vaelen::Run::SectionEntry& E : View.Sections)
			{
				char Line[256];
				std::snprintf(Line, sizeof(Line), "| %s (%u) | %llu | %llu | `%016llx` |\n", KindName(E.Kind), E.Kind,
							  static_cast<unsigned long long>(E.Offset), static_cast<unsigned long long>(E.Length),
							  static_cast<unsigned long long>(E.Digest));
				Tables += Line;
			}

			std::printf("container %-20s %8zu bytes, %zu sections, tick %llu, trailer %016llx - %s\n", C.Name,
						Bytes.size(), View.Sections.size(), static_cast<unsigned long long>(View.Tick),
						static_cast<unsigned long long>(Trailer), C.What);
		}
		std::printf("containers: %zu written to %s\n", sizeof(Containers) / sizeof(Containers[0]), Where.c_str());
		std::printf("%s%s", Rows.c_str(), Tables.c_str());
		return 0;
	}

	// ------------------------------------------------------------------
	// 17.05: THE CAUSE CENSUS, printed for a person.
	//
	// The kernel's `TakeCauseCensus` counts the whole log; this adds the table
	// per event TYPE, which is the form a person needs - "which publishers
	// pass a cause and which do not" is a question about types - and it is
	// the first consumer of 17.02's name table. Without that table every row
	// here would be sixteen hex digits.
	int PrintCensus(const Vaelen::World& W, const char* What)
	{
		const Vaelen::EventLog& Log = W.Log();
		const Vaelen::History::CauseCensus C = Vaelen::History::TakeCauseCensus(Log);

		std::printf("causes: %s\n", What);
		std::printf("causes: %llu events, %llu with a cause (%.2f%%), %llu roots\n",
					static_cast<unsigned long long>(C.Events), static_cast<unsigned long long>(C.WithCause),
					C.Events == 0u ? 0.0 : 100.0 * static_cast<double>(C.WithCause) / static_cast<double>(C.Events),
					static_cast<unsigned long long>(C.RootCauses));
		std::printf("causes: deepest chain %u edge(s), median depth %u, widest fan-out %u", C.MaxDepth, C.MedianDepth,
					C.MaxFanOut);
		if (C.Busiest.IsValid())
		{
			std::printf(" (event %llu)", static_cast<unsigned long long>(C.Busiest.Serial()));
		}
		std::printf("\n");
		std::printf("causes: faults - %llu dangling, %llu not an event, %llu not before their effect\n",
					static_cast<unsigned long long>(C.Dangling), static_cast<unsigned long long>(C.NotAnEvent),
					static_cast<unsigned long long>(C.NotBeforeEffect));

		// PER TYPE. Sorted by hash so two runs print the same order, then the
		// name looked up - and an unknown hash prints as `?<hex>` rather than
		// as nothing, which is 17.02's whole promise.
		struct Row
		{
			Vaelen::Hash64 Type = 0;
			uint64 Events = 0;
			uint64 WithCause = 0;
		};
		std::vector<Row> Rows;
		for (const Vaelen::Event& E : Log.All())
		{
			Row* Found = nullptr;
			for (Row& R : Rows)
			{
				if (R.Type == E.TypeHash)
				{
					Found = &R;
					break;
				}
			}
			if (Found == nullptr)
			{
				Rows.push_back(Row{E.TypeHash, 0u, 0u});
				Found = &Rows.back();
			}
			++Found->Events;
			if (E.Cause.IsValid())
			{
				++Found->WithCause;
			}
		}
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Type < B.Type; });

		std::printf("causes: %zu event type(s)\n", Rows.size());
		std::printf("| type | events | with a cause | share |\n|---|---|---|---|\n");
		for (const Row& R : Rows)
		{
			char Unknown[18];
			std::printf("| %s | %llu | %llu | %.1f%% |\n", Vaelen::NameOfEventType(R.Type, Unknown),
						static_cast<unsigned long long>(R.Events), static_cast<unsigned long long>(R.WithCause),
						100.0 * static_cast<double>(R.WithCause) / static_cast<double>(R.Events));
		}
		return 0;
	}

	int RunCauses(const Options& Opt)
	{
		std::FILE* F = std::fopen(Opt.Causes.c_str(), "rb");
		if (F == nullptr)
		{
			std::fprintf(stderr, "causes: cannot read %s\n", Opt.Causes.c_str());
			return 1;
		}
		std::fseek(F, 0, SEEK_END);
		const long Len = std::ftell(F);
		std::fseek(F, 0, SEEK_SET);
		std::vector<Vaelen::uint8> Bytes(static_cast<usize>(Len > 0 ? Len : 0));
		const usize Got = Bytes.empty() ? 0u : std::fread(Bytes.data(), 1, Bytes.size(), F);
		std::fclose(F);
		if (Bytes.empty() || Got != Bytes.size())
		{
			std::fprintf(stderr, "causes: short read of %s\n", Opt.Causes.c_str());
			return 1;
		}

		// THE WIRING COMES FROM THE FILE. A container carries the Options its
		// host declared (16.10's HOST section), so the world it is adopted into
		// is the world it was saved from, and a `--lively` forgotten on the
		// command line cannot make this a census of a different world.
		Vaelen::Run::CheckpointView View;
		const Vaelen::Run::CheckpointRefusal Read = Vaelen::Run::ReadCheckpoint(Bytes.data(), Bytes.size(), View);
		if (Read.Result != Vaelen::Run::CheckpointResult::Ok)
		{
			std::fprintf(stderr, "causes: %s is not a container this build reads - %s\n", Opt.Causes.c_str(),
						 Vaelen::Run::CheckpointResultToString(Read.Result));
			return 1;
		}
		Vaelen::Run::Options RO;
		if (!Vaelen::Run::ReadHostSection(View, RO))
		{
			std::fprintf(stderr, "causes: %s has no HOST section, so its wiring is unknown\n", Opt.Causes.c_str());
			return 1;
		}
		Vaelen::Run::Aelvor A(RO);
		const Vaelen::Run::Aelvor::AdoptResult R = A.Adopt(Bytes.data(), Bytes.size());
		if (R != Vaelen::Run::Aelvor::AdoptResult::Ok)
		{
			std::fprintf(stderr, "causes: REFUSED, %s\n", Vaelen::Run::Aelvor::AdoptResultToString(R));
			return 1;
		}
		char What[512];
		std::snprintf(What, sizeof(What), "%s (adopted, Generations %u, %u tiles, %u+%u years)", Opt.Causes.c_str(),
					  A.Generations(), RO.Size, RO.PreHistory, RO.Years);
		return PrintCensus(A.Instance(), What);
	}

	int RunCensus(const Options& Opt)
	{
		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Colony = Opt.Colony;
		RO.Play = true;
		RO.Stream = Opt.Stream;
		RO.Lively = Opt.Lively;
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::fprintf(stderr, "census: generation failed at %u\n", RO.Size);
			return 1;
		}
		char What[256];
		std::snprintf(What, sizeof(What), "a fresh world, %u tiles, %u+%u years, seed %016llx%s%s%s", RO.Size,
					  RO.PreHistory, RO.Years, static_cast<unsigned long long>(RO.Seed), RO.Stream ? ", stream" : "",
					  RO.Lively ? ", lively" : "", RO.Colony ? ", colony" : "");
		return PrintCensus(A.Instance(), What);
	}

	// ------------------------------------------------------------------
	// 17.06: THE INSPECTOR. What a container says about itself, read from
	// its bytes and from nothing else.
	//
	// The roadmap's own words at the close of Phase 16: "16.04's section table
	// and manifest are what makes listing saves without generating a world
	// possible; this phase stops there." `--save --sections` generates a world
	// FIRST and never opens a file, so until now nothing in the tree read a
	// container from disk except a test. This does, and it is the first thing
	// a person would reach for when a save misbehaves.
	//
	// NO WORLD IS CONSTRUCTED, let alone generated or adopted. `ReadHostSection`
	// decodes Options, `ReadRunSection` decodes a RunState and
	// `ReadStreamSection` decodes a tape, and none of the three needs an
	// Aelvor. That is the whole claim, and it is why a 2 GB save can be
	// described in the time it takes to read it.
	bool ReadWholeFile(const std::string& Path, std::vector<Vaelen::uint8>& Out, const char* Who)
	{
		std::FILE* F = std::fopen(Path.c_str(), "rb");
		if (F == nullptr)
		{
			std::fprintf(stderr, "%s: cannot read %s\n", Who, Path.c_str());
			return false;
		}
		std::fseek(F, 0, SEEK_END);
		const long Len = std::ftell(F);
		std::fseek(F, 0, SEEK_SET);
		Out.assign(static_cast<usize>(Len > 0 ? Len : 0), 0u);
		const usize Got = Out.empty() ? 0u : std::fread(Out.data(), 1, Out.size(), F);
		std::fclose(F);
		if (Out.empty() || Got != Out.size())
		{
			std::fprintf(stderr, "%s: short read of %s\n", Who, Path.c_str());
			return false;
		}
		return true;
	}

	int RunInspect(const Options& Opt)
	{
		std::vector<Vaelen::uint8> Bytes;
		if (!ReadWholeFile(Opt.Inspect, Bytes, "inspect"))
		{
			return 1;
		}

		Vaelen::Run::CheckpointView View;
		const Vaelen::Run::CheckpointRefusal Read = Vaelen::Run::ReadCheckpoint(Bytes.data(), Bytes.size(), View);
		if (Read.Result != Vaelen::Run::CheckpointResult::Ok)
		{
			// REFUSED WHOLE, never described in part. A truncated container
			// has a readable header and a table that points past the end, and
			// printing the header would look like a description of a save.
			//
			// And the one mistake this whole phase turns on gets its own
			// sentence: an IMAGE - the inner format, what Tests/Run/Golden
			// holds - begins "VAELEN" too, and a reader that only said BadMagic
			// would leave a person wondering which of two magics they had.
			const bool LooksLikeAnImage = Bytes.size() >= 8u && std::memcmp(Bytes.data(), "VAELEN", 6) == 0 &&
										  std::memcmp(Bytes.data(), Vaelen::Run::CheckpointMagic, 8) != 0;
			std::fprintf(stderr, "inspect: %s is not a container this build reads - %s%s\n", Opt.Inspect.c_str(),
						 Vaelen::Run::CheckpointResultToString(Read.Result),
						 LooksLikeAnImage ? " (it is a save IMAGE, the inner format, not a VAELENCP container "
											"around one)"
										  : "");
			if (Read.Result == Vaelen::Run::CheckpointResult::BadSectionTable ||
				Read.Result == Vaelen::Run::CheckpointResult::Truncated ||
				Read.Result == Vaelen::Run::CheckpointResult::Corrupt)
			{
				std::fprintf(stderr, "inspect: %zu bytes on disk; section %u is where it stopped describing them\n",
							 Bytes.size(), Read.Section);
			}
			return 1;
		}

		std::printf("inspect: %s, %zu bytes\n", Opt.Inspect.c_str(), Bytes.size());
		std::printf("inspect: container v%u, image v%u, flags %08x, seed %016llx, tick %llu, %llu events, %llu log "
					"bytes\n",
					View.Version, View.InnerFormat, View.Flags, static_cast<unsigned long long>(View.Seed),
					static_cast<unsigned long long>(View.Tick), static_cast<unsigned long long>(View.LogEvents),
					static_cast<unsigned long long>(View.LogBytes));

		std::printf("inspect: %zu section(s)\n", View.Sections.size());
		std::printf("| kind | offset | length | share | section digest |\n|---|---|---|---|---|\n");
		for (const Vaelen::Run::SectionEntry& E : View.Sections)
		{
			std::printf("| %s (%u) | %llu | %llu | %.1f%% | %016llx |\n", KindName(E.Kind), E.Kind,
						static_cast<unsigned long long>(E.Offset), static_cast<unsigned long long>(E.Length),
						100.0 * static_cast<double>(E.Length) / static_cast<double>(Bytes.size()),
						static_cast<unsigned long long>(E.Digest));
		}

		Vaelen::Hash64 Trailer = 0;
		if (TrailerOf(View, Trailer))
		{
			std::printf("inspect: image trailer %016llx (this is the state digest every other tool means)\n",
						static_cast<unsigned long long>(Trailer));
		}
		else
		{
			std::printf("inspect: no STATE section, so no image trailer\n");
		}

		Vaelen::Run::Options Host;
		if (Vaelen::Run::ReadHostSection(View, Host))
		{
			std::printf("inspect: host %u tiles, %u+%u years, seed %016llx, wiring:%s%s%s%s%s\n", Host.Size,
						Host.PreHistory, Host.Years, static_cast<unsigned long long>(Host.Seed),
						Host.Play ? " Play" : "", Host.Stream ? " Stream" : "", Host.Lively ? " Lively" : "",
						Host.Colony ? " Colony" : "",
						(Host.Play || Host.Stream || Host.Lively || Host.Colony) ? "" : " none");
		}
		else
		{
			std::printf("inspect: no HOST section - the wiring this was saved under is not recorded\n");
		}

		Vaelen::Run::Aelvor::RunState Run;
		if (Vaelen::Run::ReadRunSection(View, Run))
		{
			std::printf("inspect: run %s, detail %u, dug %u, eyes on region %u reach %u most %u, %zu near, %zu "
						"watched\n",
						Run.Begun ? "begun" : "not begun", Run.Detail, Run.Dug, Run.Eyes.Region, Run.Eyes.Reach,
						Run.Eyes.Most, Run.Near.size(), Run.Watched.size());
		}
		else
		{
			std::printf("inspect: no RUN section - a world restored from this parts company at the first look\n");
		}

		uint64 StreamLength = 0;
		if (View.Find(Vaelen::Run::SectionKind::Stream, StreamLength) == nullptr)
		{
			std::printf("inspect: no STREAM section - this save does not carry its own tape\n");
		}
		else
		{
			Player::InputStream Tape;
			Player::StartRules Rules;
			if (Vaelen::Run::ReadStreamSection(View, Tape, Rules))
			{
				std::printf("inspect: stream %zu day(s), %zu command(s), %zu taking(s), %zu look(s); header %u tiles "
							"%u+%u seed %016llx v%u; rules ages %u-%u bound %u ore %u\n",
							Tape.Days.size(), Tape.Commands.size(), Tape.Takings.size(), Tape.Looks.size(),
							Tape.Header.Size, Tape.Header.PreHistory, Tape.Header.Years,
							static_cast<unsigned long long>(Tape.Header.Seed), Tape.Header.Version, Rules.FromAge,
							Rules.ToAge, Rules.WantBound, Rules.PreferOre);
			}
			else
			{
				std::printf("inspect: a STREAM section of %llu bytes that this build cannot decode\n",
							static_cast<unsigned long long>(StreamLength));
			}
		}
		return 0;
	}

	int RunInspectDir(const Options& Opt)
	{
		// THROUGH THE STORE, and from a process that wrote nothing: this is
		// 17.03's second-process half. Run.StoreColdProcess proves a second
		// OBJECT lists what another wrote; this proves a second PROCESS does,
		// which is the thing a save browser actually is.
		VaelenHost::StdioCheckpointStore Store(Opt.InspectDir);
		const std::vector<Vaelen::Run::StoreEntry> Entries = Store.List();

		// THE STORE LISTS WHAT IS THERE; THE TOOL SAYS WHAT IT IS. Pointed at
		// the corpus directory, the first version of this printed README.md as
		// a checkpoint with tick 0, version 0 and a digest of sixteen zeros -
		// a row that looks like data and is not. The store is right to hand
		// the file back (it is not its job to judge bytes, Run.Store says so);
		// a browser is wrong to show it as a save. `ContainerVersion` is 0
		// exactly when `ReadCheckpoint` refused, and no container this build
		// writes has version 0.
		usize Checkpoints = 0;
		usize Others = 0;
		for (const Vaelen::Run::StoreEntry& E : Entries)
		{
			(E.ContainerVersion == 0u ? Others : Checkpoints) += 1u;
		}
		std::printf("inspect-dir: %zu checkpoint(s) and %zu other file(s) in %s\n", Checkpoints, Others,
					Opt.InspectDir.c_str());
		std::printf("| name | bytes | tick | container v | sections | image trailer |\n|---|---|---|---|---|---|\n");
		for (const Vaelen::Run::StoreEntry& E : Entries)
		{
			if (E.ContainerVersion == 0u)
			{
				std::printf("| %s | %llu | - | not a container | - | - |\n", E.Name.c_str(),
							static_cast<unsigned long long>(E.Bytes));
				continue;
			}
			std::printf("| %s | %llu | %llu | %u | %u | %016llx |\n", E.Name.c_str(),
						static_cast<unsigned long long>(E.Bytes), static_cast<unsigned long long>(E.Tick),
						E.ContainerVersion, E.SectionCount, static_cast<unsigned long long>(E.Digest));
		}
		return 0;
	}

	int RunStand(const Options& Opt)
	{
		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Play = true; // and nothing else: the host asks for exactly this
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::fprintf(stderr, "AELVOR: generation failed at %u x %u\n", RO.Size, RO.Size);
			return 1;
		}
		Player::StartRules Rules;
		Rules.WantBound = Opt.WantBound;
		Rules.FromAge = Opt.FromAge;
		Rules.ToAge = Opt.ToAge;
		Vaelen::Run::Door D(A, Rules);
		if (D.TakeUp() == 0)
		{
			std::fprintf(stderr, "AELVOR: the world offered nobody to take up\n");
			return 1;
		}

		WorldGen::RegionGraphCache Ways;
		WorldView Frame;
		LifeView Life;
		ChronicleView Told;
		PanelView Page;
		const auto Look = [&]()
		{
			TakeView(A.Instance(), A.Sources(), Frame);
			TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
			TakeChronicleView(A.Instance(), A.Sources(), Told);
			TakePanel(Frame, Life, Told, Page);
		};

		// What the month must contain: every one of the eight verbs at least
		// once, one Move the world took, one intent the world refused. None of
		// the three can be pinned to a chosen day - the page offers a verb only
		// when the hours are there, and a Move only when a neighbouring region
		// is DETAILED as well as adjacent, which the LOD decides and not this
		// tool. So each day does the still-missing work it can do today, and
		// the guard at the end says what the month never managed.
		static const Player::Intent Order[7] = {Player::Intent::Work, Player::Intent::Rest,	 Player::Intent::Eat,
												Player::Intent::Wait, Player::Intent::Speak, Player::Intent::Give,
												Player::Intent::Take};
		static const Player::Intent Filler[3] = {Player::Intent::Work, Player::Intent::Rest, Player::Intent::Eat};

		uint32 Meant[Player::IntentCount] = {};
		uint32 MovesTaken = 0;
		uint32 WorldRefused = 0;
		const auto Say = [&](Player::Intent Verb, uint32 Target, uint32 Amount)
		{
			Player::PlayerCommand What;
			const Player::Refusal Foreseen = Press(Page, Verb, Target, Amount, What);
			if (Foreseen != Player::Refusal::None)
			{
				// The page did not offer it, so nothing reached the world -
				// exactly what a key press does in 14.09. Not an error: the
				// day simply could not carry it, and another day will.
				return false;
			}
			D.Mean(What);
			++Meant[static_cast<usize>(Verb)];
			return true;
		};
		const auto Somebody = [&]() -> uint32 { return Life.CompanyCount > 0 ? Life.Company[0].Person : 0u; };

		const uint32 Days = 30;
		for (uint32 Day = 0; Day < Days; ++Day)
		{
			Look();
			// One intent the page offers and the WORLD refuses: a Speak aimed
			// at somebody who is not there comes back NoOne on
			// OrderStats::Refused, not at the door. Tried first because Speak
			// is cheap and the day's hours go to whatever else is meant on it.
			if (WorldRefused == 0 && Say(Player::Intent::Speak, 0xffffffffu, 0))
			{
				WorldRefused = A.Orders().Refused;
				Look();
			}
			// The Move, the day a neighbour is near enough to walk to.
			if (MovesTaken == 0 && Life.NearCount > 0 && Say(Player::Intent::Move, Life.Near[0], 0))
			{
				++MovesTaken;
				Look();
			}
			// Then whichever of the other seven is still unsaid, else a cheap
			// one, so that thirty days carry at least thirty intents.
			Player::Intent Next = Filler[Day % 3];
			for (const Player::Intent Verb : Order)
			{
				if (Meant[static_cast<usize>(Verb)] == 0)
				{
					Next = Verb;
					break;
				}
			}
			const bool Aimed =
				Next == Player::Intent::Speak || Next == Player::Intent::Give || Next == Player::Intent::Take;
			Say(Next, Aimed ? Somebody() : 0u, Next == Player::Intent::Give ? 1u : 0u);
			D.Day();
		}
		Look();

		// Refuse to write a stream that does not satisfy the row's shape: a
		// stand-in that silently misses a clause is worse than none at all.
		const Player::OrderStats Orders = A.Orders();
		const Player::InputStream& Tape = D.Stream();
		bool Shaped = true;
		const auto Want = [&Shaped](bool Held, const char* What)
		{
			if (!Held)
			{
				std::fprintf(stderr, "AELVOR: the stand-in stream is not shaped like a played month: %s\n", What);
				Shaped = false;
			}
		};
		Want(Tape.Days.size() == Days, "thirty day turns");
		Want(Tape.Commands.size() >= Days, "at least thirty intents");
		Want(Orders.Refused >= 1, "at least one intent the world refused");
		// The row asks for the Move among the TAKEN, so read the record's own
		// verdict rather than trusting that the page offering it was enough.
		uint32 MovesTook = 0;
		for (const Player::Recorded& One : Tape.Commands)
		{
			MovesTook +=
				(One.Command.Kind == static_cast<uint8>(Player::Intent::Move) && One.Verdict == Player::Refusal::None)
					? 1u
					: 0u;
		}
		Want(MovesTaken >= 1, "a Move the page offered");
		// The door's verdict, which Stream.h says is exactly that: None means
		// QUEUED, not "the world applied it". What makes this a Move the world
		// could take is that Press offered it at all, which needs a neighbour
		// in Life.Near - adjacent AND detailed. The world's own verdict on it
		// is in the taken/refused counts of the line this month prints.
		Want(MovesTook >= 1, "a Move the page offered and the door queued");
		Want(Life.Person != 0, "somebody still played at the end");
		for (usize k = 1; k < Player::IntentCount; ++k)
		{
			Want(Meant[k] >= 1, Player::IntentName(static_cast<Player::Intent>(k)));
		}
		if (!Shaped)
		{
			return 1;
		}

		const std::string Text = Player::EncodeStream(Tape);
		std::FILE* File = std::fopen(Opt.Stand.c_str(), "wb");
		if (File == nullptr)
		{
			std::fprintf(stderr, "AELVOR: cannot write %s\n", Opt.Stand.c_str());
			return 1;
		}
		const usize Written = std::fwrite(Text.data(), 1, Text.size(), File);
		const bool Closed = std::fclose(File) == 0;
		if (Written != Text.size() || !Closed)
		{
			std::fprintf(stderr, "AELVOR: %s is incomplete (%zu of %zu bytes)\n", Opt.Stand.c_str(), Written,
						 Text.size());
			return 1;
		}
		VAELEN_LOG_INFO(LogAtlas,
						"AELVOR %u: stand-in stream of %u day(s), %zu intent(s), %u refused by the world, "
						"%zu bytes to %s",
						RO.Size, static_cast<uint32>(Tape.Days.size()), Tape.Commands.size(), Orders.Refused,
						Text.size(), Opt.Stand.c_str());
		return 0;
	}

	/// 14.10: what the replay came to, in the words the engine's own host uses.
	///
	/// THE TWO LINES. The format strings below are
	/// Source/VaelenGame/Private/VaelenPlayCommands.cpp's, byte for byte with
	/// only TEXT() dropped, and every argument comes from the same field of
	/// the same view. That is the whole point of the clause: the engine prints
	/// them after playing a month by hand and this prints them after replaying
	/// the stream that month wrote, and the bytes after the log prefix are
	/// compared. Change one of the four here and the other must change too -
	/// they are quoted in ADR-0138 for that reason.
	///
	/// printf and not VAELEN_LOG_INFO: the log prefixes every line with
	/// "[Info] LogAtlas: " and truncates at 2048 bytes, and this is a line a
	/// checker matches whole.
	void PlayedLines(const Player::InputStream& S, const Vaelen::Run::ReplayReport& R, const LifeView& Life,
					 Hash64 PanelDigest)
	{
		// The replay's own verdict first: how many of the recorded answers the
		// fresh world gave again, and how many days it turned.
		// Answered - Wrong, with the mismatched TAKINGS taken out of Wrong
		// first: they are not answers, and a walk with no intents and three of
		// them made this underflow to 4294967293 - a number that read like
		// corruption and was arithmetic.
		const uint32 Off = R.Wrong - R.WrongTakings;
		std::printf("replay: %u of %u answered identically, %u of %u days\n",
					R.Answered - (Off > R.Answered ? R.Answered : Off), R.Answered, R.Days,
					static_cast<uint32>(S.Days.size()));

		std::printf("LogVaelenPlay: AELVOR %u seed %012llx: played %s (person %u, region %u) %u days, %u intents "
					"(%u taken, %u refused by the world, %u dropped at the door), state %016llx, log %016llx, "
					"life %016llx, panel %016llx\n",
					static_cast<unsigned>(S.Header.Size), static_cast<unsigned long long>(S.Header.Seed), Life.Name,
					static_cast<unsigned>(Life.Person), static_cast<unsigned>(Life.Region),
					static_cast<unsigned>(S.Days.size()), static_cast<unsigned>(S.Commands.size()),
					static_cast<unsigned>(Life.Taken), static_cast<unsigned>(Life.Refused),
					static_cast<unsigned>(Life.Dropped), static_cast<unsigned long long>(R.State),
					static_cast<unsigned long long>(R.Log), static_cast<unsigned long long>(R.Life),
					static_cast<unsigned long long>(PanelDigest));

		// The verbs, counted from the stream itself: what was MEANT, whatever
		// the world made of it. The letters are a mnemonic; the numbers are
		// counts (14.08's done section says which reading this is).
		uint32 Tally[Player::IntentCount] = {};
		for (const Player::Recorded& One : S.Commands)
		{
			if (One.Command.Kind < static_cast<uint8>(Player::Intent::Count))
			{
				++Tally[One.Command.Kind];
			}
		}
		const auto Of = [&Tally](Player::Intent Kind)
		{ return static_cast<unsigned>(Tally[static_cast<usize>(Kind)]); };
		std::printf("LogVaelenPlay: verbs work %u rest %u eat %u wait %u speak %u give %u take %u move %u\n",
					Of(Player::Intent::Work), Of(Player::Intent::Rest), Of(Player::Intent::Eat),
					Of(Player::Intent::Wait), Of(Player::Intent::Speak), Of(Player::Intent::Give),
					Of(Player::Intent::Take), Of(Player::Intent::Move));
	}

	/// --replay FILE and --empty (14.03): a played Run, the stream through its
	/// door, and what it came to. Writes a REPLAY document and not an atlas -
	/// "kind":"replay": the run, the stream's counts, the report of the replay
	/// (answered, wrong, days, the three digests), and the three view digests
	/// of the world after it. The world is the STREAM's (its header names the
	/// seed, the size, the years) and not the command line's; --colony is the
	/// host's configuration and is taken from the command line, as the start
	/// rules are (the defaults). --empty is the same with no stream at all:
	/// the Play wiring with nobody taken up, the baseline a played stream is
	/// compared against. Exit 1 when the stream is of another world or any
	/// answer differed, so a CTest entry can hold a stream to its world.
	int RunReplay(const Options& Opt)
	{
		Player::InputStream S;
		Player::StreamReport Report;
		Vaelen::Run::Options RO;
		RO.Size = Opt.Size;
		RO.PreHistory = Opt.PreHistory;
		RO.Years = Opt.Years;
		RO.Seed = Opt.Seed;
		RO.Colony = Opt.Colony;
		RO.Play = true;
		RO.Stream = Opt.Stream; // told, not read: see Options::Stream
		if (!Opt.Replay.empty())
		{
			std::string Text;
			if (!ReadFile(Opt.Replay, Text))
			{
				std::fprintf(stderr, "AELVOR: cannot read %s\n", Opt.Replay.c_str());
				return 1;
			}
			if (!Player::DecodeStream(Text, S, Report))
			{
				std::fprintf(stderr,
							 "AELVOR: %s is not a stream this build reads (header bad %u, version bad %u, %u lines)\n",
							 Opt.Replay.c_str(), Report.HeaderBad, Report.VersionBad, Report.Lines);
				return 1;
			}
			RO.Size = S.Header.Size;
			RO.PreHistory = S.Header.PreHistory;
			RO.Years = S.Header.Years;
			RO.Seed = S.Header.Seed;
		}
		const auto Started = std::chrono::steady_clock::now();
		Vaelen::Run::Aelvor A(RO);
		if (!A.Begin())
		{
			std::fprintf(stderr, "AELVOR: generation failed at %u x %u\n", RO.Size, RO.Size);
			return 1;
		}
		if (Opt.Replay.empty())
		{
			S.Header = A.Header();
		}
		Player::StartRules Host;
		Host.WantBound = Opt.WantBound;
		Host.FromAge = Opt.FromAge;
		Host.ToAge = Opt.ToAge;
		const Vaelen::Run::ReplayReport R = Vaelen::Run::Replay(A, S, Host);
		const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Started).count();

		WorldView Frame;
		MapView Ground;
		NetView Net;
		TakeView(A.Instance(), A.Sources(), Frame);
		TakeNetView(A.Instance(), A.Sources(), Net);
		TakeMapView(A.Instance(), A.Sources(), Ground);
		const ViewStats FrameStats = MeasureView(Frame);
		const MapStats GroundStats = MeasureMapView(Ground);
		const NetStats NetStats_ = MeasureNetView(Net);

		// 14.06: the first screen of the world the replay came to, as the rows
		// a widget draws. Printed, not written to the JSON: it is a page for a
		// person to read and for a CTest to match, and its last row is its own
		// digest - the same digest View.Panel freezes.
		if (Opt.Panel)
		{
			WorldGen::RegionGraphCache Ways;
			LifeView Life;
			ChronicleView Told;
			PanelView Page;
			TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
			TakeChronicleView(A.Instance(), A.Sources(), Told);
			TakePanel(Frame, Life, Told, Page);
			std::vector<char> Rows(PanelTextBytes, '\0');
			Lines(Page, Rows.data(), PanelTextBytes);
			std::printf("%s\n", Rows.data());
			PlayedLines(S, R, Life, MeasurePanel(Page).Digest);
		}

		Json J;
		J.Reserve(4096);
		J.Put("{\"vaelen\":\"0.0.1\",\"world\":\"AELVOR\",\"schema\":1,\"kind\":\"replay\",\n\"run\":{");
		J.Put("\"seed\":");
		J.Hex(RO.Seed);
		J.Put(",");
		J.Field("size", RO.Size);
		J.Put(",");
		J.Field("prehistory", RO.PreHistory);
		J.Put(",");
		J.Field("years", RO.Years);
		J.Put(",");
		J.Field("colony", Opt.Colony ? 1u : 0u);
		J.Put(",");
		J.Field("tick", Frame.Tick);
		J.Put(",");
		J.Field("year", Frame.Year);
		J.Put(",\"seconds\":");
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.2f", Seconds);
			J.Put(Buffer);
		}
		J.Put("},\n\"stream\":{");
		J.Field("commands", S.Commands.size());
		J.Put(",");
		J.Field("takings", S.Takings.size());
		J.Put(",");
		J.Field("days", S.Days.size());
		J.Put(",");
		J.Field("lines", Report.Lines);
		J.Put(",");
		J.Field("badLines", Report.BadLines);
		J.Put("},\n\"replay\":{");
		J.Field("answered", R.Answered);
		J.Put(",");
		J.Field("wrong", R.Wrong);
		J.Put(",");
		J.Field("days", R.Days);
		J.Put(",");
		J.Field("takings", R.Takings);
		J.Put(",");
		J.Field("left", R.Left);
		J.Put(",");
		J.Field("refused", R.Refused);
		J.Put(",\"byKind\":[");
		for (usize k = 0; k < Player::IntentCount; ++k)
		{
			if (k != 0)
			{
				J.Put(",");
			}
			J.Unsigned(R.ByKind[k]);
		}
		J.Put("],\"state\":");
		J.Hex(R.State);
		J.Put(",\"log\":");
		J.Hex(R.Log);
		J.Put(",\"life\":");
		J.Hex(R.Life);
		J.Put("},\n\"frame\":{");
		J.Field("people", Frame.People);
		J.Put(",");
		J.Field("played", Frame.Played);
		J.Put(",");
		J.Field("regions", FrameStats.Regions);
		J.Put(",\"digest\":");
		J.Hex(FrameStats.Digest);
		J.Put("},\n\"ground\":{");
		J.Field("tiles", GroundStats.Tiles);
		J.Put(",");
		J.Field("land", GroundStats.Land);
		J.Put(",\"digest\":");
		J.Hex(GroundStats.Digest);
		J.Put("},\n\"network\":{");
		J.Field("routes", NetStats_.Routes);
		J.Put(",");
		J.Field("open", NetStats_.Open);
		J.Put(",\"digest\":");
		J.Hex(NetStats_.Digest);
		J.Put("}\n}\n");

		std::FILE* File = std::fopen(Opt.Out.c_str(), "wb");
		if (File == nullptr)
		{
			std::fprintf(stderr, "AELVOR: cannot write %s\n", Opt.Out.c_str());
			return 1;
		}
		const usize Written = std::fwrite(J.Text.data(), 1, J.Text.size(), File);
		const bool Closed = std::fclose(File) == 0;
		if (Written != J.Text.size() || !Closed)
		{
			std::fprintf(stderr, "AELVOR: %s is incomplete (%zu of %zu bytes)\n", Opt.Out.c_str(), Written,
						 J.Text.size());
			return 1;
		}
		VAELEN_LOG_INFO(LogAtlas,
						"AELVOR %ux%u, seed 0x%llx: %s - %u answered, %u wrong, %u day(s), %u taking(s), %u left, "
						"refused %u; state %016llx, log %016llx, life %016llx; frame %016llx, ground %016llx; "
						"%u bytes to %s in %.2f s",
						RO.Size, RO.Size, static_cast<unsigned long long>(RO.Seed),
						Opt.Replay.empty() ? "the empty play" : Opt.Replay.c_str(), R.Answered, R.Wrong, R.Days,
						R.Takings, R.Left, R.Refused, static_cast<unsigned long long>(R.State),
						static_cast<unsigned long long>(R.Log), static_cast<unsigned long long>(R.Life),
						static_cast<unsigned long long>(FrameStats.Digest),
						static_cast<unsigned long long>(GroundStats.Digest), static_cast<uint32>(J.Text.size()),
						Opt.Out.c_str(), Seconds);
		return (R.Refused != 0 || R.Wrong != 0) ? 1 : 0;
	}

	int RunAtlas(int Argc, char** Argv)
	{
		Options Opt;
		if (!ParseOptions(Argc, Argv, Opt))
		{
			Usage();
			return 2;
		}
		if (!Opt.SaveTo.empty())
		{
			return RunSaveTo(Opt);
		}
		if (!Opt.LoadFrom.empty())
		{
			return RunLoadFrom(Opt);
		}
		if (!Opt.DeathWalk.empty())
		{
			return RunDeathWalk(Opt);
		}
		if (!Opt.SaveFuzz.empty())
		{
			return RunSaveFuzz(Opt);
		}
		if (Opt.Save)
		{
			return RunSave(Opt);
		}
		if (!Opt.Golden.empty())
		{
			return RunGolden(Opt);
		}
		if (!Opt.Containers.empty())
		{
			return RunContainers(Opt);
		}
		if (!Opt.Causes.empty())
		{
			return RunCauses(Opt);
		}
		if (Opt.Census)
		{
			return RunCensus(Opt);
		}
		if (!Opt.Inspect.empty())
		{
			return RunInspect(Opt);
		}
		if (!Opt.InspectDir.empty())
		{
			return RunInspectDir(Opt);
		}
		if (!Opt.Gate.empty())
		{
			return RunGate(Opt);
		}
		if (!Opt.Stand.empty())
		{
			return RunStand(Opt);
		}
		if (!Opt.Walk.empty())
		{
			return RunWalk(Opt);
		}
		if (!Opt.Replay.empty() || Opt.Empty)
		{
			return RunReplay(Opt);
		}

		const auto Started = std::chrono::steady_clock::now();
		std::vector<Kept> Timeline;
		KernelRun Run(Opt.Seed, Opt.Colony, Opt.Chronicle);
		WorldGenConfig Gen;
		Gen.Width = Opt.Size;
		Gen.Height = Opt.Size;
		// Seed the world and run NOTHING. Every year then goes through Run, where
		// a frame can be taken between them, and the pre-history is no longer a
		// black box the timeline cannot see into.
		//
		// This is only equivalent because the years are the same years: what
		// changes a run is not which call ticks the clock but WHEN detail is
		// requested, and that still happens after exactly PreHistory years. The
		// first attempt moved it to year zero, where nobody has spread yet and no
		// region is worth detailing, and produced a different world - 27564 alive
		// against 36374. The digests below are checked against the split run.
		if (!Run.Ages.Generate(Gen, 0, false))
		{
			std::fprintf(stderr, "AELVOR: generation failed at %u x %u\n", Opt.Size, Opt.Size);
			return 1;
		}
		// The founding centuries, in steps when a timeline is wanted.
		Step(Run, Timeline, Opt, Opt.PreHistory);

		// Detail goes where the colony will be when one is asked for, and to the
		// busiest region otherwise. Only --colony changes it, so a run without
		// the flag simulates exactly the world every run before this one did.
		uint32 Detail = Run.Busiest();
		if (Opt.Colony)
		{
			const uint32 Seamed = Run.BusiestWithOre();
			Detail = Seamed != 0 ? Seamed : Detail;
		}
		RequestDetail(Run.Instance, Run.Lod, Detail);
		// The colony goes where the people are, and on the region the world is
		// already simulating person by person - a colony of aggregates has no
		// hands on the rock, and 13.08a's test paid for that lesson.
		uint32 Dug = 0;
		if (Opt.Colony && Detail != 0 && FoundColony(Run.Instance, Run.Ages.Types(), Run.Pit, Detail))
		{
			Dug = Detail;
		}
		Step(Run, Timeline, Opt, Opt.Years);
		const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Started).count();

		WorldView Frame;
		MapView Ground;
		NetView Net;
		TakeView(Run.Instance, Run.Sources(), Frame);
		TakeNetView(Run.Instance, Run.Sources(), Net);
		if (Opt.Tiles)
		{
			TakeMapView(Run.Instance, Run.Sources(), Ground);
		}
		// The chronicle, as lines with a year and a place on them. The kernel's
		// own ExportChronicleWithEconomy writes the same sentences as one block
		// of text; this walks the records instead, because a page that puts a
		// line on a timeline needs to know which year it belongs to.
		struct Told
		{
			uint64 Event = 0; ///< the event's persistent id: what a cause chain is asked for
			uint32 Year = 0;
			uint32 Region = 0;
			std::string Line;
			std::vector<std::string> Because; ///< the cause chain, nearest cause first
		};
		std::vector<Told> Chronicle;
		if (Opt.Chronicle)
		{
			std::vector<History::RecordInfo> Records;
			Run.Instance.Components()
				.GetPool(Run.Ages.Types().History.Record)
				.ForEach([&](EntityHandle, const History::RecordInfo& R) { Records.push_back(R); });
			std::sort(Records.begin(), Records.end(), [](const History::RecordInfo& A, const History::RecordInfo& B)
					  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
			const PersonIndex Index = BuildPersonIndex(Run.Instance, Run.Persons);
			Chronicle.reserve(Records.size());
			for (const History::RecordInfo& R : Records)
			{
				Told T;
				T.Event = R.Event;
				T.Year = static_cast<uint32>(R.Tick / History::TicksPerYear);
				T.Region = R.Region;
				const Event* E = History::FindEvent(Run.Instance.Log(), PersistentId{R.Event});
				if (E != nullptr)
				{
					DescribeEconomyEvent(Run.Instance, Run.Ages.Types(), Run.Trades, *E, T.Line, &Index);
				}
				else
				{
					History::DescribeRecord(Run.Instance, Run.Ages.Types(), R, T.Line);
				}
				// The why. CauseChain walks the EVENT LOG and not the records, so
				// it can pass through causes nobody thought worth chronicling -
				// which is the point: the reason a loaf was dear is a harvest, and
				// the reason for the harvest is a drought, and only one of the
				// three was ever history.
				if (Opt.Why && E != nullptr)
				{
					std::vector<const Event*> Chain;
					History::CauseChain(Run.Instance.Log(), E->Id, Chain, 8);
					for (usize L = 1; L < Chain.size(); ++L)
					{
						std::string Step;
						DescribeEconomyEvent(Run.Instance, Run.Ages.Types(), Run.Trades, *Chain[L], Step, &Index);
						if (!Step.empty())
						{
							T.Because.push_back(std::move(Step));
						}
					}
				}
				Chronicle.push_back(std::move(T));
			}
		}

		const ViewStats FrameStats = MeasureView(Frame);
		const MapStats GroundStats = MeasureMapView(Ground);
		const NetStats NetStats_ = MeasureNetView(Net);

		Json J;
		// Four numbers a tile, seven-odd characters each, plus the regions.
		J.Reserve(Ground.Tiles.size() * 32u + Frame.Regions.size() * 128u + Net.Routes.size() * 96u +
				  Timeline.size() * (Frame.Regions.size() * 32u + Net.Routes.size() * 20u) + 4096u);
		J.Put("{\"vaelen\":\"0.0.1\",\"world\":\"AELVOR\",\"schema\":1,\n\"run\":{");
		J.Put("\"seed\":");
		J.Hex(Opt.Seed);
		J.Put(",");
		J.Field("size", Opt.Size);
		J.Put(",");
		J.Field("prehistory", Opt.PreHistory);
		J.Put(",");
		J.Field("years", Opt.Years);
		J.Put(",");
		J.Field("tick", Frame.Tick);
		J.Put(",");
		J.Field("year", Frame.Year);
		J.Put(",\"seconds\":");
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.2f", Seconds);
			J.Put(Buffer);
		}
		J.Put("},\n\"frame\":{");
		J.Field("width", Frame.Width);
		J.Put(",");
		J.Field("height", Frame.Height);
		J.Put(",");
		J.Field("people", Frame.People);
		J.Put(",");
		J.Field("played", Frame.Played);
		J.Put(",");
		J.Field("regions", FrameStats.Regions);
		J.Put(",");
		J.Field("peopled", FrameStats.Peopled);
		J.Put(",");
		J.Field("detailed", FrameStats.Detailed);
		J.Put(",");
		J.Field("named", FrameStats.Named);
		J.Put(",");
		J.Field("bytes", FrameStats.Bytes);
		J.Put(",\"digest\":");
		J.Hex(FrameStats.Digest);
		J.Put("},\n\"ground\":{");
		J.Field("width", Ground.Width);
		J.Put(",");
		J.Field("height", Ground.Height);
		J.Put(",");
		J.Field("tiles", GroundStats.Tiles);
		J.Put(",");
		J.Field("land", GroundStats.Land);
		J.Put(",");
		J.Field("coast", GroundStats.Coast);
		J.Put(",");
		J.Field("water", GroundStats.Water);
		J.Put(",");
		J.Field("regions", GroundStats.Regions);
		J.Put(",");
		J.Field("bytes", GroundStats.Bytes);
		J.Put(",\"digest\":");
		J.Hex(GroundStats.Digest);
		// Elevation is Fix64 raw shifted right 16, so a value over 65536 is the
		// height in units. The scale is written down rather than assumed.
		J.Put(",\"elevationScale\":65536");
		// One count per biome, in the enum's own order, so that a reader can
		// see at a glance whether a world has weather or is all one thing.
		// Written because nothing wrote it: a wrong biome table lived in the
		// engine drawer unseen, and no output anywhere could have contradicted it.
		J.Put(",\"biomes\":[");
		for (Vaelen::uint32 b = 0; b < Vaelen::View::BiomeKinds; ++b)
		{
			if (b != 0)
			{
				J.Put(",");
			}
			J.Number(GroundStats.Biomes[b]);
		}
		J.Put("]");
		J.Put("},\n\"network\":{");
		J.Field("routes", NetStats_.Routes);
		J.Put(",");
		J.Field("open", NetStats_.Open);
		J.Put(",");
		J.Field("colonies", NetStats_.Colonies);
		J.Put(",");
		J.Field("hands", NetStats_.Hands);
		J.Put(",");
		J.Field("bytes", NetStats_.Bytes);
		J.Put(",\"digest\":");
		J.Hex(NetStats_.Digest);
		J.Put("},\n\"regions\":[");
		for (usize I = 0; I < Frame.Regions.size(); ++I)
		{
			const RegionView& R = Frame.Regions[I];
			J.Put(I == 0 ? "\n" : ",\n");
			J.Put("{");
			J.Field("index", R.Index);
			J.Put(",");
			J.Field("tile", R.CentroidTile);
			J.Put(",");
			J.Field("tiles", R.Tiles);
			J.Put(",");
			J.Field("biome", R.Biome);
			J.Put(",");
			// Shifted the same 16 bits the ground is, so ONE elevationScale
			// describes the whole file. A region in Fix64 raw beside tiles in
			// Q16.16 is two scales in one document and a factor of 65536 waiting
			// to be drawn as a mountain range.
			J.Signed("elevation", R.Elevation >> 16);
			J.Put(",");
			J.Field("people", R.People);
			J.Put(",");
			J.Field("bound", R.Bound);
			J.Put(",");
			J.Field("settlement", R.Settlement);
			J.Put(",");
			J.Field("roads", R.Roads);
			J.Put(",");
			J.Field("names", R.Names);
			J.Put(",");
			J.Field("detailed", R.Detailed);
			J.Put("}");
		}
		J.Put("\n],\n\"routes\":[");
		for (usize I = 0; I < Net.Routes.size(); ++I)
		{
			const RouteView& R = Net.Routes[I];
			J.Put(I == 0 ? "\n" : ",\n");
			J.Put("{");
			J.Field("index", R.Index);
			J.Put(",");
			J.Field("from", R.From);
			J.Put(",");
			J.Field("to", R.To);
			J.Put(",");
			J.Field("open", R.Open);
			J.Put(",");
			J.Field("idle", R.Idle);
			J.Put(",");
			J.Field("openings", R.Openings);
			J.Put(",");
			J.Field("carried", R.Carried);
			J.Put("}");
		}
		J.Put("\n],\n\"colonies\":[");
		for (usize I = 0; I < Net.Colonies.size(); ++I)
		{
			const ColonyView& C = Net.Colonies[I];
			J.Put(I == 0 ? "\n" : ",\n");
			J.Put("{");
			J.Field("region", C.Region);
			J.Put(",");
			J.Field("hands", C.Hands);
			J.Put(",");
			J.Field("lifted", C.Lifted);
			J.Put("}");
		}
		J.Put("\n],\n\"timeline\":{");
		J.Field("every", Opt.Every);
		J.Put(",");
		J.Field("frames", static_cast<uint32>(Timeline.size()));
		// Flat arrays with a stride rather than an object a region: at a frame a
		// decade over four centuries that is the difference between a file a
		// browser opens and one it thinks about.
		J.Put(",\"regionStride\":5,\"routeStride\":3,\"keep\":[");
		for (usize F = 0; F < Timeline.size(); ++F)
		{
			const Kept& K = Timeline[F];
			J.Put(F == 0 ? "\n" : ",\n");
			J.Put("{");
			J.Field("year", K.Frame.Year);
			J.Put(",");
			J.Field("people", K.Frame.People);
			J.Put(",");
			J.Field("open", K.Net.Open);
			J.Put(",\"regions\":[");
			for (usize R = 0; R < K.Frame.Regions.size(); ++R)
			{
				const RegionView& V = K.Frame.Regions[R];
				if (R != 0)
				{
					J.Put(",");
				}
				J.Unsigned(V.Index);
				J.Put(",");
				J.Unsigned(V.People);
				J.Put(",");
				J.Unsigned(V.Bound);
				J.Put(",");
				J.Unsigned(V.Settlement);
				J.Put(",");
				J.Unsigned(V.Roads);
			}
			J.Put("],\"routes\":[");
			for (usize R = 0; R < K.Net.Routes.size(); ++R)
			{
				const RouteView& V = K.Net.Routes[R];
				if (R != 0)
				{
					J.Put(",");
				}
				J.Unsigned(V.Index);
				J.Put(",");
				J.Unsigned(V.Open);
				J.Put(",");
				J.Unsigned(V.Carried);
			}
			J.Put("]}");
		}
		J.Put("\n]},\n\"chronicle\":[");
		for (usize C = 0; C < Chronicle.size(); ++C)
		{
			J.Put(C == 0 ? "\n" : ",\n");
			J.Put("{");
			J.Field("id", Chronicle[C].Event);
			J.Put(",");
			J.Field("year", Chronicle[C].Year);
			J.Put(",");
			J.Field("region", Chronicle[C].Region);
			J.Put(",\"said\":");
			J.Str(Chronicle[C].Line);
			if (!Chronicle[C].Because.empty())
			{
				J.Put(",\"because\":[");
				for (usize B = 0; B < Chronicle[C].Because.size(); ++B)
				{
					if (B != 0)
					{
						J.Put(",");
					}
					J.Str(Chronicle[C].Because[B]);
				}
				J.Put("]");
			}
			J.Put("}");
		}
		J.Put("\n],\n\"tiles\":{");
		// Four parallel arrays in tile order rather than one object per tile: an
		// object per tile is nine times the bytes and says nothing more.
		auto Column = [&J, &Ground](const char* Name, int Which)
		{
			J.Put("\"");
			J.Put(Name);
			J.Put("\":[");
			for (usize I = 0; I < Ground.Tiles.size(); ++I)
			{
				if (I != 0)
				{
					J.Put(",");
				}
				const TileView& T = Ground.Tiles[I];
				switch (Which)
				{
				case 0:
					J.Unsigned(T.Biome);
					break;
				case 1:
					J.Unsigned(T.Ground);
					break;
				case 2:
					J.Unsigned(T.Region);
					break;
				default:
					J.Number(T.Elevation);
					break;
				}
			}
			J.Put("]");
		};
		Column("biome", 0);
		J.Put(",\n");
		Column("ground", 1);
		J.Put(",\n");
		Column("region", 2);
		J.Put(",\n");
		Column("elevation", 3);
		J.Put("}\n}\n");

		std::FILE* File = std::fopen(Opt.Out.c_str(), "wb");
		if (File == nullptr)
		{
			std::fprintf(stderr, "AELVOR: cannot write %s\n", Opt.Out.c_str());
			return 1;
		}
		const usize Written = std::fwrite(J.Text.data(), 1, J.Text.size(), File);
		const bool Closed = std::fclose(File) == 0;
		if (Written != J.Text.size() || !Closed)
		{
			std::fprintf(stderr, "AELVOR: %s is incomplete (%zu of %zu bytes)\n", Opt.Out.c_str(), Written,
						 J.Text.size());
			return 1;
		}

		VAELEN_LOG_INFO(LogAtlas,
						"AELVOR %ux%u, seed 0x%llx: year %u, %u land tiles, %u regions (%u peopled, %u detailed), "
						"%u living, %u roads open of %u, %u colonies (%u hands on region %u), %u frames kept, "
						"%u things remembered, %u bytes to %s. Simulated in %.2f s.",
						Opt.Size, Opt.Size, static_cast<unsigned long long>(Opt.Seed), Frame.Year, GroundStats.Land,
						FrameStats.Regions, FrameStats.Peopled, FrameStats.Detailed, Frame.People, NetStats_.Open,
						NetStats_.Routes, NetStats_.Colonies, NetStats_.Hands, Dug,
						static_cast<uint32>(Timeline.size()), static_cast<uint32>(Chronicle.size()),
						static_cast<uint32>(J.Text.size()), Opt.Out.c_str(), Seconds);
		return 0;
	}
} // namespace
