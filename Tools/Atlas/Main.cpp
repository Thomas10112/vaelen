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
#include "Vaelen/Run/Door.h"
#include "Vaelen/Core/Version.h"
#include "Vaelen/Sim/PreHistory.h"
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
		/// The four digests the host printed, as `state %016llx, log %016llx,
		/// life %016llx, panel %016llx` - the tail of the line
		/// Vaelen.Stream.Write logs. Given, the gate JUDGES clause (b)'s other
		/// half; withheld, it says it is not judging it. It does not guess.
		std::string Expect;
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
			else if (std::strcmp(Arg, "--golden") == 0 && HasValue)
			{
				Out.Golden = Argv[++I];
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
		if (!Opt.Golden.empty())
		{
			return RunGolden(Opt);
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
