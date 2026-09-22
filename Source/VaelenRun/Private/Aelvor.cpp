// VAELEN - VaelenRun
// Phase 14 task 14.03: one wiring of a played AELVOR.
//
// The constructor of Kernel below IS the wiring, and Tools/check_world_wiring.py
// reads this file as its fourth column: the type sets declared, the systems
// built with make_unique and added to the schedule, and the listeners
// attached, each in order, against the Atlas and the two actors. Keep those
// three idioms exactly as the Atlas writes them, or the guard goes blind to
// this file - and do not spell them out in a comment, which is how its first
// run found sixteen declarations here instead of fifteen.
//
// STATUS: PROTOTYPE (Phase 14) - Tests/Run/Test_Aelvor.cpp, Tests/Run/Test_Door.cpp; Kernel.WorldWiring
#include "Vaelen/Run/Aelvor.h"

#include "Vaelen/Run/Checkpoint.h"

#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Gameplay/Judgement.h"
#include "Vaelen/Player/Doings.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Deposits.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/WorldGen.h"
#include "Vaelen/Society/SocietyHistory.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Vaelen::Run
{
	using namespace Vaelen::Colony;
	using namespace Vaelen::Economy;
	using namespace Vaelen::Gameplay;
	using namespace Vaelen::History;
	using namespace Vaelen::Politics;
	using namespace Vaelen::Population;
	using namespace Vaelen::Society;
	using namespace Vaelen::WorldGen;
	// Not Vaelen::Player at this scope: its free functions share names with
	// the methods of Aelvor (Submit, Release) and are called qualified there.

	struct Aelvor::Kernel
	{
		static WorldConfig Config(uint64 InSeed)
		{
			WorldConfig C;
			C.Seed = InSeed;
			return C;
		}

		explicit Kernel(const Options& In) : Given(In), Instance(Config(In.Seed)), Ages(Instance, PreHistoryRules{})
		{
			using namespace Vaelen::Player;

			// ── THE ATLAS WIRING, VERBATIM (Tools/Atlas/Main.cpp, KernelRun).
			// Fifteen types in this order: the order fixes component type ids
			// and a type declared in a different position is a different world.
			W.Persons = PersonTypes::Declare(Instance, Ages);
			W.Families = FamilyTypes::Declare(Instance);
			W.Needs = NeedTypes::Declare(Instance);
			W.Traits = TraitTypes::Declare(Instance);
			W.Lod = LodTypes::Declare(Instance);
			W.Organizations = OrganizationTypes::Declare(Instance);
			W.Standing = StandingTypes::Declare(Instance);
			W.Norms = NormTypes::Declare(Instance);
			W.Bondage = BondageTypes::Declare(Instance);
			W.Economy_ = EconomyTypes::Declare(Instance);
			W.Production = ProductionTypes::Declare(Instance);
			W.Markets = MarketTypes::Declare(Instance);
			W.Trade = TradeTypes::Declare(Instance);
			W.Wealth = WealthTypes::Declare(Instance);
			W.Polities = PolityTypes::Declare(Instance);
			// Declared only when asked, and after everything the Atlas declares,
			// so a world not asked for them carries no trace and its digests are
			// the digests it had. ColonyTypes::Declare also declares
			// Economy::RegionMined, which is why the Atlas keeps it optional too.
			if (Given.Colony)
			{
				W.Pit = ColonyTypes::Declare(Instance);
			}
			if (Given.Play)
			{
				W.Played = PlayerTypes::Declare(Instance);
				W.Start = StartTypes::Declare(Instance);
				W.Hour = HourTypes::Declare(Instance);
				W.Order = OrderTypes::Declare(Instance);
				W.Regard = RegardTypes::Declare(Instance);
				W.LifeRecords = LifeChronicleTypes::Declare(Instance);
			}
			if (Given.Lively)
			{
				W.Living = LivingTypes::Declare(Instance);
				W.Repute = ReputeTypes::Declare(Instance);
				W.Fame = FameTypes::Declare(Instance);
			}

			LifeRules Lifetimes;
			Lifetimes.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), W.Persons, Lifetimes);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), W.Persons, W.Families, FamilyRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), W.Persons, W.Needs, NeedRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), W.Persons, W.Traits, TraitRules{});
			// 15.02: the bridge keeps the crossings, whose rates are shares of a
			// crowd that leaves in a YEAR, and hands the detail decisions to a
			// system that runs on a DAY - but only when asked, because a world
			// that decides detail on a different cadence is a different world.
			Bridging.DecideElsewhere = Given.Stream;
			// AND THE DETAIL BUDGET HAS TO FIT EVERYBODY WHO ASKS. Three owners
			// write into LodState's eight slots: Begin takes one, NearDetail
			// takes MaxDetailed - 1 for the walk and holds them for the whole
			// life of the played person, and the warden takes up to
			// Attention::Most. DecideDetail promotes in request order and stops
			// at MaxDetailed, so with the default four the first two owners ate
			// the entire budget and THE WARDEN'S REGIONS WERE NEVER PROMOTED -
			// a camera could stare at a region for a whole life and the world
			// would never detail it, which is the one thing 15.07 exists to do.
			// One plus three plus four is eight, which is exactly
			// LodState::MaxWanted, so a streaming world raises its budget to
			// that and every owner is served. Only a streaming world: this
			// changes what is detailed, and nothing else may move.
			Bridging.MaxDetailed = Given.Stream ? Population::LodState::MaxWanted : Bridging.MaxDetailed;
			// 15.08: one region a day at most, and only for a world that
			// streams. A promotion costs about 65 ms (14.10) and two on one day
			// turn is a fifth of a second of the game stopping.
			Bridging.PromotionsPerPass = Given.Stream ? 1u : 0u;
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), W.Persons, W.Lod, Bridging);
			if (Given.Stream)
			{
				Grain = std::make_unique<DetailSystem>(Instance, Ages.Types(), W.Persons, W.Lod, Bridging);
			}
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Traits,
														W.Organizations, OrganizationRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), W.Norms, NormRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Economy_,
												   EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Economy_,
														 W.Production, ProductionRules{});
			Fair = std::make_unique<MarketSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Economy_, W.Markets,
												  ProductionRules{}, MarketRules{});
			Roads = std::make_unique<TradeSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Economy_, W.Markets,
												  W.Trade, ProductionRules{}, MarketRules{}, TradeRules{});
			Purses = std::make_unique<WealthSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Economy_,
													W.Markets, W.Norms, W.Wealth, WealthRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Traits,
													 W.Organizations, W.Standing, StandingRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), W.Persons, W.Norms, W.Standing, W.Bondage,
													BondageRules{});
			Rulers = std::make_unique<PolitySystem>(Instance, Ages.Types(), W.Persons, W.Organizations, W.Polities,
													PolityRules{});
			if (Given.Colony)
			{
				Rock = std::make_unique<MiningSystem>(Instance, Ages.Types(), W.Persons, W.Families, W.Economy_, W.Pit,
													  MiningRules{});
				Rock->ObserveTraits(W.Traits.Traits);
			}
			if (Given.Play)
			{
				// Phase 10, as Test_PlayerGate wires it: somebody to be, a day at
				// a time, what they mean to do, what they do, and what the world
				// makes of it. The bridge honours the hold only when told to look
				// for it, which is what keeps every world without a player as it
				// was.
				Bonds->RunAfter("Lod");
				Days =
					std::make_unique<PlayerDaySystem>(Instance, Ages.Types(), W.Persons, W.Played, W.Hour, HourRules{});
				Acts = std::make_unique<PlayerOrderSystem>(Instance, Ages.Types(), W.Persons, W.Played, W.Hour, W.Order,
														   OrderRules{});
				Hands =
					std::make_unique<Doings>(Ages.Types(), W.Persons, W.Families, W.Needs, W.Economy_, DoingRules{});
				Acts->ObserveDoing(Hands.get());
				Talk = std::make_unique<RegardSystem>(Instance, Ages.Types(), W.Persons, W.Played, W.Standing, W.Regard,
													  RegardRules{});
				Bridge->ObserveHeld(W.Played.Held);
				// The life's chronicle describes through the economy's describer,
				// which speaks for the society under it and the persons under
				// that (06.07); the Run has no works to describe.
				SocietyCtx = SocietyContext{W.Persons, W.Families, W.Organizations};
				EconomyCtx = EconomyContext{W.Persons, W.Families, W.Trade, W.Markets, MarketRules{}, &SocietyCtx};
				Chron.Persons = W.Persons;
				Chron.Families = W.Families;
				Chron.Player = W.Played;
				Chron.Regard = W.Regard;
				Chron.Goods = &EconomyCtx;
				// A played life records its refusals (14.05): what the world would
				// not let them do is a line of the screen, not only a count.
				LifeChronicleRules Kept;
				Kept.RecordRefusals = 1;
				Told = std::make_unique<LifeChronicle>(Instance, Ages.Types(), Chron, W.LifeRecords, Kept);
			}
			if (Given.Lively)
			{
				// Phase 12, as Test_GameplayGate wires it: living daily after the
				// player's own day, repute after living, fame and judgement
				// yearly after the year's trade and the year's bondage.
				Daily = std::make_unique<LivingSystem>(Instance, Ages.Types(), W.Persons, W.Living, LivingRules{});
				Daily->ObserveDoing(Hands.get());
				Daily->ObserveTraits(W.Traits.Traits);
				Daily->RunAfter("PlayerOrders");
				Gossip = std::make_unique<ReputeSystem>(Instance, W.Persons, W.Repute, ReputeRules{});
				Gossip->RunAfter("Living");
				Abroad = std::make_unique<FameSystem>(Instance, Ages.Types(), W.Persons, W.Repute, W.Trade, W.Fame,
													  FameRules{});
				Abroad->RunAfter("Trade");
				Judge = std::make_unique<JudgementSystem>(Instance, Ages.Types(), W.Persons, W.Bondage, W.Fame,
														  JudgementRules{});
				Judge->RunAfter("Fame");
				Judge->RunAfter("Bondage");
			}

			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(W.Norms.Marriage);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Orgs->RunAfter("Needs");
			Ranks->RunAfter("Wealth");
			Ranks->ObserveWealth(W.Wealth.Wealth);
			Stocks->RunAfter("Lod");
			Stocks->ObserveHeirs(W.Wealth.Heir);
			Harvest->ObserveTraits(W.Traits.Traits);
			Body->RunAfter("Production");
			Body->ObserveRation(W.Production.Ration);
			Rulers->RunAfter("Lod");

			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			if (Grain)
			{
				Instance.Systems().Add(Grain.get());
			}
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
			if (Given.Play)
			{
				Instance.Systems().Add(Days.get());
				Instance.Systems().Add(Acts.get());
				Instance.Systems().Add(Talk.get());
			}
			if (Given.Lively)
			{
				Instance.Systems().Add(Daily.get());
				Instance.Systems().Add(Gossip.get());
				Instance.Systems().Add(Abroad.get());
				Instance.Systems().Add(Judge.get());
			}
			Instance.Build();
			if (Told != nullptr)
			{
				Told->Attach();
			}
		}

		/// The region with the most people: the one worth simulating person
		/// by person. Ties go to the lower index, so the choice does not ride
		/// on pool order. Copied from Tools/Atlas, whose digests depend on it.
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

		/// The peopled region with the most people that has ORE under it: a
		/// colony is people put on rock. Copied from Tools/Atlas.
		uint32 BusiestWithOre() const
		{
			std::vector<uint32> Ore;
			Instance.Components()
				.GetPool(Ages.Types().World.DepositTypes_.Deposit)
				.ForEach(
					[&](EntityHandle, const DepositInfo& D)
					{
						const bool Seam = D.Kind == static_cast<uint32>(ResourceKind::IronOre) ||
										  D.Kind == static_cast<uint32>(ResourceKind::CopperOre);
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

		Options Given;
		World Instance;
		PreHistory Ages;
		Wired W;
		// EconomyCtx holds a pointer to SocietyCtx, so the two must not be
		// reordered and this struct must not be copied. It is neither.
		SocietyContext SocietyCtx;
		EconomyContext EconomyCtx;
		Player::LifeContext Chron;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<TraitSystem> Minds;
		/// The rules the bridge was BUILT with, kept so that NearDetail reads
		/// them instead of a fresh default. 15.03: they agreed only because
		/// both were default-constructed, which is a coincidence and not a
		/// guarantee, and the day one is parameterised they would part company
		/// with nothing to say so.
		LodRules Bridging;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<DetailSystem> Grain; ///< only with Options::Stream
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
		std::unique_ptr<Player::PlayerDaySystem> Days;
		std::unique_ptr<Player::PlayerOrderSystem> Acts;
		std::unique_ptr<Player::Doings> Hands;
		std::unique_ptr<Player::RegardSystem> Talk;
		std::unique_ptr<Player::LifeChronicle> Told;
		std::unique_ptr<LivingSystem> Daily;
		std::unique_ptr<ReputeSystem> Gossip;
		std::unique_ptr<FameSystem> Abroad;
		std::unique_ptr<JudgementSystem> Judge;
	};

	Aelvor::Aelvor(const Options& In) : Given_(In)
	{
		Given_.Play = Given_.Play || Given_.Lively;
		K = std::make_unique<Kernel>(Given_);
	}

	Aelvor::~Aelvor() = default;

	bool Aelvor::Begin()
	{
		if (Run_.Begun)
		{
			return false;
		}
		WorldGenConfig Gen;
		Gen.Width = Given_.Size;
		Gen.Height = Given_.Size;
		// Seed the world and run NOTHING, then the years through Run: what
		// changes a run is WHEN detail is requested, and that happens after
		// exactly PreHistory years, as in Tools/Atlas.
		if (!K->Ages.Generate(Gen, 0, false))
		{
			return false;
		}
		if (Given_.PreHistory != 0)
		{
			K->Ages.Run(Given_.PreHistory);
		}
		Run_.Detail = K->Busiest();
		if (Given_.Colony)
		{
			const uint32 Seamed = K->BusiestWithOre();
			Run_.Detail = Seamed != 0 ? Seamed : Run_.Detail;
		}
		RequestDetail(K->Instance, K->W.Lod, Run_.Detail);
		if (Given_.Colony && Run_.Detail != 0 && FoundColony(K->Instance, K->Ages.Types(), K->W.Pit, Run_.Detail))
		{
			Run_.Dug = Run_.Detail;
		}
		if (Given_.Lively && Run_.Detail != 0)
		{
			MakeLively(K->Instance, K->Ages.Types(), K->W.Living, Run_.Detail);
		}
		if (Given_.Years != 0)
		{
			K->Ages.Run(Given_.Years);
		}
		Run_.Begun = true;
		return true;
	}

	World& Aelvor::Instance() noexcept
	{
		return K->Instance;
	}
	const World& Aelvor::Instance() const noexcept
	{
		return K->Instance;
	}
	SimTick Aelvor::Now() const noexcept
	{
		return K->Instance.Now();
	}
	const Wired& Aelvor::Handles() const noexcept
	{
		return K->W;
	}
	const PreHistoryTypes& Aelvor::Ages() const noexcept
	{
		return K->Ages.Types();
	}

	void Aelvor::LookAt(const Attention& At)
	{
		Run_.Eyes = At;
		// HERE rather than in a host, for the reason NearDetail gives: a host
		// that asked for detail itself would be a host whose stream replays
		// into a different world. And only when asked for, because a world that
		// pays attention to where somebody is looking is a different world from
		// the same seed (ADR-0141).
		if (Given_.Stream && Run_.Begun)
		{
			Reside(At);
		}
	}

	void Aelvor::Reside(const Attention& At)
	{
		// The warden. A function from attention to detail REQUESTS, and never a
		// promotion: the bridge is the only thing that promotes and it decides
		// on its own cadence (ADR-0037, ADR-0141). Deterministic in the only
		// way that matters for a replay - breadth first from the region looked
		// at, ties broken by ascending region index, nothing weighed, no clock
		// read and no random stream drawn.
		const uint32 Most = At.Most == 0 ? K->Bridging.MaxDetailed : At.Most;
		const WorldGen::RegionGraph& Graph = Ways_.Of(K->Instance.Map(), K->Ages.Types().World.Regions);

		// Want: within Reach steps, capped by what the host will pay for.
		// Keep: within Reach + 1. The band between them is the hysteresis, and
		// it is what stops a camera walking back and forth over one border from
		// promoting and demoting the same two regions every day - a promotion
		// costs about 65 ms (14.10) and a demotion throws away every person it
		// made, so thrashing there is not a stutter, it is a world that keeps
		// forgetting a place and inventing it again.
		std::vector<uint16> Want;
		std::vector<uint16> Keep;
		if (At.Region != 0 && At.Region < Graph.Neighbours.size())
		{
			std::vector<uint32> Step(Graph.Neighbours.size(), 0xFFFFFFFFu);
			std::vector<uint32> Edge{At.Region};
			Step[At.Region] = 0;
			for (uint32 Far = 0; Far <= At.Reach + 1u && !Edge.empty(); ++Far)
			{
				std::sort(Edge.begin(), Edge.end());
				std::vector<uint32> Next;
				for (const uint32 R : Edge)
				{
					if (Far <= At.Reach && Want.size() < Most)
					{
						Want.push_back(static_cast<uint16>(R));
					}
					Keep.push_back(static_cast<uint16>(R));
					for (const uint16 N : Graph.Neighbours[R])
					{
						if (N != 0 && Step[N] == 0xFFFFFFFFu)
						{
							Step[N] = Far + 1u;
							Next.push_back(N);
						}
					}
				}
				Edge.swap(Next);
			}
		}
		std::sort(Want.begin(), Want.end());
		std::sort(Keep.begin(), Keep.end());

		// Give back what this look no longer wants. Never Begin's region, which
		// is the world's floor, and never one the taking of ADR-0139 asked for,
		// which is what a Move needs. The region the played person stands in is
		// safe without being named here: 15.01's fence refuses to demote a
		// region holding somebody, so releasing it costs them nothing.
		//
		// What survives stays IN Run_.Watched, and that is not bookkeeping tidiness:
		// a region kept by the band and forgotten here would be one this warden
		// never released again, because releasing is only ever done to something
		// it remembers asking for. The first draft dropped them and leaked a
		// slot per crossing.
		std::vector<uint16> Now;
		for (const uint16 Old : Run_.Watched)
		{
			const bool Held = uint32{Old} == Run_.Detail ||
							  std::find(Run_.Near.begin(), Run_.Near.end(), Old) != Run_.Near.end() ||
							  std::binary_search(Keep.begin(), Keep.end(), Old);
			if (Held)
			{
				Now.push_back(Old);
			}
			else
			{
				ReleaseDetail(K->Instance, K->W.Lod, Old);
			}
		}
		for (const uint16 R : Want)
		{
			if (RequestDetail(K->Instance, K->W.Lod, R))
			{
				Now.push_back(R);
			}
		}
		std::sort(Now.begin(), Now.end());
		Now.erase(std::unique(Now.begin(), Now.end()), Now.end());

		// AND THE BUDGET BINDS THE UNION, not just the Want. Keeping the band's
		// survivors is what stops a border crossing from thrashing, but a
		// survivor is still a region the host is paying to hold: the first
		// version added them on top of a Want already capped at Most and so
		// asked for more than the host said it would pay for, which is the one
		// promise Attention::Most makes. The oldest go first, because the newer
		// ones are nearer where the camera is now.
		if (Now.size() > Most)
		{
			for (usize i = Most; i < Now.size(); ++i)
			{
				const uint16 Over = Now[i];
				const bool Keeps = uint32{Over} == Run_.Detail ||
								   std::find(Run_.Near.begin(), Run_.Near.end(), Over) != Run_.Near.end();
				if (!Keeps)
				{
					ReleaseDetail(K->Instance, K->W.Lod, Over);
				}
			}
			Now.resize(Most);
		}
		Run_.Watched.swap(Now);
	}

	Player::StreamHeader Aelvor::Header() const
	{
		Player::StreamHeader H;
		H.Seed = Given_.Seed;
		H.Size = Given_.Size;
		H.PreHistory = Given_.PreHistory;
		H.Years = Given_.Years;
		return H;
	}

	View::ViewSources Aelvor::Sources() const
	{
		View::ViewSources S;
		S.Types = K->Ages.Types();
		S.Persons = K->W.Persons;
		S.HasBondage = true;
		S.Bondage = K->W.Bondage;
		S.HasTrade = true;
		S.Trade = K->W.Trade;
		S.HasColony = Given_.Colony;
		S.Colony_ = K->W.Pit;
		S.HasPlayer = Given_.Play;
		S.Played = K->W.Played;
		S.HasFame = Given_.Lively;
		S.Fame = K->W.Fame;
		S.HasLife = Given_.Play;
		S.Hour = K->W.Hour;
		S.Order = K->W.Order;
		S.Regard = K->W.Regard;
		S.Start = K->W.Start;
		S.Needs = K->W.Needs;
		S.Families = K->W.Families;
		S.HasGoods = true;
		S.Markets = K->W.Markets;
		S.Organizations = K->W.Organizations;
		return S;
	}

	uint32 Aelvor::TakeUp(const Player::StartRules& Rules)
	{
		if (!Run_.Begun || !Given_.Play || Played() != 0)
		{
			return 0;
		}
		const uint32 Who = Player::BeginEnslaved(K->Instance, K->Ages.Types(), K->W.Persons, K->W.Bondage,
												 K->W.Standing, K->W.Played, K->W.Start, Rules, K->Instance.Now());
		if (Who == 0)
		{
			return 0;
		}
		if (!Player::BeginOrders(K->Instance, K->W.Played, K->W.Order, K->Instance.Now()))
		{
			Release();
			return 0;
		}
		NearDetail(Who);
		return Who;
	}

	void Aelvor::NearDetail(uint32 Who)
	{
		// ADR-0139. Ask for the neighbours of the region the played person
		// stands in, so that a Move has somewhere to go.
		//
		// Until 14.10 the only region this world ever asked to detail was the
		// busiest one (Begin, above), while LodRules::MaxDetailed allows four -
		// three slots nobody used. View::Life lists a neighbour in Near only
		// when it is adjacent AND detailed, because Player::Doings refuses a
		// Move to anywhere else as TooFar, so Near was empty on every day of
		// every played world: the page never offered Move and the world could
		// never have taken it. The game offered eight verbs and could take
		// seven. Measured over thirty days at AELVOR 256: NearCount 0, thirty
		// times out of thirty.
		//
		// Here rather than in the host, and in Aelvor rather than in Door, so
		// that every taking gets it - the first one, the one that follows a
		// death, and the ones Run::Replay applies out of a stream. A host that
		// asked for this itself would be a host whose stream replays into a
		// different world.
		//
		// Ascending region index, and nothing weighed: the order has to be the
		// same on every machine, and "the neighbour with the most people" is a
		// judgement that would move with the year.
		const Population::PersonIndex Index = Population::BuildPersonIndex(K->Instance, K->W.Persons);
		const EntityHandle Self = Who < Index.Handles.size() ? Index.Handles[Who] : EntityHandle{};
		if (Self.IsNull())
		{
			return;
		}
		const Population::PersonInfo* P = K->Instance.Components().GetPool(K->W.Persons.Person).TryGet(Self);
		if (P == nullptr || P->Region == 0)
		{
			return;
		}
		const WorldGen::RegionGraph Graph =
			WorldGen::BuildRegionGraph(K->Instance.Map(), K->Ages.Types().World.Regions);
		if (P->Region >= Graph.Neighbours.size())
		{
			return;
		}
		// 15.03: GIVE BACK what the last taking asked for, before asking again.
		//
		// RequestDetail only ever appends and returns false for good once
		// LodState::MaxWanted = 8 is reached (Population/Lod.cpp). This runs on
		// EVERY taking - the first, the one after a death, and every TakenUp
		// that Run::Replay applies - and until this line released nothing. One
		// region at Begin and three per taking: after roughly three deaths the
		// wanted list saturated, Near went empty and the world refused every
		// Move as TooFar for the rest of that world's life. ADR-0139's fix
		// expired, silently, in a way the checked-in month cannot show because
		// it has exactly one taking.
		//
		// Two regions are never given back: the one the new person is standing
		// in, and Begin's busiest, which is the colony's and the world's floor.
		// The 15.01 fence is what makes this safe to ship at all - releasing is
		// what finally lets the bridge demote, and a demotion used to destroy
		// whoever was being played.
		for (const uint16 Old : Run_.Near)
		{
			if (uint32{Old} == P->Region || uint32{Old} == Run_.Detail)
			{
				continue;
			}
			ReleaseDetail(K->Instance, K->W.Lod, Old);
		}
		Run_.Near.clear();

		// HOW MANY NEIGHBOURS A WALK NEEDS, and it is not "all the budget but
		// one". This read MaxDetailed - 1, which tied the walk's appetite to
		// the world's whole detail budget: raising that budget so the warden
		// could be served raised NearDetail's share with it and starved the
		// warden completely - 1 + 7 = 8 = MaxWanted, and every RequestDetail
		// the camera made was refused. The two numbers answer different
		// questions and must not be the same number.
		//
		// The arithmetic a streaming world runs on: 1 for where the played
		// person stands, MaxNear for somewhere to walk, and Attention::Most for
		// what the camera is looking at. 1 + 3 + 4 = 8 = LodState::MaxWanted,
		// which is the ceiling all three share.
		static constexpr uint32 MaxNear = 3;
		const LodRules& Rules = K->Bridging;
		uint32 Asked = 0;
		std::vector<uint16> Near(Graph.Neighbours[P->Region]);
		std::sort(Near.begin(), Near.end());
		for (const uint16 N : Near)
		{
			if (Rules.MaxDetailed == 0 || Asked >= MaxNear || Asked + 1 >= Rules.MaxDetailed)
			{
				break;
			}
			if (N == 0 || IsWanted(K->Instance, K->W.Lod, N))
			{
				continue;
			}
			if (RequestDetail(K->Instance, K->W.Lod, N))
			{
				++Asked;
				Run_.Near.push_back(N);
			}
		}
	}

	bool Aelvor::Release()
	{
		if (!Given_.Play || Played() == 0)
		{
			return false;
		}
		Player::ReleasePlayer(K->Instance, K->W.Played);
		Player::EndOrders(K->Instance, K->W.Order);
		Player::EndStart(K->Instance, K->W.Start);
		Player::EndHours(K->Instance, K->W.Hour);
		Player::EndRegard(K->Instance, K->W.Regard);
		return true;
	}

	uint32 Aelvor::Played() const
	{
		return Given_.Play ? Player::PlayerPerson(K->Instance, K->W.Played) : 0u;
	}

	bool Aelvor::PlayedAlive() const
	{
		const uint32 P = Played();
		if (P == 0)
		{
			return false;
		}
		const PersonInfo* I = FindPerson(K->Instance, K->W.Persons, P);
		return I != nullptr && I->State == static_cast<uint8>(LifeState::Alive);
	}

	Player::Refusal Aelvor::Submit(const Player::PlayerCommand& C)
	{
		if (!Given_.Play)
		{
			return Player::Refusal::NoPlayer;
		}
		return Player::Submit(K->Instance, K->W.Played, K->W.Order, Player::OrderRules{}, C);
	}

	uint64 Aelvor::Day()
	{
		// Nothing to turn before Begin(): the world has no map and no history,
		// and a tick of it would fail a check rather than do nothing.
		if (Run_.Begun)
		{
			K->Instance.TickMany(24);
		}
		return K->Instance.Now();
	}

	Hash64 Aelvor::StateDigest() const
	{
		return ComputeStateDigest(K->Instance);
	}

	Hash64 Aelvor::LogDigest() const
	{
		return K->Instance.Log().Digest();
	}

	std::string Aelvor::Life() const
	{
		std::string Out;
		if (Given_.Play)
		{
			Player::ExportLife(K->Instance, K->Ages.Types(), K->Chron, Out, 40);
		}
		return Out;
	}

	Player::OrderStats Aelvor::Orders() const
	{
		if (!Given_.Play)
		{
			return Player::OrderStats{};
		}
		return Player::MeasureOrders(K->Instance, K->W.Persons, K->W.Played, K->W.Order, Player::OrderRules{});
	}

	Player::HourStats Aelvor::Hours() const
	{
		if (!Given_.Play)
		{
			return Player::HourStats{};
		}
		return Player::MeasureHours(K->Instance, K->W.Persons, K->W.Played, K->W.Hour, Player::HourRules{});
	}

	uint32 Aelvor::Generations() const noexcept
	{
		return K == nullptr ? 0u : K->Ages.Generations();
	}

	const char* Aelvor::AdoptResultToString(AdoptResult Result) noexcept
	{
		switch (Result)
		{
		case AdoptResult::Ok:
			return "Ok";
		case AdoptResult::AlreadyBegun:
			return "AlreadyBegun";
		case AdoptResult::ContainerRefused:
			return "ContainerRefused";
		case AdoptResult::WrongSeed:
			return "WrongSeed";
		case AdoptResult::StateRefused:
			return "StateRefused";
		case AdoptResult::NoRunSection:
			return "NoRunSection";
		case AdoptResult::RunRefused:
			return "RunRefused";
		case AdoptResult::NoHostSection:
			return "NoHostSection";
		case AdoptResult::WorldSizeDiffers:
			return "WorldSizeDiffers";
		case AdoptResult::PreHistoryDiffers:
			return "PreHistoryDiffers";
		case AdoptResult::YearsDiffers:
			return "YearsDiffers";
		case AdoptResult::ColonyDiffers:
			return "ColonyDiffers";
		case AdoptResult::PlayDiffers:
			return "PlayDiffers";
		case AdoptResult::LivelyDiffers:
			return "LivelyDiffers";
		case AdoptResult::StreamDiffers:
			return "StreamDiffers";
		}
		return "Unknown";
	}

	Aelvor::AdoptResult Aelvor::Adopt(const uint8* Bytes, usize Size)
	{
		if (Run_.Begun)
		{
			// Adopting over a live world would restore what the container
			// carries and leave everything it does not - which is the exact
			// shape of the defect this phase exists to remove.
			return AdoptResult::AlreadyBegun;
		}
		if (K == nullptr)
		{
			return AdoptResult::ContainerRefused;
		}

		CheckpointView View;
		if (ReadCheckpoint(Bytes, Size, View).Result != CheckpointResult::Ok)
		{
			return AdoptResult::ContainerRefused;
		}
		// CHECKED AGAINST Given_ BEFORE ANYTHING IS TOUCHED. The seed is the
		// world's identity and every derived stream hangs off it; a checkpoint
		// of another seed is another world wearing this one's shape. The map
		// SIZE is deliberately not checked here - LoadSnapshot's layout gate
		// owns that, and duplicating it would give two answers to one question.
		if (View.Seed != Given_.Seed)
		{
			return AdoptResult::WrongSeed;
		}

		// THE WHOLE DECLARED WORLD, NOT JUST ITS SEED - task 16.10, and the
		// order matters: every one of these is checked BEFORE the world is
		// touched, so a host handed a checkpoint of another world keeps the one
		// it has. Measured before it was written: a 32-tile save adopted
		// cleanly into hosts declaring 16 and 64, and into hosts declaring
		// other pre-histories, other year counts and Stream=false. Four silent,
		// one caught by accident, one on purpose.
		Options Declared;
		if (!ReadHostSection(View, Declared))
		{
			// Without it there is no way to know what world this is OF, and
			// guessing is what the whole task exists to stop.
			return AdoptResult::NoHostSection;
		}
		if (Declared.Size != Given_.Size)
		{
			return AdoptResult::WorldSizeDiffers;
		}
		if (Declared.PreHistory != Given_.PreHistory)
		{
			return AdoptResult::PreHistoryDiffers;
		}
		if (Declared.Years != Given_.Years)
		{
			return AdoptResult::YearsDiffers;
		}
		// EACH FLAG UNDER ITS OWN NAME. `Options::Stream` declares no component
		// type, so it is invisible to every guard the kernel already had - and
		// it decides what is detailed WHEN, which decides who exists.
		if (Declared.Colony != Given_.Colony)
		{
			return AdoptResult::ColonyDiffers;
		}
		if (Declared.Play != Given_.Play)
		{
			return AdoptResult::PlayDiffers;
		}
		if (Declared.Lively != Given_.Lively)
		{
			return AdoptResult::LivelyDiffers;
		}
		if (Declared.Stream != Given_.Stream)
		{
			return AdoptResult::StreamDiffers;
		}

		uint64 StateLength = 0;
		const uint8* State = View.Find(SectionKind::State, StateLength);
		if (State == nullptr)
		{
			return AdoptResult::ContainerRefused;
		}
		// THE RUN IS READ BEFORE THE WORLD IS TOUCHED. A container with no RUN
		// section must refuse while this Aelvor is still untouched, not after
		// its world has been replaced - otherwise the refusal leaves exactly
		// the half-restored world 16.03 taught this file not to make.
		RunState Carried;
		if (!ReadRunSection(View, Carried))
		{
			return AdoptResult::NoRunSection;
		}

		if (LoadSnapshot(K->Instance, State, static_cast<usize>(StateLength)) != SnapshotResult::Ok)
		{
			// 16.03: the world is what it was.
			return AdoptResult::StateRefused;
		}
		if (!SetRunState(Carried))
		{
			return AdoptResult::RunRefused;
		}

		// NO Generate, NO Run, NO year. Everything Begin() would have derived
		// is already in the bytes - which is the whole of this task.
		Run_.Begun = true;
		return AdoptResult::Ok;
	}

	Aelvor::RunState Aelvor::GetRunState() const
	{
		// A copy of the whole thing, deliberately. Handing back a reference
		// would let a caller hold the run's state while the run moves under it,
		// and 16.06 hands this across a restore where that would be exactly
		// wrong.
		return Run_;
	}

	bool Aelvor::SetRunState(const RunState& In)
	{
		// REFUSED RATHER THAN CLAMPED. A region past the end of this world's map
		// means the state came from a world this one is not - a different size,
		// a different wiring - and quietly dropping it would restore a run that
		// LOOKS right and watches somewhere that does not exist. 16.03 settled
		// the principle for the world; this is the same principle for the run.
		if (K == nullptr)
		{
			return false;
		}
		// The bound comes from the same region graph LookAt uses, through the
		// same cache, so a state this accepts is one the warden can act on.
		// Region indices are 1-based with entry 0 unused (Regions.h:85), so the
		// last valid one is RegionCount() itself.
		const uint32 Regions = Ways_.Of(K->Instance.Map(), K->Ages.Types().World.Regions).RegionCount();
		const auto Beyond = [Regions](uint32 Region) { return Region > Regions; };
		for (const uint16 Region : In.Near)
		{
			if (Beyond(Region))
			{
				return false;
			}
		}
		for (const uint16 Region : In.Watched)
		{
			if (Beyond(Region))
			{
				return false;
			}
		}
		if (Beyond(In.Eyes.Region))
		{
			return false;
		}
		if (Beyond(In.Detail))
		{
			return false;
		}

		Run_ = In;
		// Ways_ is NOT restored and NOT cleared here. It is a cache keyed on
		// HashCombine(HashUInt64(Map.Revision()), ...) (Regions.cpp:421), and
		// WorldMap::Reset bumps Replaced on every load (WorldMap.cpp:53), so
		// the load that brought this state in has already invalidated it. There
		// is nothing to do, and doing something would hide that.
		return true;
	}
} // namespace Vaelen::Run
