// VAELEN - Tests/Player
// Phase 10.02: the enslaved start - a life the world already made, found rather
// than invented, and the search that is allowed to come back empty.
//
// STATUS: PROTOTYPE (Phase 10)

#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"
#include "Vaelen/Society/Organizations.h"
#include "Vaelen/Society/Standing.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Player;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogStart);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, BondageRules InBonds = BondageRules{})
			: Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Organizations = OrganizationTypes::Declare(Instance);
			Standing = StandingTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			Bondage = BondageTypes::Declare(Instance);
			One = PlayerTypes::Declare(Instance);
			First = StartTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Orgs = std::make_unique<OrganizationSystem>(Instance, Ages.Types(), Persons, Families, Traits,
														Organizations, OrganizationRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Ranks = std::make_unique<StandingSystem>(Instance, Ages.Types(), Persons, Families, Traits, Organizations,
													 Standing, StandingRules{});
			Bonds = std::make_unique<BondageSystem>(Instance, Ages.Types(), Persons, Norms, Standing, Bondage, InBonds);
			Houses->RunAfter("Lod");
			Orgs->RunAfter("Lod");
			Orgs->RunAfter("Traits");
			Bonds->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Orgs.get());
			Instance.Systems().Add(Customs.get());
			Instance.Systems().Add(Ranks.get());
			Instance.Systems().Add(Bonds.get());
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
		std::vector<uint32> Ranked() const
		{
			std::vector<std::pair<uint32, uint32>> All;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (P != nullptr && P->Total > 0)
						{
							All.push_back({P->Total, R.Index});
						}
					});
			std::sort(All.begin(), All.end(), [](const auto& A, const auto& B)
					  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
			std::vector<uint32> Out;
			for (const auto& [People, Index] : All)
			{
				Out.push_back(Index);
			}
			return Out;
		}
		uint32 Begin(StartRules R = StartRules{})
		{
			return BeginEnslaved(Instance, Ages.Types(), Persons, Bondage, Standing, One, First, R, Instance.Now());
		}
		const PlayerStart* Started() const { return StartOf(Instance, First); }
		StartStats Stats() const { return MeasureStart(Instance, Persons, Bondage, One, First); }
		uint32 Played() const { return PlayerPerson(Instance, One); }
		const BondState* Bond(uint32 Person) const { return BondOf(Instance, Persons, Bondage, Person); }
		/// How many living people of the detailed regions are bound at all.
		uint32 BoundCount() const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.State != static_cast<uint8>(LifeState::Alive))
						{
							return;
						}
						const BondState* B = Instance.Components().GetPool(Bondage.Bond).TryGet(H);
						Out += B != nullptr && B->Kind != static_cast<uint8>(BondKind::Free) ? 1u : 0u;
					});
			return Out;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		OrganizationTypes Organizations;
		StandingTypes Standing;
		NormTypes Norms;
		BondageTypes Bondage;
		PlayerTypes One;
		StartTypes First;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<OrganizationSystem> Orgs;
		std::unique_ptr<NormSystem> Customs;
		std::unique_ptr<StandingSystem> Ranks;
		std::unique_ptr<BondageSystem> Bonds;
	};

	/// A world grown to 300 years with its two busiest regions simulated person
	/// by person, then Years more so that 05.04 has had time to bind people.
	bool Grown(Run& W, uint32 Years)
	{
		if (!W.Ages.Generate(Run::Square(128), 300))
		{
			return false;
		}
		const std::vector<uint32> Ranked = W.Ranked();
		if (Ranked.size() < 2 || !RequestDetail(W.Instance, W.Lod, Ranked[0]) ||
			!RequestDetail(W.Instance, W.Lod, Ranked[1]))
		{
			return false;
		}
		W.Ages.Run(Years);
		return true;
	}
} // namespace

VAELEN_TEST(Start, ALifeBeginsBound)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Grown(W, 60));
	VAELEN_LOG_INFO(LogStart, "%u bound people to choose from", W.BoundCount());
	VT_REQUIRE(W.BoundCount() > 0);

	StartRules Anywhere;
	Anywhere.PreferOre = 0; // ore is 10.02's preference, not its subject
	const uint32 Who = W.Begin(Anywhere);
	VT_REQUIRE(Who != 0);
	VT_CHECK_EQ(W.Played(), Who);

	const PlayerStart* Life = W.Started();
	VT_REQUIRE(Life != nullptr);
	VAELEN_LOG_INFO(LogStart, "person %u, region %u, bond %s, holder %u, family %u, standing %u, aged %u", Life->Person,
					Life->Region, BondKindName(static_cast<BondKind>(Life->Bond)), Life->Holder, Life->Family,
					Life->Standing, Life->Age);
	VT_CHECK_EQ(Life->Person, Who);
	VT_CHECK_MSG(Life->Bond != static_cast<uint32>(BondKind::Free), "the life this start is, is a bound one");
	const BondState* Bond = W.Bond(Who);
	VT_REQUIRE(Bond != nullptr);
	VT_CHECK_EQ(Life->Bond, Bond->Kind);
	VT_CHECK_EQ(Life->Holder, Bond->Holder);
	VT_CHECK(Life->Age >= StartRules{}.FromAge && Life->Age <= StartRules{}.ToAge);
	const StartStats S = W.Stats();
	VT_CHECK_EQ(S.Started, 1u);
	VT_CHECK_EQ(S.StillBound, 1u);
	VT_CHECK_EQ(S.Bad, 0u);

	// A life is taken up once.
	VT_CHECK_EQ(W.Begin(Anywhere), 0u);
	VT_CHECK_EQ(W.Stats().Started, 1u);
}

VAELEN_TEST(Start, TheWorldOffersTheLifeOrItDoesNot)
{
	// A world where nobody is ever bound offers nobody, and the start writes
	// nothing rather than inventing a bound person to be.
	BondageRules Never;
	Never.DebtPerMille = 0;
	Never.BirthFollowsMother = 0;
	Run Free(AelvorSeed, Never);
	VT_REQUIRE(Grown(Free, 60));
	StartRules Anywhere;
	Anywhere.PreferOre = 0;
	VAELEN_LOG_INFO(LogStart, "a world that binds nobody: %u bound", Free.BoundCount());
	VT_CHECK_EQ(Free.Begin(Anywhere), 0u);
	VT_CHECK_EQ(Free.Played(), 0u);
	VT_CHECK(Free.Started() == nullptr);
	VT_CHECK_EQ(Free.Stats().Started, 0u);
	VT_CHECK_EQ(Free.Stats().Bad, 0u);

	// And the same world takes anybody when the start does not ask for a bond.
	StartRules Anybody;
	Anybody.PreferOre = 0;
	Anybody.WantBound = 0;
	const uint32 Who = Free.Begin(Anybody);
	VT_CHECK_MSG(Who != 0, "a world with people in it offers somebody when nothing is asked of them");
	VT_CHECK_EQ(Free.Played(), Who);
}

VAELEN_TEST(Start, RulesAndEdges)
{
	// An age nobody has: nobody is offered.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 60));
		StartRules Impossible;
		Impossible.PreferOre = 0;
		Impossible.FromAge = 4000000000u;
		Impossible.ToAge = 4000000001u;
		VT_CHECK_EQ(W.Begin(Impossible), 0u);
		VT_CHECK_EQ(W.Played(), 0u);
	}
	// Ground with ore under it is a PREFERENCE and not a requirement, and the
	// record says which it got. At AELVOR 128 the two busiest regions have no
	// ore under them at all, so the default start lands off it and says so -
	// which is the honest answer, not a start that refuses to happen.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(Grown(W, 60));
		const uint32 Who = W.Begin(StartRules{});
		VT_REQUIRE(Who != 0); // the default start finds a life even where the ore is not
		const PlayerStart* Life = W.Started();
		VT_REQUIRE(Life != nullptr);
		VAELEN_LOG_INFO(LogStart, "default start: person %u in region %u, on ore=%u", Life->Person, Life->Region,
						Life->OnOre);
		VT_CHECK(Life->OnOre == 0u || Life->OnOre == 1u);
		VT_CHECK_EQ(W.Stats().Bad, 0u);
	}
	// Nothing is known about a life that was never taken up.
	{
		Run W(AelvorSeed);
		VT_REQUIRE(W.Ages.Generate(Run::Square(64), 60));
		VT_CHECK(W.Started() == nullptr);
		const StartStats S = W.Stats();
		VT_CHECK_EQ(S.Started, 0u);
		VT_CHECK_EQ(S.Bad, 0u);
	}
}

VAELEN_TEST(Start, DeterministicAndSnapshotSafe)
{
	StartRules Anywhere;
	Anywhere.PreferOre = 0;
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(Grown(A, 60));
	VT_REQUIRE(Grown(B, 60));
	const uint32 Who = A.Begin(Anywhere);
	VT_REQUIRE(Who != 0);
	VT_CHECK_MSG(B.Begin(Anywhere) == Who, "the same world hands over the same life twice");
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);

	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	VT_REQUIRE(!Image.empty());
	Run C(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(C.Played(), Who);
	VT_REQUIRE(C.Started() != nullptr);
	VT_CHECK_EQ(C.Started()->Person, Who);
	VT_CHECK_EQ(C.Stats().Digest, A.Stats().Digest);

	// The life goes on being a life: the world runs and the record of its first
	// moment does not change, because it is a record and not a rule.
	const PlayerStart Was = *A.Started();
	A.Ages.Run(20);
	VT_REQUIRE(A.Started() != nullptr);
	VT_CHECK_EQ(A.Started()->Began, Was.Began);
	VT_CHECK_EQ(A.Started()->Age, Was.Age);
	VT_CHECK_EQ(A.Started()->Bond, Was.Bond);
}
