// VAELEN - Tests/View
// Phase 13.08b's missing half: the PEOPLE, as a renderer needs them.
//
// 13.08b asks for "a person, a colony and a road drawn from the view of 13.01".
// The road arrived in 13.08a and the colony with it. The person never did: a
// RegionView carries a head COUNT, which draws a number over a province and
// cannot put one figure anywhere on it. This file is about the claim that the
// view now carries people, under the same three promises every other view in
// this module is held to - numbers only, index order, and it outlives the world.
//
// STATUS: PROTOTYPE (Phase 13)
#include "Vaelen/View/Folk.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Traits.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::View;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogFolk);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// A world with people in it and nothing else: no trade, no colony, no
	/// markets. People are what this file is about, and a fixture that builds
	/// the whole economy to test a person would hide which part put them there.
	struct Peopled
	{
		explicit Peopled(uint64 Seed) : Instance(Config(Seed)), Ages(Instance, PreHistoryRules{})
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Traits = TraitTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);

			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Minds = std::make_unique<TraitSystem>(Instance, Ages.Types(), Persons, Traits, TraitRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Houses->RunAfter("Lod");
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Minds.get());
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
		ViewSources Sources() const
		{
			ViewSources S;
			S.Types = Ages.Types();
			S.Persons = Persons;
			return S;
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

		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		TraitTypes Traits;
		LodTypes Lod;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<TraitSystem> Minds;
		std::unique_ptr<LodSystem> Bridge;
	};
} // namespace

VAELEN_TEST(Folk, APersonCarriesNoWayBackIntoTheWorld)
{
	// The same claim 13.01 made and every view since has had to keep: what a
	// renderer holds is numbers. Not a handle, not an id it could look up, not
	// a pointer it could follow. The layering rule is structural or it is a
	// promise, and a promise is what the last project broke.
	VT_CHECK(std::is_trivially_copyable<PersonView>::value);
	VT_CHECK_EQ(sizeof(PersonView), usize{40});
	VT_CHECK(std::is_trivially_copyable<PeopleStats>::value);
}

VAELEN_TEST(Folk, ThePeopleMatchTheWorldTheyWereReadFrom)
{
	Peopled W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Peopled::Square(96), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	RequestDetail(W.Instance, W.Lod, Where);
	W.Ages.Run(20);

	PeopleView V;
	TakePeopleView(W.Instance, W.Sources(), V);

	// Every materialised person is in the view, once, with their own numbers.
	uint32 InWorld = 0;
	uint32 Wrong = 0;
	W.Instance.Components()
		.GetPool(W.Persons.Person)
		.ForEach(
			[&](EntityHandle, const PersonInfo& P)
			{
				++InWorld;
				const PersonView* Seen = PersonIn(V, P.Index);
				if (Seen == nullptr || Seen->Region != P.Region || Seen->Family != P.Family ||
					Seen->Culture != P.Culture || Seen->Religion != P.Religion || Seen->Spouse != P.Spouse ||
					Seen->Sex != P.Sex || Seen->State != P.State || Seen->Identity != P.Identity)
				{
					++Wrong;
				}
			});
	VAELEN_LOG_INFO(LogFolk, "region %u detailed: %u people materialised, %u in the view, %u living", Where, InWorld,
					static_cast<uint32>(V.People.size()), V.Living);
	VT_CHECK_MSG(InWorld > 0, "a detailed region materialises its people, or this test proves nothing");
	VT_CHECK_EQ(static_cast<uint32>(V.People.size()), InWorld);
	VT_CHECK_EQ(Wrong, 0u);

	// Index order, always - a renderer keeps its own array in step with this one.
	VT_CHECK(std::is_sorted(V.People.begin(), V.People.end(),
							[](const PersonView& A, const PersonView& B) { return A.Index < B.Index; }));

	// The lookup answers honestly at both edges.
	VT_CHECK(PersonIn(V, 0) == nullptr);
	VT_CHECK(PersonIn(V, 0xffffffffu) == nullptr);

	// Everyone in the view lives in the one region that is detailed. A person in
	// a region nobody is looking at closely does not exist as a person at all,
	// and the view says so rather than inventing one.
	uint32 Elsewhere = 0;
	for (const PersonView& P : V.People)
	{
		Elsewhere += P.Region != Where ? 1u : 0u;
	}
	VT_CHECK_EQ(Elsewhere, 0u);

	const PeopleStats S = MeasurePeopleView(V);
	VT_CHECK_EQ(S.People, static_cast<uint32>(V.People.size()));
	VT_CHECK_EQ(S.Regions, 1u);
	VT_CHECK_MSG(S.Oldest > 0, "somebody has had a birthday");
	VT_CHECK_MSG(S.Oldest < 200, "and nobody is impossibly old");
}

VAELEN_TEST(Folk, AWorldNobodyLooksAtCloselyHasNoPeopleAndSaysSo)
{
	// No RequestDetail. The world has a population of thousands and not one
	// PERSON, because none was ever made. An empty view here is the truth about
	// this world, not a failure to find anything.
	Peopled W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Peopled::Square(64), 300));
	W.Ages.Run(10);

	PeopleView V;
	TakePeopleView(W.Instance, W.Sources(), V);
	VT_CHECK(V.People.empty());
	VT_CHECK_EQ(V.Living, 0u);
	VT_CHECK_EQ(V.Year, static_cast<uint32>(V.Tick / TicksPerYear));

	const PeopleStats S = MeasurePeopleView(V);
	VT_CHECK_EQ(S.People, 0u);
	VT_CHECK_EQ(S.Regions, 0u);
}

VAELEN_TEST(Folk, ThePeopleOutliveTheWorldTheyCameFrom)
{
	// The whole reason a view is a copy of numbers: it is read AFTER the world
	// that made it is gone. If this needed the world alive it would be a handle
	// with extra steps.
	PeopleView V;
	uint32 Where = 0;
	{
		auto W = std::make_unique<Peopled>(AelvorSeed);
		VT_REQUIRE(W->Ages.Generate(Peopled::Square(64), 300));
		Where = W->Busiest();
		VT_REQUIRE(Where != 0);
		RequestDetail(W->Instance, W->Lod, Where);
		W->Ages.Run(15);
		TakePeopleView(W->Instance, W->Sources(), V);
	}
	// THE WORLD IS GONE.
	VT_CHECK_MSG(!V.People.empty(), "and the people are still here");
	const PeopleStats S = MeasurePeopleView(V);
	VT_CHECK_EQ(S.People, static_cast<uint32>(V.People.size()));
	std::vector<PersonView> There;
	PeopleOfRegion(V, Where, There);
	VT_CHECK_EQ(static_cast<uint32>(There.size()), S.People);
}

VAELEN_TEST(Folk, TwoWorldsOfTheSameSeedGiveTheSamePeople)
{
	Peopled A(AelvorSeed);
	Peopled B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Peopled::Square(64), 300));
	VT_REQUIRE(B.Ages.Generate(Peopled::Square(64), 300));
	const uint32 Where = A.Busiest();
	VT_REQUIRE(Where != 0);
	VT_CHECK_EQ(B.Busiest(), Where);
	RequestDetail(A.Instance, A.Lod, Where);
	RequestDetail(B.Instance, B.Lod, Where);
	A.Ages.Run(15);
	B.Ages.Run(15);

	PeopleView P, Q;
	TakePeopleView(A.Instance, A.Sources(), P);
	TakePeopleView(B.Instance, B.Sources(), Q);
	const PeopleStats S = MeasurePeopleView(P);
	const PeopleStats T = MeasurePeopleView(Q);
	VAELEN_LOG_INFO(LogFolk, "same seed twice: %u people, %u living, oldest %u, digest %016llx", S.People, S.Living,
					S.Oldest, static_cast<unsigned long long>(S.Digest));
	VT_CHECK_MSG(S.People > 0, "there are people to compare");
	VT_CHECK_EQ(S.Digest, T.Digest);
	VT_CHECK_EQ(S.People, T.People);
	VT_CHECK_EQ(S.Living, T.Living);
	VT_CHECK_EQ(S.Oldest, T.Oldest);
}

// The question the renderer actually has, and could not ask.
//
// Written after DrawFolk, not before it, and that order is the finding. The
// view had a count of the living (PeopleView::Living) and no way to point at
// one of them: State is a raw uint8 and the enum that gives it meaning is in
// VaelenPopulation, which a renderer may not include - so the drawer could
// report how many people were alive and could not draw them.
//
// This suite checks what IsAlive is for: it separates the living from BOTH
// other states, and "not dead" is not the same answer. Population::LifeState
// has Gone, somebody who left the detailed grain of 04.06 and is kept only so
// history still has them. They are not dead and they are nowhere, and a
// renderer drawing everybody who is not dead draws people who are not there.
VAELEN_TEST(Folk, TheLivingCanBeToldApartWithoutTheKernelsEnum)
{
	PersonView Living;
	Living.State = AliveState;
	VT_CHECK_MSG(IsAlive(Living), "the value the view publishes is the one the view's own predicate accepts");

	// Every other byte is somebody the renderer must not place. Checked over
	// the whole range rather than over the two states that exist today,
	// because the failure mode here is a state ADDED later and quietly drawn.
	for (uint32 Other = 0; Other <= 255u; ++Other)
	{
		if (static_cast<uint8>(Other) == AliveState)
		{
			continue;
		}
		PersonView P;
		P.State = static_cast<uint8>(Other);
		VT_CHECK_MSG(!IsAlive(P), "anything that is not the living state is not alive");
	}

	// And the predicate agrees with the count the view took from the world,
	// which is the thing that would drift if either were ever changed alone.
	Peopled W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Peopled::Square(64), 300));
	const uint32 Where = W.Busiest();
	VT_REQUIRE(Where != 0);
	RequestDetail(W.Instance, W.Lod, Where);
	W.Ages.Run(40);

	PeopleView V;
	TakePeopleView(W.Instance, W.Sources(), V);
	uint32 Counted = 0;
	uint32 Others = 0;
	for (const PersonView& P : V.People)
	{
		Counted += IsAlive(P) ? 1u : 0u;
		Others += IsAlive(P) ? 0u : 1u;
	}
	VAELEN_LOG_INFO(LogFolk, "region %u after 40 years: %u people, %u living by the predicate, %u not", Where,
					static_cast<uint32>(V.People.size()), Counted, Others);
	VT_CHECK_EQ(Counted, V.Living);
	VT_CHECK_MSG(Others > 0, "forty years is long enough that somebody has died, or the test proves nothing");
}
