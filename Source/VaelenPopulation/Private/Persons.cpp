// VAELEN - VaelenPopulation
// Phase 04.01: persons and the two grains of population.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population

#include "Vaelen/Population/Persons.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Population
{
	using History::RegionFaith;
	using History::RegionPopulation;

	namespace
	{
		EntityHandle RegionHandle(const World& W, const History::PreHistoryTypes& Types, uint32 Region)
		{
			EntityHandle Found;
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (R.Index == Region && Found.IsNull())
						{
							Found = H;
						}
					});
			return Found;
		}

		uint32 HighestPersonIndex(const World& W, const PersonTypes& Persons)
		{
			uint32 Highest = 0;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach([&](EntityHandle, const PersonInfo& P) { Highest = P.Index > Highest ? P.Index : Highest; });
			return Highest;
		}

		// Small deterministic draw stream (same construction as the naming stream).
		struct Draw
		{
			uint64 State;
			uint32 Next(uint32 Bound) noexcept
			{
				State = HashUInt64(State + 0x9e3779b97f4a7c15ull);
				return Bound == 0 ? 0u : static_cast<uint32>((State >> 17) % Bound);
			}
		};
	} // namespace

	PersonTypes PersonTypes::Declare(World& W)
	{
		PersonTypes T;
		T.Person = W.Types().Register<PersonInfo>("PersonInfo");
		T.Detail = W.Types().Register<RegionDetail>("RegionDetail");
		T.Lod = W.Types().Register<History::RegionLod>("RegionLod");
		W.Components().CreatePool(T.Person);
		W.Components().CreatePool(T.Detail);
		W.Components().CreatePool(T.Lod);
		return T;
	}

	PersonTypes PersonTypes::Declare(World& W, History::PreHistory& Ages)
	{
		const PersonTypes T = Declare(W);
		T.Attach(Ages);
		return T;
	}

	void PersonTypes::Attach(History::PreHistory& Ages) const noexcept
	{
		Ages.Peoples().ObserveLod(Lod);
		Ages.Migrations().ObserveLod(Lod);
		Ages.Disasters().ObserveLod(Lod);
	}

	bool IsDetailed(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons, uint32 Region)
	{
		const EntityHandle H = RegionHandle(W, Types, Region);
		return !H.IsNull() && W.Components().GetPool(Persons.Detail).TryGet(H) != nullptr;
	}

	uint32 PromoteRegion(World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
						 const MaterialiseRules& Rules, uint32 Region, SimTick Now)
	{
		const EntityHandle H = RegionHandle(W, Types, Region);
		if (H.IsNull() || W.Components().GetPool(Persons.Detail).TryGet(H) != nullptr)
		{
			return 0;
		}
		const RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(H);
		if (Counts == nullptr || Counts->Total == 0 || Counts->Total > Rules.MaxPersonsPerRegion)
		{
			return 0;
		}
		const RegionPopulation People = *Counts; // the pools below may move
		RegionFaith Faith;
		if (const RegionFaith* F = W.Components().GetPool(Types.Religion.Faith).TryGet(H))
		{
			Faith = *F;
		}
		// Language of each culture, for the persons' tongue.
		uint32 LanguageOf[RegionPopulation::MaxCultures] = {};
		for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
		{
			if (People.Culture[S] == 0)
			{
				continue;
			}
			W.Components()
				.GetPool(Types.Languages.Language)
				.ForEach(
					[&](EntityHandle, const History::LanguageInfo& L)
					{
						if (L.Culture == People.Culture[S] && LanguageOf[S] == 0)
						{
							LanguageOf[S] = L.Index;
						}
					});
		}
		// Faith is handed out in slot order across the persons of the region:
		// the first Adherents[0] persons believe in Religion[0], and so on.
		uint32 FaithSlot = 0;
		uint32 FaithLeft = Faith.Religion[0] != 0 ? Faith.Adherents[0] : 0u;
		auto NextFaith = [&]() -> uint32
		{
			while (FaithSlot < RegionFaith::MaxFaiths && (FaithLeft == 0 || Faith.Religion[FaithSlot] == 0))
			{
				++FaithSlot;
				FaithLeft = FaithSlot < RegionFaith::MaxFaiths && Faith.Religion[FaithSlot] != 0
								? Faith.Adherents[FaithSlot]
								: 0u;
			}
			if (FaithSlot >= RegionFaith::MaxFaiths)
			{
				return 0;
			}
			--FaithLeft;
			return Faith.Religion[FaithSlot];
		};

		uint32 Index = HighestPersonIndex(W, Persons);
		Draw D{HashCombine(HashCombine(HashUInt64(W.Config().Seed), HashUInt64(Region)), HashUInt64(Now))};
		const uint32 YoungLimit = Rules.MaxAgeYears / 3u;
		uint32 Created = 0;
		for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
		{
			if (People.Culture[S] == 0)
			{
				continue;
			}
			for (uint32 N = 0; N < People.Count[S]; ++N)
			{
				PersonInfo P;
				P.Index = ++Index;
				P.Region = Region;
				P.Culture = People.Culture[S];
				P.Religion = NextFaith();
				P.Language = LanguageOf[S];
				const bool Young = D.Next(1000) < Rules.YoungHalfPerMille;
				const uint32 AgeYears =
					Young ? D.Next(YoungLimit + 1u) : YoungLimit + D.Next(Rules.MaxAgeYears - YoungLimit + 1u);
				const uint64 AgeTicks = uint64{AgeYears} * History::TicksPerYear + D.Next(History::TicksPerYear);
				P.Born = Now >= AgeTicks ? Now - AgeTicks : 0u;
				P.Sex = static_cast<uint8>(D.Next(1000) < Rules.FemalePerMille ? Sex::Female : Sex::Male);
				P.State = static_cast<uint8>(LifeState::Alive);
				P.Identity = Noise::LatticeHash(W.Config().Seed ^ 0x504552534full, static_cast<int32>(P.Index),
												static_cast<int32>(Region));
				const EntityHandle E = W.CreateEntity(IdKind::Person);
				W.Components().GetPool(Persons.Person).Add(E, P);
				++Created;
			}
		}
		RegionDetail Detail;
		Detail.Region = Region;
		Detail.Persons = Created;
		Detail.PromotedAt = Now;
		Detail.Promotions = 1;
		W.Components().GetPool(Persons.Detail).Add(H, Detail);
		History::RegionLod Marker;
		Marker.Level = History::RegionLod::DetailedLevel;
		W.Components().GetPool(Persons.Lod).Add(H, Marker);
		return Created;
	}

	RegionCensus CountPersons(const World& W, const PersonTypes& Persons, uint32 Region)
	{
		RegionCensus C;
		C.Region = Region;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					if (P.Region != Region)
					{
						return;
					}
					if (P.State != static_cast<uint8>(LifeState::Alive))
					{
						++C.Dead;
						return;
					}
					++C.Alive;
					C.Female += P.Sex == static_cast<uint8>(Sex::Female) ? 1u : 0u;
					C.Male += P.Sex == static_cast<uint8>(Sex::Male) ? 1u : 0u;
					for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
					{
						if (C.CultureOf[S] == P.Culture || C.CultureOf[S] == 0)
						{
							C.CultureOf[S] = P.Culture;
							++C.ByCulture[S];
							break;
						}
					}
					if (P.Religion == 0)
					{
						++C.Faithless;
						return;
					}
					for (uint32 S = 0; S < RegionFaith::MaxFaiths; ++S)
					{
						if (C.FaithOf[S] == P.Religion || C.FaithOf[S] == 0)
						{
							C.FaithOf[S] = P.Religion;
							++C.ByFaith[S];
							break;
						}
					}
				});
		return C;
	}

	bool IsConsistent(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons, uint32 Region)
	{
		const EntityHandle H = RegionHandle(W, Types, Region);
		if (H.IsNull() || W.Components().GetPool(Persons.Detail).TryGet(H) == nullptr)
		{
			return false;
		}
		const RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(H);
		if (Counts == nullptr)
		{
			return false;
		}
		const RegionCensus C = CountPersons(W, Persons, Region);
		if (C.Alive != Counts->Total)
		{
			return false;
		}
		for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
		{
			if (Counts->Culture[S] == 0)
			{
				continue;
			}
			uint32 Seen = 0;
			for (uint32 K = 0; K < RegionPopulation::MaxCultures; ++K)
			{
				Seen += C.CultureOf[K] == Counts->Culture[S] ? C.ByCulture[K] : 0u;
			}
			if (Seen != Counts->Count[S])
			{
				return false;
			}
		}
		const RegionFaith* F = W.Components().GetPool(Types.Religion.Faith).TryGet(H);
		for (uint32 S = 0; S < RegionFaith::MaxFaiths; ++S)
		{
			const uint32 Religion = F != nullptr ? F->Religion[S] : 0u;
			if (Religion == 0)
			{
				continue;
			}
			uint32 Seen = 0;
			for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
			{
				Seen += C.FaithOf[K] == Religion ? C.ByFaith[K] : 0u;
			}
			if (Seen != F->Adherents[S])
			{
				return false;
			}
		}
		const uint32 Believers = F != nullptr ? F->Total() : 0u;
		return C.Faithless == Counts->Total - Believers;
	}

	uint32 DemoteRegion(World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons, uint32 Region)
	{
		const EntityHandle H = RegionHandle(W, Types, Region);
		if (H.IsNull() || W.Components().GetPool(Persons.Detail).TryGet(H) == nullptr)
		{
			return 0;
		}
		// Fold the living back into the counts.
		const RegionCensus C = CountPersons(W, Persons, Region);
		if (RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(H))
		{
			for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
			{
				if (Counts->Culture[S] != 0)
				{
					Counts->Remove(Counts->Culture[S], Counts->Count[S]);
				}
			}
			for (uint32 K = 0; K < RegionPopulation::MaxCultures; ++K)
			{
				if (C.CultureOf[K] != 0 && C.ByCulture[K] > 0)
				{
					Counts->Add(C.CultureOf[K], C.ByCulture[K]);
				}
			}
			Counts->Recount();
		}
		if (RegionFaith* F = W.Components().GetPool(Types.Religion.Faith).TryGet(H))
		{
			for (uint32 S = 0; S < RegionFaith::MaxFaiths; ++S)
			{
				if (F->Religion[S] != 0)
				{
					F->Remove(F->Religion[S], F->Adherents[S]);
				}
			}
			for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
			{
				if (C.FaithOf[K] != 0 && C.ByFaith[K] > 0)
				{
					F->Add(C.FaithOf[K], C.ByFaith[K]);
				}
			}
			F->Recount();
		}
		// Destroy every person of the region, the dead included.
		std::vector<EntityHandle> Doomed;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle E, const PersonInfo& P)
				{
					if (P.Region == Region)
					{
						Doomed.push_back(E);
					}
				});
		for (const EntityHandle E : Doomed)
		{
			W.DestroyEntity(E);
		}
		W.Components().GetPool(Persons.Detail).Remove(H);
		W.Components().GetPool(Persons.Lod).Remove(H);
		return static_cast<uint32>(Doomed.size());
	}

	DetailStats MeasureDetail(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons)
	{
		DetailStats S;
		std::vector<PersonInfo> All;
		W.Components().GetPool(Persons.Person).ForEach([&](EntityHandle, const PersonInfo& P) { All.push_back(P); });
		std::sort(All.begin(), All.end(), [](const PersonInfo& A, const PersonInfo& B) { return A.Index < B.Index; });
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const PersonInfo& P : All)
		{
			++S.Persons;
			S.Alive += P.State == static_cast<uint8>(LifeState::Alive) ? 1u : 0u;
			S.Dead += P.State == static_cast<uint8>(LifeState::Dead) ? 1u : 0u;
			S.Gone += P.State == static_cast<uint8>(LifeState::Gone) ? 1u : 0u;
			Digest = HashBytes(reinterpret_cast<const char*>(&P), sizeof(P), Digest);
		}
		S.PersonsDigest = Digest;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach(
				[&](EntityHandle, const RegionDetail& D)
				{
					++S.DetailedRegions;
					S.Inconsistent += IsConsistent(W, Types, Persons, D.Region) ? 0u : 1u;
				});
		return S;
	}
} // namespace Vaelen::Population
