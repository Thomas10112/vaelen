// VAELEN - Tests/Gameplay
// Phase 12.03: documents.
//
// A document is hearsay that outlives the teller. The claim this file exists to
// show is that a page and the truth come apart: what is written stays written
// after its writer has changed their mind, after the writer is dead, and after
// the person it speaks of has become somebody else.
//
// STATUS: PROTOTYPE (Phase 12)

#include "Vaelen/Gameplay/Living.h"
#include "Vaelen/Gameplay/Documents.h"
#include "Vaelen/Gameplay/Repute.h"
#include "Vaelen/Player/Doings.h"
#include "Vaelen/Colony/Holders.h"
#include "Vaelen/Colony/Mining.h"
#include "Vaelen/Economy/Production.h"
#include "Vaelen/Economy/Stocks.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Colony;
using namespace Vaelen::Gameplay;
using namespace Vaelen::Economy;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogDocs);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// The whole stack: a colony needs bondage AND feeding to be measured, and
	/// the two live in different modules.
	struct Run
	{
		explicit Run(uint64 Seed, uint32 HeldRegion = 0, LivingRules InLive = LivingRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Needs = NeedTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			Economy_ = EconomyTypes::Declare(Instance);
			Production = ProductionTypes::Declare(Instance);
			Colony = ColonyTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			// NOT LodRules::Held. That rule is fixed when the system is built, so
			// it holds the region through the whole of Generate's pre-history -
			// and with the full economy running, three hundred years of that
			// empties the region: 1460 people in a world without the hold, none
			// in the world with it. The dated way is RequestDetail (04.06), which
			// protects a region from demotion (Lod.cpp:170) exactly as the rule
			// does, but from the tick it is asked rather than from the beginning
			// of time. Same lesson as ADR-0090: a colony is a fact with a date.
			(void)HeldRegion;
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage,
													BondageRules{});
			Stocks = std::make_unique<StockSystem>(Instance, Ages.Types(), Persons, Families, Economy_, EconomyRules{});
			Harvest = std::make_unique<ProductionSystem>(Instance, Ages.Types(), Persons, Families, Economy_,
														 Production, ProductionRules{});
			Ranks->ObserveBonds(Bondage.Bond);
			Orgs->ObserveBonds(Bondage.Bond);
			Harvest->ObserveTraits(Traits.Traits);
			Harvest->ObserveMined(Colony.Mined);
			// 12.01: the people of a lively region act for themselves, through the
			// very Doings 10.05 hands to the player's own system.
			Live = LivingTypes::Declare(Instance);
			Acts = std::make_unique<Player::Doings>(Ages.Types(), Persons, Families, Needs, Economy_,
													Player::DoingRules{});
			Lives_ = std::make_unique<LivingSystem>(Instance, Ages.Types(), Persons, Live, InLive);
			Lives_->ObserveDoing(Acts.get());
			// 12.02: what people make of each other, and what they are told.
			Names = ReputeTypes::Declare(Instance);
			Papers = DocumentTypes::Declare(Instance);
			Talk = std::make_unique<ReputeSystem>(Instance, Persons, Names, ReputeRules{});
			Talk->RunAfter("Living");
			// 11.06: and who is on the rock, so the rest of the colony still farms.
			Harvest->ObserveBonds(Bondage.Bond);
			Body->RunAfter("Production");
			Body->ObserveRation(Production.Ration);
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			Houses->ObserveNorms(Norms.Marriage);
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Stocks->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Body.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Bonds.get());
			Instance.Systems().Add(Stocks.get());
			Instance.Systems().Add(Harvest.get());
			Instance.Systems().Add(Lives_.get());
			Instance.Systems().Add(Talk.get());
			Instance.Build();
		}
		static WorldConfig Config(uint64 Seed)
		{
			WorldConfig C;
			C.Seed = Seed;
			return C;
		}
		static WorldGenConfig Square(uint32 Size)
		{
			WorldGenConfig Gen;
			Gen.Width = Size;
			Gen.Height = Size;
			return Gen;
		}
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
		uint32 Alive(uint32 Region) const
		{
			uint32 N = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach([&](EntityHandle, const PersonInfo& P)
						 { N += P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) ? 1u : 0u; });
			return N;
		}
		/// Asks for a region to be simulated person by person from now on, and
		/// materialises it at once. RequestDetail is what keeps it detailed
		/// afterwards - the bridge will not demote a region that is wanted.
		bool Promote(uint32 Region)
		{
			RequestDetail(Instance, Lod, Region);
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0 ||
				   Alive(Region) > 0;
		}
		uint32 Grain(uint32 Region) const
		{
			EntityHandle RH;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						if (R.Index == Region && RH.IsNull())
						{
							RH = H;
						}
					});
			const RegionStock* S = RH.IsNull() ? nullptr : Instance.Components().GetPool(Economy_.Region).TryGet(RH);
			return S == nullptr ? 0u : S->Amount[static_cast<uint32>(Good::Grain)];
		}
		uint32 Feed(uint32 Region, uint32 Units)
		{
			return AddStock(Instance, Ages.Types(), Families, Economy_, Region, 0u, Good::Grain,
							static_cast<int32>(Units), Instance.Now());
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		NeedTypes Needs;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		EconomyTypes Economy_;
		ProductionTypes Production;
		ColonyTypes Colony;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<NeedSystem> Body;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<BondageSystem> Bonds;
		std::unique_ptr<StockSystem> Stocks;
		std::unique_ptr<ProductionSystem> Harvest;
		LivingTypes Live;
		std::unique_ptr<Player::Doings> Acts;
		std::unique_ptr<LivingSystem> Lives_;
		ReputeTypes Names;
		DocumentTypes Papers;
		std::unique_ptr<ReputeSystem> Talk;
	};
} // namespace

VAELEN_TEST(Documents, APageGoesOnSayingWhatItsWriterHasStoppedThinking)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 5);
	VT_REQUIRE(MakeLively(W.Instance, W.Ages.Types(), W.Live, Where));
	W.Instance.TickMany(24ull * 30ull);

	// Somebody who thinks something of somebody, taken from the world rather
	// than arranged: the first pair the opinions offer, in index order, so the
	// same seed picks the same two.
	uint32 About = 0;
	uint32 Writer = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				if (About != 0)
				{
					return;
				}
				const PersonRepute* R = W.Instance.Components().GetPool(W.Names.Repute).TryGet(H);
				if (R == nullptr || R->Known == 0 || R->Who[0].Person == 0 || R->Who[0].Regard == 0)
				{
					return;
				}
				About = P.Index;
				Writer = R->Who[0].Person;
			});
	VT_REQUIRE(About != 0 && Writer != 0);

	const Player::Opinion* Then = OpinionOf(W.Instance, W.Persons, W.Names, About, Writer);
	VT_REQUIRE(Then != nullptr);
	const int32 Thought = Then->Regard;
	const uint32 Doc = WriteDocument(W.Instance, W.Persons, W.Names, W.Papers, Writer, About, W.Instance.Now());
	VT_REQUIRE(Doc != 0);
	const DocumentInfo* D = DocumentOf(W.Instance, W.Papers, Doc);
	VT_REQUIRE(D != nullptr);
	VT_CHECK_EQ(D->Says, Thought);

	// A year of the region living. Opinions move; the page does not.
	W.Instance.TickMany(TicksPerYear);
	const Player::Opinion* Now_ = OpinionOf(W.Instance, W.Persons, W.Names, About, Writer);
	const int32 Thinks = Now_ != nullptr ? Now_->Regard : 0;
	const DocumentInfo* Still = DocumentOf(W.Instance, W.Papers, Doc);
	VT_REQUIRE(Still != nullptr);
	VAELEN_LOG_INFO(LogDocs, "person %u wrote of person %u at %d; a year on they think %d, and the page still says %d",
					Writer, About, Thought, Thinks, Still->Says);
	VT_CHECK_MSG(Still->Says == Thought, "the page says what it always said");
	VT_CHECK_MSG(Thinks != Thought, "and its writer has stopped thinking it, which is the whole point");

	// A copy says the same, however wrong it has become.
	const uint32 Copy = CopyDocument(W.Instance, W.Papers, Doc, Writer, W.Instance.Now());
	VT_REQUIRE(Copy != 0);
	const DocumentInfo* C = DocumentOf(W.Instance, W.Papers, Copy);
	VT_REQUIRE(C != nullptr);
	VT_CHECK_EQ(C->Says, Thought);
	VT_CHECK_EQ(C->About, About);
	VT_CHECK_MSG(C->From == Doc, "and it knows what it was copied from");

	// A lost page says nothing to anybody.
	VT_CHECK(LoseDocument(W.Instance, W.Papers, Doc, W.Instance.Now()));
	VT_CHECK(!LoseDocument(W.Instance, W.Papers, Doc, W.Instance.Now()));
	VT_CHECK(!CopyDocument(W.Instance, W.Papers, Doc, Writer, W.Instance.Now()));
	const DocumentStats S = MeasureDocuments(W.Instance, W.Persons, W.Papers);
	VAELEN_LOG_INFO(LogDocs, "%u written, %u standing, %u of them copies", S.Written, S.Standing, S.Copies);
	VT_CHECK_EQ(S.Written, 2u);
	VT_CHECK_EQ(S.Standing, 1u);
	VT_CHECK_EQ(S.Copies, 1u);
}

VAELEN_TEST(Documents, SomebodyWhoNeverMetThemLearnsThemFromAPage)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(W.Promote(Where));
	W.Instance.TickMany(TicksPerYear * 5);
	VT_REQUIRE(MakeLively(W.Instance, W.Ages.Types(), W.Live, Where));
	W.Instance.TickMany(24ull * 30ull);

	uint32 About = 0;
	uint32 Writer = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle H, const PersonInfo& P)
			{
				if (About != 0)
				{
					return;
				}
				const PersonRepute* R = W.Instance.Components().GetPool(W.Names.Repute).TryGet(H);
				if (R == nullptr || R->Known == 0 || R->Who[0].Regard == 0)
				{
					return;
				}
				About = P.Index;
				Writer = R->Who[0].Person;
			});
	VT_REQUIRE(About != 0);
	const uint32 Doc = WriteDocument(W.Instance, W.Persons, W.Names, W.Papers, Writer, About, W.Instance.Now());
	VT_REQUIRE(Doc != 0);

	// A reader who thinks nothing of the subject at all.
	uint32 Reader = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				if (Reader != 0 || P.Region != Where || P.Index == About || P.Index == Writer)
				{
					return;
				}
				if (OpinionOf(W.Instance, W.Persons, W.Names, About, P.Index) == nullptr)
				{
					Reader = P.Index;
				}
			});
	VT_REQUIRE(Reader != 0);
	VT_CHECK_MSG(OpinionOf(W.Instance, W.Persons, W.Names, About, Reader) == nullptr,
				 "they think nothing of them, because nothing has happened between them");
	VT_CHECK(ReadDocument(W.Instance, W.Persons, W.Names, W.Papers, DocumentRules{}, Doc, Reader, W.Instance.Now()));
	const Player::Opinion* Learnt = OpinionOf(W.Instance, W.Persons, W.Names, About, Reader);
	VT_REQUIRE(Learnt != nullptr);
	VAELEN_LOG_INFO(LogDocs, "person %u had never met person %u and now thinks %d of them, off a page", Reader, About,
					Learnt->Regard);
	VT_CHECK_MSG(Learnt->Regard != 0, "and now they think something, which they learnt from a page");
	// Nobody learns what they are from a page about themselves.
	VT_CHECK(!ReadDocument(W.Instance, W.Persons, W.Names, W.Papers, DocumentRules{}, Doc, About, W.Instance.Now()));
	// And an unknown page teaches nobody anything.
	VT_CHECK(
		!ReadDocument(W.Instance, W.Persons, W.Names, W.Papers, DocumentRules{}, 99999u, Reader, W.Instance.Now()));
}
