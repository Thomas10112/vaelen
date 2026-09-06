// VAELEN - Tests/Society
// Phase 05.03: norms - customs from identity and parents, marriages by culture,
// drifts on schisms and disasters.
//
// STATUS: VALIDATED (Phase 05)

#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/Snapshot.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/Norms.h"

#include "Vaelen/Core/Log.h"
#include "VaelenTest.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::History;
using namespace Vaelen::Population;
using namespace Vaelen::Society;
using namespace Vaelen::WorldGen;

// Recorded on clang 18 / Linux x86_64 on 2026-09-06 (05.03): AELVOR 128 at
// year 300 and 100 more years with norms, the busiest region detailed with
// lives, families observing the norms and the bridge.
#define VAELEN_NORMS_FROZEN_128 0xf2b7de051eab022bull
#define VAELEN_NORMS_CULTURES_128 4u
#define VAELEN_NORMS_DRIFTS_128 16u

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogNorms);

	constexpr uint64 AelvorSeed = 0x41454c564f52ull;

	struct Run
	{
		explicit Run(uint64 Seed, bool Observe = true, PreHistoryRules Ages_ = PreHistoryRules{})
			: Instance(Config(Seed)), Ages(Instance, Ages_)
		{
			Persons = PersonTypes::Declare(Instance, Ages);
			Families = FamilyTypes::Declare(Instance);
			Lod = LodTypes::Declare(Instance);
			Norms = NormTypes::Declare(Instance);
			LifeRules Life;
			Life.SpouseRequired = 1;
			Lives = std::make_unique<LifeSystem>(Instance, Ages.Types(), Persons, Life);
			Houses = std::make_unique<FamilySystem>(Instance, Ages.Types(), Persons, Families, FamilyRules{});
			Bridge = std::make_unique<LodSystem>(Instance, Ages.Types(), Persons, Lod, LodRules{});
			Customs = std::make_unique<NormSystem>(Instance, Ages.Types(), Norms, NormRules{});
			Houses->RunAfter("Lod");
			Houses->RunAfter("Norms");
			if (Observe)
			{
				Houses->ObserveNorms(Norms.Marriage);
			}
			Instance.Systems().Add(Lives.get());
			Instance.Systems().Add(Houses.get());
			Instance.Systems().Add(Bridge.get());
			Instance.Systems().Add(Customs.get());
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
						if (P != nullptr && P->Total > People)
						{
							People = P->Total;
							Best = R.Index;
						}
					});
			return Best;
		}
		uint32 MajorityOf(uint32 Region) const
		{
			uint32 Found = 0;
			Instance.Components()
				.GetPool(Ages.Types().World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const RegionInfo& R)
					{
						const RegionPopulation* P =
							Instance.Components().GetPool(Ages.Types().Population.Population).TryGet(H);
						if (R.Index == Region && P != nullptr)
						{
							Found = P->Majority;
						}
					});
			return Found;
		}
		NormStats Stats() const { return MeasureNorms(Instance, Ages.Types(), Norms); }
		World Instance;
		PreHistory Ages;
		PersonTypes Persons;
		FamilyTypes Families;
		LodTypes Lod;
		NormTypes Norms;
		std::unique_ptr<LifeSystem> Lives;
		std::unique_ptr<FamilySystem> Houses;
		std::unique_ptr<LodSystem> Bridge;
		std::unique_ptr<NormSystem> Customs;
	};

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

	bool InRules(const NormSet& N, const NormRules& R)
	{
		return N.Marriage.MarryFrom >= R.MarryFromMin && N.Marriage.MarryFrom <= R.MarryFromMax &&
			   N.Marriage.MarryTo >= R.MarryToMin && N.Marriage.MarryTo <= R.MarryToMax &&
			   N.Marriage.MaxAgeGap >= R.GapMin && N.Marriage.MaxAgeGap <= R.GapMax && N.Marriage.FaithMatters <= 1 &&
			   N.Marriage.MarriagesPerMille >= R.MarriagesMin && N.Marriage.MarriagesPerMille <= R.MarriagesMax &&
			   N.Descent_ <= 1 && N.FaithTolerancePerMille >= R.ToleranceMin &&
			   N.FaithTolerancePerMille <= R.ToleranceMax && N.MobilityPerMille >= R.MobilityMin &&
			   N.MobilityPerMille <= R.MobilityMax && N.BondageAllowed < 8;
	}
} // namespace

VAELEN_TEST(Norms, CustomsComeFromTheIdentityAndTheParent)
{
	const NormRules Rules;
	const NormSet A = NormsFromIdentity(0x1234, Rules);
	const NormSet B = NormsFromIdentity(0x1234, Rules);
	VT_CHECK(HashBytes(reinterpret_cast<const char*>(&A), sizeof(A)) ==
			 HashBytes(reinterpret_cast<const char*>(&B), sizeof(B)));
	VT_CHECK(InRules(A, Rules));
	// Over many identities every custom varies and every value stays in its range.
	std::set<uint32> Seen[static_cast<uint32>(NormField::Count)];
	uint32 Matrilineal = 0;
	uint32 FaithMatters = 0;
	uint32 Out = 0;
	for (uint32 i = 1; i <= 256; ++i)
	{
		const NormSet N = NormsFromIdentity(HashUInt64(i), Rules);
		Out += InRules(N, Rules) ? 0u : 1u;
		for (uint32 F = 0; F < static_cast<uint32>(NormField::Count); ++F)
		{
			Seen[F].insert(NormValue(N, static_cast<NormField>(F)));
		}
		Matrilineal += N.Descent_ == static_cast<uint32>(Descent::Matrilineal) ? 1u : 0u;
		FaithMatters += N.Marriage.FaithMatters;
	}
	VT_CHECK_EQ(Out, 0u);
	for (uint32 F = 0; F < static_cast<uint32>(NormField::Count); ++F)
	{
		VT_CHECK_MSG(Seen[F].size() >= 2, "custom %s never varies", NormFieldName(static_cast<NormField>(F)));
	}
	VT_CHECK(Matrilineal > 20 && Matrilineal < 90);		// about a fifth
	VT_CHECK(FaithMatters > 150 && FaithMatters < 220); // about seven in ten
	// A child takes its parent's customs but one.
	uint32 Differences = 0;
	for (uint32 i = 1; i <= 64; ++i)
	{
		const NormSet Child = NormsOfChild(A, HashUInt64(1000 + i), Rules);
		uint32 Differ = 0;
		for (uint32 F = 0; F < static_cast<uint32>(NormField::Count); ++F)
		{
			Differ += NormValue(Child, static_cast<NormField>(F)) != NormValue(A, static_cast<NormField>(F)) ? 1u : 0u;
		}
		VT_CHECK(Differ <= 1);
		Differences += Differ;
		VT_CHECK(InRules(Child, Rules));
		VT_CHECK_EQ(Child.Drifts, 0u);
	}
	VT_CHECK(Differences > 20);
	VT_CHECK(NormFieldName(NormField::Count)[0] == '?');
	VT_CHECK_EQ(NormValue(A, NormField::Count), 0u);
}

VAELEN_TEST(Norms, EveryCultureGetsCustomsAndTheFamiliesFollowThem)
{
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 600)); // long enough for cultures to split
	NormStats S = W.Stats();
	VT_CHECK(S.Cultures >= 4);
	VT_CHECK_EQ(S.WithNorms, S.Cultures);
	VT_CHECK_EQ(S.MirrorMismatch, 0u);
	VT_CHECK(S.Matrilineal < S.Cultures);
	// A split culture shares all but one custom with its parent.
	uint32 Children = 0;
	W.Instance.Components()
		.GetPool(W.Ages.Types().Population.Culture)
		.ForEach(
			[&](EntityHandle, const CultureInfo& C)
			{
				if (C.Parent == 0)
				{
					return;
				}
				const NormSet* Mine = NormsOf(W.Instance, W.Ages.Types(), W.Norms, C.Index);
				const NormSet* Theirs = NormsOf(W.Instance, W.Ages.Types(), W.Norms, C.Parent);
				if (Mine == nullptr || Theirs == nullptr)
				{
					return;
				}
				++Children;
				uint32 Differ = 0;
				for (uint32 F = 0; F < static_cast<uint32>(NormField::Count); ++F)
				{
					Differ +=
						NormValue(*Mine, static_cast<NormField>(F)) != NormValue(*Theirs, static_cast<NormField>(F))
							? 1u
							: 0u;
				}
				VT_CHECK(Differ <= 1 + Mine->Drifts + Theirs->Drifts);
			});
	VT_CHECK(Children > 0);
	// The families follow the customs: the majority culture of the busiest region
	// marries from 30 and cares nothing for faith; the others keep their own.
	const uint32 Region = W.Busiest();
	const uint32 Culture = W.MajorityOf(Region);
	VT_REQUIRE(Culture != 0);
	const NormSet* Before = NormsOf(W.Instance, W.Ages.Types(), W.Norms, Culture);
	VT_REQUIRE(Before != nullptr);
	NormSet Late = *Before;
	Late.Marriage.MarryFrom = 30;
	Late.Marriage.FaithMatters = 0;
	Late.Marriage.MarriagesPerMille = 500;
	VT_CHECK(SetNorms(W.Instance, W.Ages.Types(), W.Norms, Culture, Late));
	VT_CHECK(!SetNorms(W.Instance, W.Ages.Types(), W.Norms, 0xfffffff0u, Late));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, Region));
	const usize Mark = W.Instance.Log().Count();
	W.Ages.Run(20);
	uint32 Marriages = 0;
	uint32 Young = 0;
	uint32 Mixed = 0;
	const std::vector<Event>& All = W.Instance.Log().All();
	for (usize i = Mark; i < All.size(); ++i)
	{
		if (!All[i].Is(PersonMarriedEvent))
		{
			continue;
		}
		const MarriagePayload P = All[i].Get<MarriagePayload>();
		const PersonInfo* Groom = FindPerson(W.Instance, W.Persons, P.Person);
		const PersonInfo* Bride = FindPerson(W.Instance, W.Persons, P.Spouse);
		VT_REQUIRE(Groom != nullptr && Bride != nullptr);
		if (Groom->Culture != Culture)
		{
			continue;
		}
		++Marriages;
		Young += AgeYears(*Groom, All[i].Tick) < 30 || AgeYears(*Bride, All[i].Tick) < 30 ? 1u : 0u;
		Mixed += Groom->Religion != Bride->Religion ? 1u : 0u;
	}
	VAELEN_LOG_INFO(LogNorms,
					"culture %u marrying from 30 without regard to faith: %u marriages, %u under 30, %u mixed", Culture,
					Marriages, Young, Mixed);
	VT_CHECK(Marriages > 50);
	VT_CHECK_EQ(Young, 0u);
	// The same world under the family rules alone: marriages under 30, none mixed.
	Run X(AelvorSeed, false);
	VT_REQUIRE(X.Ages.Generate(Run::Square(128), 600));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	const usize MarkX = X.Instance.Log().Count();
	X.Ages.Run(20);
	uint32 YoungX = 0;
	uint32 MixedX = 0;
	const std::vector<Event>& AllX = X.Instance.Log().All();
	for (usize i = MarkX; i < AllX.size(); ++i)
	{
		if (!AllX[i].Is(PersonMarriedEvent))
		{
			continue;
		}
		const MarriagePayload P = AllX[i].Get<MarriagePayload>();
		const PersonInfo* Groom = FindPerson(X.Instance, X.Persons, P.Person);
		const PersonInfo* Bride = FindPerson(X.Instance, X.Persons, P.Spouse);
		VT_REQUIRE(Groom != nullptr && Bride != nullptr);
		YoungX += AgeYears(*Groom, AllX[i].Tick) < 30 ? 1u : 0u;
		MixedX += Groom->Religion != Bride->Religion ? 1u : 0u;
	}
	VT_CHECK(YoungX > 50);
	VT_CHECK_EQ(MixedX, 0u);
}

VAELEN_TEST(Norms, CustomsDriftWithSchismsAndDisasters)
{
	PreHistoryRules Quiet;
	for (uint32 K = 0; K < static_cast<uint32>(DisasterKind::Count); ++K)
	{
		Quiet.Disasters.OmenPerMille[K] = 0;
	}
	Quiet.Disasters.StrikePerMille = 1000;
	Run W(AelvorSeed, true, Quiet);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	const uint32 Region = W.Busiest();
	const uint32 Culture = W.MajorityOf(Region);
	VT_REQUIRE(Culture != 0);
	const NormSet Start = *NormsOf(W.Instance, W.Ages.Types(), W.Norms, Culture);
	const uint32 DriftsBefore = W.Stats().Drifts;
	// Great disasters loosen the people: mobility rises once per strike of severity two or more.
	uint32 Great = 0;
	for (uint32 Year = 1; Year <= 12; ++Year)
	{
		VT_REQUIRE(Curse(W, Region, DisasterKind::Drought));
		W.Ages.Run(1);
	}
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(DisasterStruckEvent) && E.Get<DisasterPayload>().Region == Region)
		{
			Great += E.Get<DisasterPayload>().Severity >= 2 ? 1u : 0u;
		}
	}
	const NormSet* After = NormsOf(W.Instance, W.Ages.Types(), W.Norms, Culture);
	VT_REQUIRE(After != nullptr);
	VT_CHECK(Great > 0);
	VT_CHECK_EQ(After->MobilityPerMille,
				std::min(1000u, Start.MobilityPerMille + Great * NormRules{}.DisasterMobilityGain));
	VT_CHECK_EQ(After->Drifts, Great);
	uint32 Changes = 0;
	for (const Event& E : W.Instance.Log().All())
	{
		if (E.Is(NormChangedEvent))
		{
			const NormPayload P = E.Get<NormPayload>();
			VT_CHECK_EQ(P.Culture, Culture);
			VT_CHECK_EQ(P.Field, static_cast<uint32>(NormField::Mobility));
			VT_CHECK(P.After > P.Before);
			++Changes;
		}
	}
	VT_CHECK_EQ(Changes, Great);
	VT_CHECK_EQ(W.Stats().Drifts, DriftsBefore + Great);
	VT_CHECK_EQ(W.Stats().MirrorMismatch, 0u);
	// A schism in the culture hardens the faith and lowers the tolerance, once per schism.
	const uint32 ToleranceBefore = After->FaithTolerancePerMille;
	const uint32 FaithBefore = After->Marriage.FaithMatters;
	W.Instance.Events().Publish(W.Instance.Now(), SchismEvent, ReligionPayload{1, Region, Culture, 1});
	W.Ages.Run(1);
	const NormSet* Hardened = NormsOf(W.Instance, W.Ages.Types(), W.Norms, Culture);
	VT_REQUIRE(Hardened != nullptr);
	VT_CHECK_EQ(Hardened->Marriage.FaithMatters, 1u);
	VT_CHECK_EQ(Hardened->FaithTolerancePerMille, ToleranceBefore > NormRules{}.SchismToleranceLoss
													  ? ToleranceBefore - NormRules{}.SchismToleranceLoss
													  : 0u);
	VT_CHECK_EQ(Hardened->Drifts, Great + (FaithBefore == 0 ? 2u : 1u));
	VT_CHECK_EQ(W.Stats().MirrorMismatch, 0u);
	// Nothing drifts in a quiet world.
	Run Q(AelvorSeed, true, Quiet);
	VT_REQUIRE(Q.Ages.Generate(Run::Square(64), 120));
	const uint32 Drifts = Q.Stats().Drifts;
	Q.Ages.Run(20);
	VT_CHECK_EQ(Q.Stats().Drifts, Drifts);
}

VAELEN_TEST(Norms, DeterministicSnapshotSafeAndFrozen)
{
	Run A(AelvorSeed);
	Run B(AelvorSeed);
	VT_REQUIRE(A.Ages.Generate(Run::Square(64), 120));
	VT_REQUIRE(B.Ages.Generate(Run::Square(64), 120));
	const uint32 Region = A.Busiest();
	VT_CHECK(RequestDetail(A.Instance, A.Lod, Region));
	VT_CHECK(RequestDetail(B.Instance, B.Lod, Region));
	A.Ages.Run(20);
	B.Ages.Run(20);
	VT_CHECK_EQ(ComputeStateDigest(A.Instance), ComputeStateDigest(B.Instance));
	VT_CHECK_EQ(A.Stats().Digest, B.Stats().Digest);
	A.Instance.TickMany(100);
	std::vector<uint8> Image;
	SaveSnapshot(A.Instance, Image);
	Run R(AelvorSeed);
	VT_REQUIRE(LoadSnapshot(R.Instance, Image.data(), Image.size()) == SnapshotResult::Ok);
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	A.Ages.Run(20);
	R.Ages.Run(20);
	VT_CHECK_EQ(ComputeStateDigest(R.Instance), ComputeStateDigest(A.Instance));
	VT_CHECK_EQ(R.Stats().Digest, A.Stats().Digest);
	// The family rules alone give another world: the norms change who marries.
	Run X(AelvorSeed, false);
	VT_REQUIRE(X.Ages.Generate(Run::Square(64), 120));
	VT_CHECK(RequestDetail(X.Instance, X.Lod, Region));
	X.Ages.Run(20);
	VT_CHECK(ComputeStateDigest(X.Instance) != ComputeStateDigest(B.Instance));
	// Frozen: AELVOR 128 at year 300 and 100 more with the busiest region detailed.
	Run W(AelvorSeed);
	VT_REQUIRE(W.Ages.Generate(Run::Square(128), 300));
	VT_CHECK(RequestDetail(W.Instance, W.Lod, W.Busiest()));
	W.Ages.Run(100);
	const NormStats S = W.Stats();
	VAELEN_LOG_INFO(LogNorms, "frozen: norms128=%016llx cultures=%u drifts=%u (%u matrilineal, %u faith matters)",
					static_cast<unsigned long long>(S.Digest), S.WithNorms, S.Drifts, S.Matrilineal, S.FaithMatters);
	VT_CHECK_EQ(S.Digest, Hash64{VAELEN_NORMS_FROZEN_128});
	VT_CHECK_EQ(S.WithNorms, uint32{VAELEN_NORMS_CULTURES_128});
	VT_CHECK_EQ(S.Drifts, uint32{VAELEN_NORMS_DRIFTS_128});
	VT_CHECK_EQ(S.MirrorMismatch, 0u);
	VT_CHECK_EQ(S.WithNorms, S.Cultures);
}
