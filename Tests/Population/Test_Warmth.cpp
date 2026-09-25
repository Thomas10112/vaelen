// VAELEN - Tests/Population
// Phase 18.05: who is cold - the chill as a component declared only in a
// climate world, the cause, and the yearly judgement.
//
// STATUS: PROTOTYPE (Phase 18)
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Population/Warmth.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::WorldGen;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogWarmth);
	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	/// What a run is asked for: the type declared, the system told, the rules.
	struct Ask
	{
		bool Declared = false;
		bool Told = false;
		WarmthRules Winter;
		PreHistoryRules Ages;
	};

	/// A world with lives and needs and, when asked, warmth. The wiring is
	/// Test_Needs's, so the run not asked for warmth IS that file's world.
	struct Run
	{
		explicit Run(uint64 Seed, const Ask& A) : Instance(Config(Seed)), Ages(Instance, A.Ages)
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Needs = NeedTypes::Declare(Instance);
			if (A.Declared)
			{
				Warmth = WarmthTypes::Declare(Instance);
			}
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, LifeRules{});
			Body = std::make_unique<NeedSystem>(Instance, Ages.Types(), Persons, Needs, NeedRules{});
			if (A.Told)
			{
				Body->ObserveWinter(Warmth, A.Winter);
			}
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Body.get());
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
		/// The region with the most people, ties to the lower index; or, with
		/// Not set, the most peopled OTHER region.
		uint32 Busiest(uint32 Not = 0) const
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
						if (R.Index != Not && P != nullptr && P->Total > People)
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		bool Promote(uint32 Region)
		{
			return PromoteRegion(Instance, Ages.Types(), Persons, MaterialiseRules{}, Region, Instance.Now()) > 0;
		}
		/// The living of a region, lowest index first.
		std::vector<uint32> Living(uint32 Region) const
		{
			std::vector<uint32> Out;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						if (P.Region == Region && P.State == static_cast<uint8>(LifeState::Alive))
						{
							Out.push_back(P.Index);
						}
					});
			std::sort(Out.begin(), Out.end());
			return Out;
		}
		EntityHandle HandleOf(uint32 Person) const
		{
			EntityHandle Out;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (Out.IsNull() && P.Index == Person)
						{
							Out = H;
						}
					});
			return Out;
		}
		const PersonInfo* InfoOf(uint32 Person) const
		{
			const EntityHandle H = HandleOf(Person);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Persons.Person).TryGet(H);
		}
		const PersonNeeds* NeedsOf(uint32 Person) const
		{
			const EntityHandle H = HandleOf(Person);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Needs.Needs).TryGet(H);
		}
		const PersonWarmth* WarmthOf(uint32 Person) const
		{
			const EntityHandle H = HandleOf(Person);
			return H.IsNull() ? nullptr : Instance.Components().GetPool(Warmth.Warmth).TryGet(H);
		}
		/// The frail take more of every blow: under five, or from sixty
		/// (NeedRules::ElderFrom). The lowest-index living person of the region
		/// whose frailty is as asked, skipping the ones named.
		uint32 Somebody(uint32 Region, bool Frail, std::vector<uint32> Skip = {}) const
		{
			for (const uint32 Person : Living(Region))
			{
				if (std::find(Skip.begin(), Skip.end(), Person) != Skip.end())
				{
					continue;
				}
				const uint32 Age = AgeYears(*InfoOf(Person), Instance.Now());
				if ((Age < 5 || Age >= NeedRules{}.ElderFrom) == Frail)
				{
					return Person;
				}
			}
			return 0;
		}
		/// A dead person's index, 0 when nobody has died yet.
		uint32 Dead() const
		{
			uint32 Out = 0;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						if (Out == 0 && P.State == static_cast<uint8>(LifeState::Dead))
						{
							Out = P.Index;
						}
					});
			return Out;
		}
		/// Every living person's need bytes, by index: what ADR-0149 rule 2
		/// compares between the world with the option and the world without.
		std::map<uint32, PersonNeeds> NeedBytes() const
		{
			std::map<uint32, PersonNeeds> Out;
			Instance.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						const PersonNeeds* N = Instance.Components().GetPool(Needs.Needs).TryGet(H);
						if (N != nullptr && P.State == static_cast<uint8>(LifeState::Alive))
						{
							Out[P.Index] = *N;
						}
					});
			return Out;
		}
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		NeedTypes Needs;
		WarmthTypes Warmth;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<NeedSystem> Body;
	};

	/// A world without omens of its own where every omen strikes (Test_Needs's).
	PreHistoryRules Cursed()
	{
		PreHistoryRules R;
		for (uint32 K = 0; K < static_cast<uint32>(DisasterKind::Count); ++K)
		{
			R.Disasters.OmenPerMille[K] = 0;
		}
		R.Disasters.StrikePerMille = 1000;
		return R;
	}

	/// A world where no omen ever comes: every person of a region eats and
	/// heals alike, so a difference between two of them is the cold's alone.
	PreHistoryRules Calm()
	{
		PreHistoryRules R;
		for (uint32 K = 0; K < static_cast<uint32>(DisasterKind::Count); ++K)
		{
			R.Disasters.OmenPerMille[K] = 0;
		}
		R.Disasters.StrikePerMille = 0;
		return R;
	}

	/// Queues an omen of full risk on a region: it strikes at the next yearly tick.
	bool Curse(Run& W, uint32 Region, DisasterKind Kind)
	{
		bool Queued = false;
		W.Instance.Components()
			.GetPool(W.Ages.Types().Disasters.State)
			.ForEach(
				[&](EntityHandle, DisasterState& S)
				{
					if (!Queued && S.PendingCount < DisasterState::MaxPending)
					{
						S.Pending[S.PendingCount] = PendingOmen{Region, static_cast<uint32>(Kind), 1000, 0, 0};
						++S.PendingCount;
						Queued = true;
					}
				});
		return Queued;
	}

	struct Death
	{
		uint32 Person = 0;
		uint32 Cause = 0;
		PersistentId CauseEvent;
	};

	std::vector<Death> DeathsSince(const World& W, uint32 Region, usize From)
	{
		std::vector<Death> Out;
		const std::vector<Event>& Events = W.Log().All();
		for (usize i = From; i < Events.size(); ++i)
		{
			const Event& E = Events[i];
			if (E.Is(PersonDiedEvent) && E.Get<PersonPayload>().Region == Region)
			{
				const PersonPayload P = E.Get<PersonPayload>();
				Out.push_back(Death{P.Person, P.Other, E.Cause});
			}
		}
		return Out;
	}

	/// The word of a cause, so a failing check names it rather than numbering it.
	const char* CauseWord(uint32 Cause)
	{
		switch (static_cast<DeathCause>(Cause))
		{
		case DeathCause::Natural:
			return "of age";
		case DeathCause::Famine:
			return "of famine";
		case DeathCause::Starvation:
			return "of hunger";
		case DeathCause::Plague:
			return "of plague";
		case DeathCause::Cold:
			return "of the cold";
		}
		return "of a cause without a word";
	}

	bool SameBytes(const PersonNeeds& A, const PersonNeeds& B)
	{
		return A.Food == B.Food && A.Health == B.Health && A.Rest == B.Rest && A.Hungry == B.Hungry &&
			   A.Reserved == B.Reserved;
	}
} // namespace

VAELEN_TEST(Warmth, DefaultsAndTheComponentAreSane)
{
	VT_CHECK_EQ(static_cast<uint32>(sizeof(PersonWarmth)), 8u);
	VT_CHECK_EQ(static_cast<uint32>(sizeof(WinterPayload)), 24u); // 18.06: People and Usual joined it
	const PersonWarmth Warm;
	VT_CHECK_EQ(Warm.Chill, 0u);
	VT_CHECK_EQ(Warm.ColdYears, 0u);
	VT_CHECK_EQ(Warm.Clad, 0u);
	VT_CHECK_EQ(Warm.Reserved, 0u);
	VT_CHECK_EQ(Warm.Reserved2, 0u);
	const WarmthRules R;
	VT_CHECK_EQ(R.ChillLine, 100u);
	VT_CHECK_EQ(R.ColdDamage, 40u);
	VT_CHECK_EQ(R.ChillRecovery, 200u);
	VT_CHECK(R.ChillRecovery > R.ChillLine); // a year spends more than a line of chill
	VT_CHECK_EQ(static_cast<uint32>(DeathCause::Cold), 4u);
	VT_CHECK(WinterEvent.IsValid());
	VT_CHECK(WinterForeseenEvent.IsValid());
	VT_CHECK(WinterEvent.TypeHash != WinterForeseenEvent.TypeHash);
	VT_CHECK_EQ(WinterEvent.TypeHash, HashString("Winter"));
	VT_CHECK_EQ(WinterForeseenEvent.TypeHash, HashString("WinterForeseen"));
}

VAELEN_TEST(Warmth, TheChillSaturatesTheWarmthFloorsAndNobodyElseIsTouched)
{
	Run W(AelvorSeed, Ask{true, true, WarmthRules{}, Calm()});
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	const std::vector<uint32> People = W.Living(Region);
	VT_REQUIRE(!People.empty());
	const uint32 Who = People.front();
	// Promoted since the last year's turn: nobody carries warmth yet, and the
	// verbs say so rather than inventing a component.
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Who, 10), 0u);
	VT_CHECK_EQ(MeasureWarmth(W.Instance, W.Persons, W.Warmth, WarmthRules{}, Region).WithWarmth, 0u);
	W.Ages.Run(1);
	// After the year's turn everyone alive carries it, warm.
	const LifeStats L = MeasureLives(W.Instance, W.Persons, Region, W.Instance.Now());
	WarmthStats S = MeasureWarmth(W.Instance, W.Persons, W.Warmth, WarmthRules{}, Region);
	VT_CHECK_EQ(S.WithWarmth, L.Alive);
	VT_CHECK(S.WithWarmth > 0);
	VT_CHECK_EQ(S.ChillSum, 0ull);
	VT_CHECK_EQ(S.Cold, 0u);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Who, 250), 250u);
	VT_REQUIRE(W.WarmthOf(Who) != nullptr);
	VT_CHECK_EQ(W.WarmthOf(Who)->Chill, 250u);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Who, 10), 5u); // saturates at frozen through
	VT_CHECK_EQ(W.WarmthOf(Who)->Chill, 255u);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Who, 1), 0u);
	S = MeasureWarmth(W.Instance, W.Persons, W.Warmth, WarmthRules{}, Region);
	VT_CHECK_EQ(S.Cold, 1u); // one person's chill is nobody else's
	VT_CHECK_EQ(S.ChillSum, 255ull);
	VT_CHECK_EQ(WarmPerson(W.Instance, W.Persons, W.Warmth, Who, 300), 255u); // floors at warm
	VT_CHECK_EQ(W.WarmthOf(Who)->Chill, 0u);
	VT_CHECK_EQ(WarmPerson(W.Instance, W.Persons, W.Warmth, Who, 1), 0u);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Who, 0), 0u);
	// Nobody else: an unknown person, a dead one, and one of a region promoted
	// since the turn, who carries no warmth yet.
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, 0, 10), 0u);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, 1u << 30, 10), 0u);
	const uint32 Gone = W.Dead();
	VT_REQUIRE(Gone != 0);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Gone, 10), 0u);
	VT_CHECK_EQ(WarmPerson(W.Instance, W.Persons, W.Warmth, Gone, 10), 0u);
	const uint32 Other = W.Busiest(Region);
	VT_REQUIRE(Other != 0 && Other != Region);
	VT_REQUIRE(W.Promote(Other));
	const std::vector<uint32> Newcomers = W.Living(Other);
	VT_REQUIRE(!Newcomers.empty());
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Newcomers.front(), 10), 0u);
	VT_CHECK_EQ(MeasureWarmth(W.Instance, W.Persons, W.Warmth, WarmthRules{}, 0).WithWarmth, S.WithWarmth);
	VAELEN_LOG_INFO(LogWarmth,
					"region %u: %u alive carry warmth; region %u promoted after the turn: %u people, none yet", Region,
					S.WithWarmth, Other, static_cast<uint32>(Newcomers.size()));
}

VAELEN_TEST(Warmth, TheColdKillsWithACauseAndTheWintersName)
{
	// A cold year costs at least ColdDamage; 255 of it kills anyone, so the
	// deaths below are certain and the case is about their cause.
	Run W(AelvorSeed, Ask{true, true, WarmthRules{100, 255, 200}, Calm()});
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	W.Ages.Run(1); // the turn that gives everyone warmth
	std::vector<uint32> People = W.Living(Region);
	VT_REQUIRE(People.size() >= 6);
	usize Mark = W.Instance.Log().Count();
	// This year's winter, planted where 18.06 will publish it: at the year's
	// turn, before the judgement. Three are frozen through.
	const PersistentId Winter =
		W.Instance.Events().Publish(W.Instance.Now(), WinterEvent, WinterPayload{Region, 2u, 3000u, 0u});
	VT_REQUIRE(Winter.IsValid());
	for (usize i = 0; i < 3; ++i)
	{
		VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, People[i], 255), 255u);
	}
	W.Ages.Run(1);
	uint32 Cold = 0;
	for (const Death& D : DeathsSince(W.Instance, Region, Mark))
	{
		if (D.Cause == static_cast<uint32>(DeathCause::Cold))
		{
			++Cold;
			VT_CHECK_MSG(D.CauseEvent == Winter, "person %u died of the cold with cause %llu, not the winter %llu",
						 D.Person, static_cast<unsigned long long>(D.CauseEvent.Value),
						 static_cast<unsigned long long>(Winter.Value));
			VT_CHECK_MSG(std::find(People.begin(), People.begin() + 3, D.Person) != People.begin() + 3,
						 "person %u died of the cold and was not chilled", D.Person);
		}
		else
		{
			// A calm world: whoever else died, died of age, named by nothing.
			VT_CHECK_MSG(D.Cause == static_cast<uint32>(DeathCause::Natural), "person %u died %s", D.Person,
						 CauseWord(D.Cause));
			VT_CHECK(!D.CauseEvent.IsValid());
		}
	}
	VT_CHECK_EQ(Cold, 3u);
	for (usize i = 0; i < 3; ++i)
	{
		VT_REQUIRE(W.NeedsOf(People[i]) != nullptr);
		VT_CHECK_EQ(W.NeedsOf(People[i])->Health, 0u);
		VT_CHECK_EQ(W.InfoOf(People[i])->State, static_cast<uint8>(LifeState::Dead));
	}
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region)); // the counts followed the dead
	VT_CHECK_EQ(MeasureNeeds(W.Instance, W.Persons, W.Needs, Region).ColdDeaths, 3u);
	VT_CHECK_EQ(MeasureWarmth(W.Instance, W.Persons, W.Warmth, WarmthRules{}, Region).ColdDeaths, 3u);
	VT_CHECK_EQ(W.Instance.Log().CountCausedBy(Winter), 3ull);
	// A year later that winter is last year's. Three more frozen through die of
	// the cold, which is real even when no winter names it.
	Mark = W.Instance.Log().Count();
	People = W.Living(Region);
	VT_REQUIRE(People.size() >= 3);
	for (usize i = 0; i < 3; ++i)
	{
		VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, People[i], 255), 255u);
	}
	W.Ages.Run(1);
	Cold = 0;
	for (const Death& D : DeathsSince(W.Instance, Region, Mark))
	{
		if (D.Cause == static_cast<uint32>(DeathCause::Cold))
		{
			++Cold;
			VT_CHECK_MSG(!D.CauseEvent.IsValid(), "person %u died of the cold named by %llu, a year after the winter",
						 D.Person, static_cast<unsigned long long>(D.CauseEvent.Value));
		}
	}
	VT_CHECK_EQ(Cold, 3u);
	VT_CHECK_EQ(W.Instance.Log().CountCausedBy(Winter), 3ull); // the old winter named nobody new
	VT_CHECK(IsConsistent(W.Instance, W.Ages.Types(), W.Persons, Region));
	VT_CHECK_EQ(MeasureNeeds(W.Instance, W.Persons, W.Needs, Region).ColdDeaths, 6u);
}

VAELEN_TEST(Warmth, TheFrailTakeMoreAndTheYearWarmsWhomItJudged)
{
	Run W(AelvorSeed, Ask{true, true, WarmthRules{}, Calm()});
	VT_REQUIRE(W.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = W.Busiest();
	VT_REQUIRE(W.Promote(Region));
	W.Ages.Run(1);
	const uint32 Hale = W.Somebody(Region, false);
	const uint32 Frail = W.Somebody(Region, true);
	const uint32 Control = W.Somebody(Region, false, {Hale});
	VT_REQUIRE(Hale != 0 && Frail != 0 && Control != 0);
	// Every person of the calm region ate and healed alike this year, so the
	// control's health is what the two would have had: the difference is the
	// cold's alone. One chill over the line: ColdDamage plus a draw below two.
	// The line itself is not cold - the control stands exactly on it.
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Hale, 101), 101u);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Frail, 101), 101u);
	VT_CHECK_EQ(ChillPerson(W.Instance, W.Persons, W.Warmth, Control, 100), 100u);
	VT_REQUIRE(W.NeedsOf(Hale)->Health == W.NeedsOf(Control)->Health);
	VT_REQUIRE(W.NeedsOf(Frail)->Health == W.NeedsOf(Control)->Health);
	W.Ages.Run(1);
	VT_REQUIRE(W.InfoOf(Hale)->State == static_cast<uint8>(LifeState::Alive));
	VT_REQUIRE(W.InfoOf(Frail)->State == static_cast<uint8>(LifeState::Alive));
	VT_REQUIRE(W.InfoOf(Control)->State == static_cast<uint8>(LifeState::Alive));
	const int32 C = W.NeedsOf(Control)->Health;
	const int32 H = W.NeedsOf(Hale)->Health;
	const int32 F = W.NeedsOf(Frail)->Health;
	VT_CHECK_MSG(C - H == 40 || C - H == 41, "hale: the control has %d, the judged %d", C, H);
	VT_CHECK_MSG(C - F == 52 || C - F == 53, "frail (age %u): the control has %d, the judged %d",
				 AgeYears(*W.InfoOf(Frail), W.Instance.Now()), C, F);
	VT_CHECK_EQ(W.WarmthOf(Hale)->ColdYears, 1u);
	VT_CHECK_EQ(W.WarmthOf(Frail)->ColdYears, 1u);
	VT_CHECK_EQ(W.WarmthOf(Control)->ColdYears, 0u);
	// The year spent the chill of everyone it judged, cold or not.
	VT_CHECK_EQ(W.WarmthOf(Hale)->Chill, 0u);
	VT_CHECK_EQ(W.WarmthOf(Control)->Chill, 0u);
	// Next year nobody is cold: the streak ends and health comes back.
	W.Ages.Run(1);
	VT_REQUIRE(W.InfoOf(Hale)->State == static_cast<uint8>(LifeState::Alive));
	VT_CHECK_EQ(W.WarmthOf(Hale)->ColdYears, 0u);
	VT_CHECK_EQ(static_cast<int32>(W.NeedsOf(Hale)->Health), std::min(255, H + 40));
	VAELEN_LOG_INFO(LogWarmth, "control %d, hale %d, frail %d after one cold year (line 100, chill 101)", C, H, F);
}

VAELEN_TEST(Warmth, TheChillIsStateAndSurvivesAnImage)
{
	Run A(AelvorSeed, Ask{true, true, WarmthRules{}, Calm()});
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_REQUIRE(A.Promote(Region));
	A.Ages.Run(1);
	const uint32 Who = A.Living(Region).front();
	VT_CHECK_EQ(ChillPerson(A.Instance, A.Persons, A.Warmth, Who, 77), 77u);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed, Ask{true, true, WarmthRules{}, Calm()});
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_REQUIRE(R.WarmthOf(Who) != nullptr);
	VT_CHECK_EQ(R.WarmthOf(Who)->Chill, 77u);
	A.Ages.Run(3);
	R.Ages.Run(3);
	VT_CHECK_DIGEST_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_DIGEST_EQ(R.Instance.Log().Digest(), A.Instance.Log().Digest());
	// A world that never declared the type cannot take the image: the layout
	// differs, which is the door ADR-0153 chose over a promise.
	Run N(AelvorSeed, Ask{false, false, WarmthRules{}, Calm()});
	VT_CHECK(LoadSnapshot(N.Instance, Image.data(), Image.size()) != SnapshotResult::Ok);
}

VAELEN_TEST(Warmth, AWinterThatChillsNobodyLeavesEveryNeedByteAsItWas)
{
	// ADR-0149 rule 2: the world with the option and the world without differ
	// by the option alone. A: never told. B: declared and told, nobody
	// chilled. C: declared and told with the line at frozen through, and one
	// person frozen through every year - a chill that is never cold.
	Run A(AelvorSeed, Ask{false, false, WarmthRules{}, Cursed()});
	Run B(AelvorSeed, Ask{true, true, WarmthRules{}, Cursed()});
	Run C(AelvorSeed, Ask{true, true, WarmthRules{255, 40, 200}, Cursed()});
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(C.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_REQUIRE(Region == B.Busiest() && Region == C.Busiest());
	VT_REQUIRE(A.Promote(Region) && B.Promote(Region) && C.Promote(Region));
	uint32 Carried = 0;
	for (uint32 Year = 1; Year <= 6; ++Year)
	{
		const DisasterKind Kind = Year % 2 == 0 ? DisasterKind::Plague : DisasterKind::Drought;
		VT_REQUIRE(Curse(A, Region, Kind) && Curse(B, Region, Kind) && Curse(C, Region, Kind));
		const std::vector<uint32> People = C.Living(Region);
		VT_REQUIRE(!People.empty());
		const uint32 Who = People.front();
		const uint32 Given = ChillPerson(C.Instance, C.Persons, C.Warmth, Who, 255);
		// Nobody carries warmth before the first turn; after it the person is
		// frozen through from warm (255) or from the 55 the last year left.
		VT_CHECK_MSG(Year == 1 ? Given == 0 : (Given == 255 || Given == 200), "year %u: person %u took %u of chill",
					 Year, Who, Given);
		A.Ages.Run(1);
		B.Ages.Run(1);
		C.Ages.Run(1);
		if (C.InfoOf(Who)->State == static_cast<uint8>(LifeState::Alive) && Given > 0)
		{
			// The year judged the chill (255, at the line, not cold) and spent
			// 200 of it: the schedule to the unit, the streak never started.
			VT_CHECK_EQ(C.WarmthOf(Who)->Chill, 55u);
			VT_CHECK_EQ(C.WarmthOf(Who)->ColdYears, 0u);
			++Carried;
		}
	}
	VT_CHECK_MSG(Carried >= 3u, "only %u of the five chills were carried through a year alive", Carried);
	const std::map<uint32, PersonNeeds> Was = A.NeedBytes();
	const std::map<uint32, PersonNeeds> Told = B.NeedBytes();
	const std::map<uint32, PersonNeeds> Frozen = C.NeedBytes();
	VT_CHECK(Was.size() > 10);
	VT_CHECK_EQ(static_cast<uint32>(Told.size()), static_cast<uint32>(Was.size()));
	VT_CHECK_EQ(static_cast<uint32>(Frozen.size()), static_cast<uint32>(Was.size()));
	uint32 Differ = 0;
	for (const auto& [Person, N] : Was)
	{
		const auto T = Told.find(Person);
		const auto F = Frozen.find(Person);
		const bool Same = T != Told.end() && F != Frozen.end() && SameBytes(N, T->second) && SameBytes(N, F->second);
		Differ += Same ? 0u : 1u;
	}
	VT_CHECK_EQ(Differ, 0u);
	VT_CHECK_DIGEST_EQ(A.Instance.Log().Digest(), B.Instance.Log().Digest());
	VT_CHECK_DIGEST_EQ(A.Instance.Log().Digest(), C.Instance.Log().Digest());
	VT_CHECK(MeasureNeeds(A.Instance, A.Persons, A.Needs, Region).CausedDeaths > 0); // the curse was felt
	VT_CHECK_EQ(MeasureNeeds(B.Instance, B.Persons, B.Needs, Region).ColdDeaths, 0u);
	VT_CHECK_EQ(MeasureNeeds(C.Instance, C.Persons, C.Needs, Region).ColdDeaths, 0u);
	const WarmthStats S = MeasureWarmth(C.Instance, C.Persons, C.Warmth, WarmthRules{255, 40, 200}, Region);
	VT_CHECK_EQ(S.Cold, 0u); // a chill at the line is never cold
	VT_CHECK(S.WithWarmth > 0);
	// The declared type moves the state digest while the log stays: the reason
	// it is optional, seen. And the chill is state.
	VT_CHECK(ComputeStateDigest(A.Instance) != ComputeStateDigest(B.Instance));
	VT_CHECK(ComputeStateDigest(B.Instance) != ComputeStateDigest(C.Instance));
	VT_CHECK(IsConsistent(A.Instance, A.Ages.Types(), A.Persons, Region));
	VT_CHECK(IsConsistent(B.Instance, B.Ages.Types(), B.Persons, Region));
	VT_CHECK(IsConsistent(C.Instance, C.Ages.Types(), C.Persons, Region));
	VAELEN_LOG_INFO(
		LogWarmth, "%u people, %u need bytes differ between never-told, told and frozen-at-the-line; %u chills carried",
		static_cast<uint32>(Was.size()), Differ, Carried);
}
