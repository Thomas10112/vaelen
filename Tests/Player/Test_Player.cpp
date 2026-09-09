// VAELEN - Tests/Player
// Phase 10.01: the player as a marker on one person the world already had -
// and the claim that carrying the mark changes nothing at all.
//
// STATUS: PROTOTYPE (Phase 10)

#include "Vaelen/Player/Player.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Player;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogPlayer);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			One = PlayerTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Houses->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Bridge.get());
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
		/// The lowest-numbered living person of a region, 0 when it has none.
		uint32 SomebodyIn(uint32 Region) const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						if (P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive) &&
							(Out == 0 || P.Index < Out))
						{
							Out = P.Index;
						}
					});
			return Out;
		}
		bool Take(uint32 Person) { return TakePlayer(Instance, Persons, One, Person, Instance.Now()); }
		bool Release() { return ReleasePlayer(Instance, One); }
		uint32 Played() const { return PlayerPerson(Instance, One); }
		PlayerStats Stats() const { return MeasurePlayer(Instance, Persons, One); }
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		LodTypes Lod;
		PlayerTypes One;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<LodSystem> Bridge;
	};

	/// A world grown to 300 years with its busiest region simulated person by
	/// person, which is the only kind of region a player can be somebody in.
	bool Peopled(Run& W)
	{
		if (!W.Ages.Generate(Run::Square(128), 300))
		{
			return false;
		}
		const std::vector<uint32> Ranked = W.Ranked();
		return !Ranked.empty() && RequestDetail(W.Instance, W.Lod, Ranked[0]);
	}
} // namespace

VAELEN_TEST(Player, ThePlayerIsSomebodyTheWorldAlreadyHad)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Peopled(W));
	W.Ages.Run(1); // the promotion makes the persons
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(!Ranked.empty());
	const uint32 Who = W.SomebodyIn(Ranked[0]);
	VT_REQUIRE(Who != 0);

	VT_CHECK_EQ(W.Played(), 0u);
	VT_CHECK(W.Take(Who));
	VT_CHECK_EQ(W.Played(), Who);
	const PlayerStats S = W.Stats();
	VAELEN_LOG_INFO(LogPlayer, "played person %u in region %u, %u mark(s), bad=%u", Who, S.Region, S.Marks, S.Bad);
	VT_CHECK_EQ(S.Marks, 1u);
	VT_CHECK_EQ(S.Alive, 1u);
	VT_CHECK_EQ(S.Region, Ranked[0]);
	VT_CHECK_EQ(S.Bad, 0u);

	// One player to a world: taking a second is refused rather than leaving the
	// first behind with no way to say which was meant.
	const uint32 Other = W.SomebodyIn(Ranked[0]);
	VT_CHECK(!W.Take(Other));
	VT_CHECK_EQ(W.Stats().Marks, 1u);

	// And the mark comes off.
	VT_CHECK(W.Release());
	VT_CHECK_EQ(W.Played(), 0u);
	VT_CHECK_EQ(W.Stats().Marks, 0u);
	VT_CHECK(!W.Release());
}

VAELEN_TEST(Player, CarryingTheMarkChangesNothing)
{
	// The whole architectural claim of Phase 10, tested before anything is built
	// on it: a world with a player in it and the same world without one are the
	// same world, tick for tick, until the player actually does something - and
	// nothing in 10.01 gives them a way to.
	Run Played(AelvorSeed);
	Run Empty(AelvorSeed);
	VT_REQUIRE(Peopled(Played));
	VT_REQUIRE(Peopled(Empty));
	Played.Ages.Run(1);
	Empty.Ages.Run(1);
	const std::vector<uint32> Ranked = Played.Ranked();
	VT_REQUIRE(!Ranked.empty());
	const uint32 Who = Played.SomebodyIn(Ranked[0]);
	VT_REQUIRE(Who != 0);
	VT_REQUIRE(Played.Take(Who));

	for (uint32 Year = 0; Year < 50; ++Year)
	{
		Played.Ages.Run(1);
		Empty.Ages.Run(1);
	}
	// The mark is not part of the world's state for this purpose: it is taken
	// off before the two are compared, so that the digest is of the world and
	// not of the marking.
	VT_CHECK(Played.Release());
	const Hash64 A = ComputeStateDigest(Played.Instance);
	const Hash64 B = ComputeStateDigest(Empty.Instance);
	VAELEN_LOG_INFO(LogPlayer, "fifty years played=%016llx empty=%016llx", static_cast<unsigned long long>(A),
					static_cast<unsigned long long>(B));
	VT_CHECK_MSG(A == B, "a world with a player in it runs exactly as one without");
}

VAELEN_TEST(Player, RulesAndEdges)
{
	Run W(AelvorSeed);
	VT_REQUIRE(Peopled(W));
	W.Ages.Run(1);

	// Nobody is played until somebody is, and nothing that is not a living
	// person can be.
	VT_CHECK(!W.Take(0));
	VT_CHECK(!W.Take(4000000000u));
	VT_CHECK_EQ(W.Played(), 0u);
	VT_CHECK_EQ(W.Stats().Marks, 0u);
	VT_CHECK_EQ(W.Stats().Bad, 0u);
	VT_CHECK(PlayerOf(W.Instance, W.One) == nullptr);

	// A person of a coarse region is not a person at all: there is nobody there
	// to be, only a count of them.
	const std::vector<uint32> Ranked = W.Ranked();
	VT_REQUIRE(Ranked.size() >= 2);
	VT_CHECK_EQ(W.SomebodyIn(Ranked[1]), 0u);
}

VAELEN_TEST(Player, DeterministicAndSnapshotSafe)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(Peopled(A));
	VT_REQUIRE(Peopled(B));
	A.Ages.Run(1);
	B.Ages.Run(1);
	const std::vector<uint32> Ranked = A.Ranked();
	VT_REQUIRE(!Ranked.empty());
	const uint32 Who = A.SomebodyIn(Ranked[0]);
	VT_REQUIRE(Who != 0);
	VT_REQUIRE(A.Take(Who));
	VT_REQUIRE(B.Take(B.SomebodyIn(Ranked[0])));
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);

	// The mark survives a save and a load, and still points at the same person.
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	VT_REQUIRE(!Image.empty());
	Run C(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(C.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(C.Played(), Who);
	VT_CHECK_EQ(C.Stats().Digest, A.Stats().Digest);
	VT_CHECK_EQ(C.Stats().Bad, 0u);
}
