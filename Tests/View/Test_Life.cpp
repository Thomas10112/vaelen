// VAELEN - Tests/View
// Phase 14.04: one played life, as the screen that shows it needs it.
//
// The claim of the leaf is the claim of every view: a flat block of numbers
// and names that a renderer can copy, hash and outlive the world with, every
// one of which equals what the kernel says through its own accessors. The
// world it is taken from is a played one, through Run::Aelvor (14.03): the
// first View test to take up a person rather than declare the Player types by
// hand.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Take.h"

#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

using namespace Vaelen;
using namespace Vaelen::Run;
using namespace Vaelen::View;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogLife);

	Options Small()
	{
		Options O;
		O.Size = 96;
		O.PreHistory = 240;
		O.Years = 60;
		O.Play = true;
		return O;
	}

	Player::StartRules Anywhere()
	{
		Player::StartRules R;
		R.PreferOre = 0;
		R.ToAge = 25;
		R.WantBound = 0;
		return R;
	}

	/// Printable ASCII, NUL-terminated, and nothing hidden after the NUL.
	bool CleanName(const char (&S)[LifeNameBytes])
	{
		bool Ended = false;
		for (usize i = 0; i < LifeNameBytes; ++i)
		{
			if (Ended)
			{
				if (S[i] != '\0')
				{
					return false;
				}
				continue;
			}
			if (S[i] == '\0')
			{
				Ended = true;
				continue;
			}
			if (S[i] < 0x20 || S[i] > 0x7e)
			{
				return false;
			}
		}
		return Ended;
	}

	std::string Named(const Aelvor& A, uint32 Person)
	{
		std::string S;
		Population::NamePerson(A.Instance(), A.Ages(), A.Handles().Persons, Person, S);
		return S;
	}

	double Ms(std::chrono::steady_clock::time_point T0)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	}
} // namespace

VAELEN_TEST(Life, TheViewIsFlatAndItsPaddingIsNamed)
{
	VT_CHECK(std::is_trivially_copyable<LifeView>::value);
	VT_CHECK(std::is_standard_layout<LifeView>::value);
	VT_CHECK_EQ(sizeof(KnownView), usize{48});
	VT_CHECK_EQ(sizeof(CompanyView), usize{44});
	VT_CHECK_EQ(sizeof(LifeView), usize{1608});
	const LifeView Empty;
	const LifeStats S = MeasureLifeView(Empty);
	VT_CHECK_EQ(S.Bytes, 1608u);
	VT_CHECK_EQ(S.Named, 0u);
	VT_CHECK_EQ(S.Listed, 0u);
	const LifeView Other;
	VT_CHECK_EQ(MeasureLifeView(Other).Digest, S.Digest);
	VT_CHECK(CleanName(Empty.Name));
}

VAELEN_TEST(Life, NobodyPlayedIsAnEmptyLife)
{
	// A world without the life types, and a played world with nobody taken
	// up: Person = 0, every name empty, the clock and the costs still there.
	Options O = Small();
	O.Play = false;
	Aelvor A(O);
	VT_REQUIRE(A.Begin());
	WorldGen::RegionGraphCache Ways;
	LifeView V;
	TakeLifeView(A.Instance(), A.Sources(), Ways, V);
	VT_CHECK(!A.Sources().HasLife);
	VT_CHECK_EQ(V.Person, 0u);
	VT_CHECK_EQ(V.Region, 0u);
	VT_CHECK_EQ(V.Name[0], '\0');
	VT_CHECK_EQ(V.Tick, A.Now());
	VT_CHECK_EQ(V.Year, static_cast<uint32>(A.Now() / History::TicksPerYear));
	VT_CHECK_EQ(V.Cost[static_cast<usize>(Player::Intent::Work)], Player::OrderRules{}.HoursOf[2]);
	VT_CHECK_EQ(V.NearCount, 0u);
	VT_CHECK_EQ(V.CompanyCount, 0u);
	VT_CHECK_MSG(Ways.Builds() == 0, "nobody played, no graph built");
	VT_CHECK_EQ(MeasureLifeView(V).Named, 0u);

	Aelvor B(Small());
	VT_REQUIRE(B.Begin());
	VT_CHECK(B.Sources().HasLife);
	TakeLifeView(B.Instance(), B.Sources(), Ways, V);
	VT_CHECK_EQ(V.Person, 0u);
	VT_CHECK_EQ(V.Name[0], '\0');
	VT_CHECK_EQ(V.Held, 0u);
}

VAELEN_TEST(Life, EveryNumberIsTheKernelsAndSixtyTakesMoveNothing)
{
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const uint32 Who = A.TakeUp(Anywhere());
	VT_REQUIRE(Who != 0);
	const Wired& T = A.Handles();
	const World& W = A.Instance();
	const Population::PersonInfo* P = Population::FindPerson(W, T.Persons, Who);
	VT_REQUIRE(P != nullptr);
	const uint32 Home = P->Region;

	// A neighbour simulated person by person, so that Near has somewhere to
	// point: the Run details one region, and a Move needs a detailed one. The
	// kernel's own way - ask the bridge (04.06) and let the days pass until it
	// honours the request; PromoteRegion materialises people but does not
	// mark the region detailed, and the Move rule reads the mark.
	WorldGen::RegionGraphCache Ways;
	uint32 Promoted = 0;
	uint32 Waited = 0;
	{
		const WorldGen::RegionGraph& G = Ways.Of(W.Map(), A.Ages().World.Regions);
		VT_REQUIRE(Home < G.Neighbours.size());
		VT_REQUIRE(!G.Neighbours[Home].empty());
		const uint32 N = G.Neighbours[Home][0];
		VT_CHECK(Population::RequestDetail(A.Instance(), T.Lod, N) ||
				 Population::IsDetailed(W, A.Ages(), T.Persons, N));
		for (; Waited < 400 && !Population::IsDetailed(W, A.Ages(), T.Persons, N); ++Waited)
		{
			A.Day();
		}
		Promoted = Population::IsDetailed(W, A.Ages(), T.Persons, N) ? N : 0;
	}
	VT_CHECK_MSG(Promoted != 0, "a neighbour of region %u is detailed after %u day(s)", Home, Waited);
	VT_CHECK_EQ(Ways.Builds(), 1u);

	// A few things meant and a day turned, so the queue, the hours and the
	// regard hold numbers rather than zeros.
	Player::PlayerCommand C;
	C.Kind = static_cast<uint8>(Player::Intent::Work);
	C.Amount = 1;
	C.Issued = A.Now(); // Aelvor::Submit stamps nothing: unstamped, an order is Stale by the time it is read
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	C.Kind = static_cast<uint8>(Player::Intent::Speak);
	C.Target = 0; // nobody: refused by the world, which is a number too
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	A.Day();
	C.Kind = static_cast<uint8>(Player::Intent::Rest);
	C.Target = 0;
	C.Issued = A.Now();
	VT_CHECK(A.Submit(C) == Player::Refusal::None);

	const auto T0 = std::chrono::steady_clock::now();
	LifeView V;
	TakeLifeView(W, A.Sources(), Ways, V);
	const double Took = Ms(T0);

	// Who, and where.
	P = Population::FindPerson(W, T.Persons, Who);
	VT_REQUIRE(P != nullptr);
	VT_CHECK_EQ(V.Person, Who);
	VT_CHECK_EQ(V.Region, P->Region);
	VT_CHECK_EQ(V.Alive, A.PlayedAlive() ? 1u : 0u);
	VT_CHECK_EQ(V.Tick, A.Now());
	VT_CHECK_EQ(V.Year, static_cast<uint32>(A.Now() / History::TicksPerYear));
	VT_CHECK_EQ(V.Day, static_cast<uint32>((A.Now() % History::TicksPerYear) / 24u));
	VT_CHECK_EQ(V.Years, static_cast<uint32>((A.Now() - P->Born) / History::TicksPerYear));
	VT_CHECK(CleanName(V.Name));
	VT_CHECK_MSG(std::strcmp(V.Name, Named(A, Who).c_str()) == 0, "the name is the world's: %s", V.Name);
	{
		std::string R;
		History::NameRegion(W, A.Ages(), P->Region, R);
		VT_CHECK(CleanName(V.RegionName));
		VT_CHECK_MSG(std::strcmp(V.RegionName, R.c_str()) == 0, "the region's name is the world's: %s", V.RegionName);
	}

	// The body.
	{
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, T.Persons);
		VT_REQUIRE(Who < Index.Handles.size());
		const Population::PersonNeeds* N = W.Components().GetPool(T.Needs.Needs).TryGet(Index.Handles[Who]);
		VT_REQUIRE(N != nullptr);
		VT_CHECK_EQ(V.Food, N->Food);
		VT_CHECK_EQ(V.Health, N->Health);
		VT_CHECK_EQ(V.Rest, N->Rest);
		VT_CHECK_EQ(V.Hungry, N->Hungry);
	}

	// The start.
	{
		const Player::PlayerStart* S = Player::StartOf(W, T.Start);
		VT_REQUIRE(S != nullptr);
		VT_CHECK_EQ(V.StartRegion, S->Region);
		VT_CHECK_EQ(V.Holder, S->Holder);
		VT_CHECK_EQ(V.Bond, S->Bond);
		VT_CHECK_EQ(V.StartYear, static_cast<uint32>(S->Began / History::TicksPerYear));
		VT_CHECK(CleanName(V.HolderName));
		if (S->Holder != 0)
		{
			VT_CHECK(std::strcmp(V.HolderName, Named(A, S->Holder).c_str()) == 0);
		}
		else
		{
			VT_CHECK_EQ(V.HolderName[0], '\0');
		}
	}

	// The day.
	{
		const Player::PlayerHours* H = Player::HoursOf(W, T.Hour);
		VT_REQUIRE(H != nullptr);
		VT_CHECK_EQ(V.Awake, H->Awake);
		VT_CHECK_EQ(V.Spent, H->Spent);
		VT_CHECK_EQ(V.DaysLived, H->Days);
		VT_CHECK_EQ(V.Missed, H->Missed);
		VT_CHECK_EQ(V.Left, Player::HoursLeft(W, T.Hour));
	}

	// The queue.
	{
		const Player::PlayerOrders* Q = Player::OrdersOf(W, T.Order);
		VT_REQUIRE(Q != nullptr);
		VT_CHECK_EQ(V.Held, Q->Held);
		VT_CHECK_EQ(V.Taken, Q->Taken);
		VT_CHECK_EQ(V.Refused, Q->Refused);
		VT_CHECK_MSG(V.Taken >= 1 && V.Refused >= 1,
					 "the Work was taken and the Speak at nobody refused: taken %u, refused %u", V.Taken, V.Refused);
		VT_CHECK_EQ(V.Dropped, Q->Dropped);
		VT_CHECK_EQ(V.LastRefusal, Q->Last);
		VT_CHECK_MSG(Q->Held >= 1, "a Rest is waiting for tomorrow");
		for (uint32 i = 0; i < Q->Held && i < MostWaiting; ++i)
		{
			const Player::PlayerCommand& R = Q->Ring[(Q->First + i) % Player::MostOrders];
			VT_CHECK_EQ(V.Waiting[i].Kind, R.Kind);
			VT_CHECK_EQ(V.Waiting[i].Why, R.Why);
			VT_CHECK_EQ(V.Waiting[i].Target, R.Target);
			VT_CHECK_EQ(V.Waiting[i].Amount, R.Amount);
			VT_CHECK_EQ(V.Waiting[i].Hours, R.Hours);
			VT_CHECK_EQ(V.Waiting[i].Issued, R.Issued);
			VT_CHECK(std::memcmp(&V.Waiting[i], &R, sizeof(Player::PlayerCommand)) == 0);
		}
		VT_CHECK_EQ(V.Waiting[Q->Held < MostWaiting ? Q->Held : MostWaiting - 1].Kind,
					Q->Held < MostWaiting ? uint8{0} : V.Waiting[MostWaiting - 1].Kind);
	}

	// The regard.
	{
		// Written by the regard system the first time somebody is met, so a
		// life one day old may have none yet - and then the view says so.
		const Player::PlayerRegard* R = Player::RegardOf(W, T.Regard);
		VT_CHECK_EQ(V.Repute, Player::ReputeOf(W, T.Regard));
		VT_CHECK_EQ(V.KnownCount, R != nullptr ? R->Known : 0u);
		VT_CHECK_EQ(V.Kindnesses, R != nullptr ? R->Kindnesses : 0u);
		VT_CHECK_EQ(V.Wrongs, R != nullptr ? R->Wrongs : 0u);
		for (uint32 i = 0; R != nullptr && i < R->Known && i < MostKnownOf; ++i)
		{
			VT_CHECK_EQ(V.Known[i].Person, R->Who[i].Person);
			VT_CHECK_EQ(V.Known[i].Regard, R->Who[i].Regard);
			VT_CHECK_EQ(V.Known[i].Met, R->Who[i].Met);
			VT_CHECK(CleanName(V.Known[i].Name));
			VT_CHECK(std::strcmp(V.Known[i].Name, Named(A, R->Who[i].Person).c_str()) == 0);
		}
	}

	// The verbs.
	for (usize k = 0; k < IntentSlots; ++k)
	{
		VT_CHECK_EQ(V.Cost[k], Player::OrderRules{}.HoursOf[k]);
	}

	// Near: adjacent AND detailed, every one - and the neighbour detailed above.
	{
		const WorldGen::RegionGraph& G = Ways.Of(W.Map(), A.Ages().World.Regions);
		bool Listed = false;
		for (uint32 i = 0; i < V.NearCount; ++i)
		{
			VT_CHECK(G.AreAdjacent(static_cast<uint16>(V.Region), static_cast<uint16>(V.Near[i])));
			VT_CHECK(Population::IsDetailed(W, A.Ages(), T.Persons, V.Near[i]));
			Listed = Listed || V.Near[i] == Promoted;
		}
		VT_CHECK_MSG(Promoted == 0 || Listed, "the detailed neighbour %u is in Near", Promoted);
		VT_CHECK_MSG(Promoted == 0 || V.NearCount >= 1, "somewhere to walk to");
	}

	// Company: alive, here, not the played person, index order, lowest first.
	{
		uint32 Lowest = 0;
		uint32 There = 0;
		W.Components()
			.GetPool(T.Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& O)
				{
					if (O.Region == V.Region && O.Index != Who &&
						O.State == static_cast<uint8>(Population::LifeState::Alive))
					{
						++There;
						Lowest = Lowest == 0 || O.Index < Lowest ? O.Index : Lowest;
					}
				});
		VT_CHECK_EQ(V.CompanyThere, There);
		VT_CHECK_EQ(V.CompanyCount, There < MostCompany ? There : MostCompany);
		if (There > 0)
		{
			VT_CHECK_EQ(V.Company[0].Person, Lowest);
		}
		for (uint32 i = 0; i < V.CompanyCount; ++i)
		{
			const Population::PersonInfo* O = Population::FindPerson(W, T.Persons, V.Company[i].Person);
			VT_REQUIRE(O != nullptr);
			VT_CHECK_EQ(O->Region, V.Region);
			VT_CHECK(O->State == static_cast<uint8>(Population::LifeState::Alive));
			VT_CHECK(V.Company[i].Person != Who);
			VT_CHECK(i == 0 || V.Company[i - 1].Person < V.Company[i].Person);
			VT_CHECK_EQ(V.Company[i].Sex, O->Sex);
			VT_CHECK_EQ(V.Company[i].Years, static_cast<uint32>((A.Now() - O->Born) / History::TicksPerYear));
			VT_CHECK(CleanName(V.Company[i].Name));
			VT_CHECK(std::strcmp(V.Company[i].Name, Named(A, V.Company[i].Person).c_str()) == 0);
		}
	}

	// Sixty takes: the view is const on the world, and the graph is built once.
	const Hash64 State = A.StateDigest();
	const Hash64 Log = A.LogDigest();
	const Hash64 First = MeasureLifeView(V).Digest;
	LifeView Again;
	for (uint32 i = 0; i < 60; ++i)
	{
		TakeLifeView(W, A.Sources(), Ways, Again);
		VT_CHECK_EQ(MeasureLifeView(Again).Digest, First);
	}
	VT_CHECK_EQ(A.StateDigest(), State);
	VT_CHECK_EQ(A.LogDigest(), Log);
	VT_CHECK_EQ(Ways.Builds(), 1u);
	VT_CHECK(std::memcmp(&V, &Again, sizeof(LifeView)) == 0);
	const LifeStats S = MeasureLifeView(V);
	VAELEN_LOG_INFO(LogLife,
					"the life of %s (person %u) of %s: age %u, %u/%u hours left, food %u health %u rest %u, queue "
					"%u held %u taken %u refused, repute %d, %u known, %u near (a neighbour detailed after %u "
					"day(s)), %u of %u company; %u names, %u bytes, digest %016llx, taken in %.2f ms",
					V.Name, V.Person, V.RegionName, V.Years, V.Left, V.Awake, V.Food, V.Health, V.Rest, V.Held, V.Taken,
					V.Refused, V.Repute, V.KnownCount, V.NearCount, Waited, V.CompanyCount, V.CompanyThere, S.Named,
					S.Bytes, static_cast<unsigned long long>(S.Digest), Took);
}

VAELEN_TEST(Life, TheKnownAndTheHolderAreNamed)
{
	// The two name branches the fixture above never enters: somebody KNOWN
	// (the regard is written when somebody is met - a Speak aimed at company,
	// taken the next day) and somebody who HOLDS the played person (a bound
	// start, which the 96-map does not offer and AELVOR at 128/120 may).
	// Each branch asserts the name whenever the world offers the case, and
	// the log says whether it did.
	Aelvor A(Small());
	VT_REQUIRE(A.Begin());
	const uint32 Who = A.TakeUp(Anywhere());
	VT_REQUIRE(Who != 0);
	WorldGen::RegionGraphCache Ways;
	LifeView V;
	TakeLifeView(A.Instance(), A.Sources(), Ways, V);
	VT_REQUIRE(V.CompanyCount >= 1);
	Player::PlayerCommand C;
	C.Kind = static_cast<uint8>(Player::Intent::Speak);
	C.Target = V.Company[0].Person;
	C.Amount = 1;
	C.Issued = A.Now(); // unstamped, it would wait past StaleAfter and be refused as no longer meant
	VT_CHECK(A.Submit(C) == Player::Refusal::None);
	for (uint32 d = 0; d < 3; ++d)
	{
		A.Day();
	}
	TakeLifeView(A.Instance(), A.Sources(), Ways, V);
	const Player::PlayerRegard* R = Player::RegardOf(A.Instance(), A.Handles().Regard);
	VT_CHECK_MSG(R != nullptr && R->Known >= 1, "somebody spoken to is somebody known");
	VT_CHECK_EQ(V.KnownCount, R != nullptr ? R->Known : 0u);
	for (uint32 i = 0; i < V.KnownCount && i < MostKnownOf; ++i)
	{
		VT_CHECK(CleanName(V.Known[i].Name));
		VT_CHECK(V.Known[i].Name[0] != '\0');
		VT_CHECK_MSG(std::strcmp(V.Known[i].Name, Named(A, V.Known[i].Person).c_str()) == 0, "known: %s",
					 V.Known[i].Name);
	}

	Options O;
	O.Play = true;
	Aelvor B(O);
	VT_REQUIRE(B.Begin());
	const uint32 Bound = B.TakeUp(Player::StartRules{}); // WantBound = 1: somebody bound to be
	VT_CHECK_MSG(Bound != 0, "AELVOR at 128/120 offers somebody bound to be");
	uint32 Holder = 0;
	if (Bound != 0)
	{
		LifeView H;
		TakeLifeView(B.Instance(), B.Sources(), Ways, H);
		const Player::PlayerStart* S = Player::StartOf(B.Instance(), B.Handles().Start);
		VT_REQUIRE(S != nullptr);
		Holder = S->Holder;
		VT_CHECK_EQ(H.Holder, S->Holder);
		VT_CHECK_EQ(H.Bond, S->Bond);
		VT_CHECK(CleanName(H.HolderName));
		if (S->Holder != 0)
		{
			VT_CHECK(H.HolderName[0] != '\0');
			VT_CHECK_MSG(std::strcmp(H.HolderName, Named(B, S->Holder).c_str()) == 0, "held by %s", H.HolderName);
		}
		else
		{
			VT_CHECK_EQ(H.HolderName[0], '\0');
		}
	}
	VAELEN_LOG_INFO(LogLife, "known after a Speak: %u (%s); the bound start at 128: person %u held by %s", V.KnownCount,
					V.KnownCount != 0 ? V.Known[0].Name : "-", Bound,
					Holder != 0 ? Named(B, Holder).c_str() : "the region itself");
}

VAELEN_TEST(Life, TheViewOutlivesTheWorld)
{
	LifeView V;
	Hash64 Before = 0;
	{
		std::unique_ptr<Aelvor> A = std::make_unique<Aelvor>(Small());
		VT_REQUIRE(A->Begin());
		VT_REQUIRE(A->TakeUp(Anywhere()) != 0);
		WorldGen::RegionGraphCache Ways;
		TakeLifeView(A->Instance(), A->Sources(), Ways, V);
		Before = MeasureLifeView(V).Digest;
		VT_REQUIRE(V.Person != 0);
	} // the world is gone here

	VT_CHECK_EQ(MeasureLifeView(V).Digest, Before);
	VT_CHECK(CleanName(V.Name));
	VT_CHECK(V.Name[0] != '\0');
}
