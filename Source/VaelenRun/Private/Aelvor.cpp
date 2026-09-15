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
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), W.Persons, W.Lod, LodRules{});
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
		if (Begun_)
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
		Detail_ = K->Busiest();
		if (Given_.Colony)
		{
			const uint32 Seamed = K->BusiestWithOre();
			Detail_ = Seamed != 0 ? Seamed : Detail_;
		}
		RequestDetail(K->Instance, K->W.Lod, Detail_);
		if (Given_.Colony && Detail_ != 0 && FoundColony(K->Instance, K->Ages.Types(), K->W.Pit, Detail_))
		{
			Dug_ = Detail_;
		}
		if (Given_.Lively && Detail_ != 0)
		{
			MakeLively(K->Instance, K->Ages.Types(), K->W.Living, Detail_);
		}
		if (Given_.Years != 0)
		{
			K->Ages.Run(Given_.Years);
		}
		Begun_ = true;
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
		if (!Begun_ || !Given_.Play || Played() != 0)
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
		// One slot is the played person's own region; the rest are the walk.
		const LodRules Rules;
		uint32 Asked = 0;
		std::vector<uint16> Near(Graph.Neighbours[P->Region]);
		std::sort(Near.begin(), Near.end());
		for (const uint16 N : Near)
		{
			if (Rules.MaxDetailed == 0 || Asked + 1 >= Rules.MaxDetailed)
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
		if (Begun_)
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
} // namespace Vaelen::Run
