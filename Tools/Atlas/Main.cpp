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
#include "Vaelen/Politics/Polities.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
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
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Net.h"

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
		uint32 Every = 0; ///< years between kept frames; 0 = keep only the last
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
		std::fprintf(stderr, "VaelenAtlas - generates AELVOR and writes what can be drawn of it.\n"
							 "  --size N        map side in tiles (32..512, default 128)\n"
							 "  --prehistory N  years run by the pre-history (default 300)\n"
							 "  --years N       years run after it (default 120)\n"
							 "  --seed V        decimal or 0x hex (default 0x41454c564f52)\n"
							 "  --out PATH      where to write the JSON (default aelvor.json)\n"
							 "  --no-tiles      write the regions only, not the ground\n"
							 "  --colony        found a mining colony on the busiest region\n"
							 "  --every N       also keep a frame every N years, for a timeline\n"
							 "  --chronicle     remember what happened, and write it out in words\n"
							 "  --why           and why: the cause of each thing, back to its root\n");
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
	int Run(int Argc, char** Argv);
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
	const int Code = Run(Argc, Argv);
	Log::RemoveSink(&Console);
	return Code;
}

namespace
{
	int Run(int Argc, char** Argv)
	{
		Options Opt;
		if (!ParseOptions(Argc, Argv, Opt))
		{
			Usage();
			return 2;
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
