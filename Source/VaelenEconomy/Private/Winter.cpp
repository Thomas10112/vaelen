// VAELEN - VaelenEconomy
// Phase 18.06: the winter, yearly, as a consequence. See Winter.h.
//
// STATUS: PROTOTYPE (Phase 18) - unit/deterministic tests in Tests/Economy

#include "Vaelen/Economy/Winter.h"

#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Economy
{
	namespace
	{
		constexpr uint32 G_GRAIN = static_cast<uint32>(Good::Grain);
		constexpr uint32 G_CLOTH = static_cast<uint32>(Good::Cloth);
		constexpr uint32 G_TIMBER = static_cast<uint32>(Good::Timber);

		/// Region index -> region entity, index 0 unused.
		void RegionHandlesOf(const World& W, const History::PreHistoryTypes& Types, std::vector<EntityHandle>& Out)
		{
			Out.clear();
			W.Components()
				.GetPool(Types.World.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (R.Index >= Out.size())
						{
							Out.resize(R.Index + 1u);
						}
						Out[R.Index] = H;
					});
		}

		uint32 FloorToUnsigned(Fix64 V) noexcept
		{
			const int32 I = V.FloorToInt();
			return I > 0 ? static_cast<uint32>(I) : 0u;
		}

		int32 Take(uint64 Units) noexcept
		{
			return -static_cast<int32>(std::min<uint64>(Units, 0x7fffffffull));
		}
	} // namespace

	void WinterSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		const uint64 Year = Context.Tick / History::TicksPerYear;
		if (Year == 0)
		{
			return; // no winter has lain yet
		}
		std::vector<EntityHandle> Regions;
		RegionHandlesOf(W, Types, Regions);
		const uint32 N = static_cast<uint32>(Regions.size());
		if (N < 2)
		{
			return;
		}
		// The winter just lain, the one coming, and the region's usual year -
		// the same year without its variation, which is the climate the news
		// is measured against. Three passes of the ONE function everybody reads.
		std::vector<WorldGen::YearShape> Lain;
		std::vector<WorldGen::YearShape> Coming;
		std::vector<WorldGen::YearShape> Usual;
		WorldGen::ShapeRegionYears(W, Types.World, Year - 1u, Rules.Climate, Lain);
		WorldGen::ShapeRegionYears(W, Types.World, Year, Rules.Climate, Coming);
		WorldGen::ClimateRules Still = Rules.Climate;
		Still.YearAmplitude = Fix64::Zero();
		WorldGen::ShapeRegionYears(W, Types.World, Year - 1u, Still, Usual);
		if (Lain.size() < N || Coming.size() < N || Usual.size() < N)
		{
			return;
		}

		// Who is where: the detailed regions, their living persons, the houses
		// of every region, and the settlements standing. One pass over each pool.
		std::vector<uint8> Detailed(N, 0u);
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach(
				[&](EntityHandle, const Population::RegionDetail& D)
				{
					if (D.Region < N)
					{
						Detailed[D.Region] = 1u;
					}
				});
		std::vector<uint32> Alive(N, 0u);
		std::vector<std::pair<uint32, EntityHandle>> People; // (region, person), region-sorted below
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (P.Region < N && Detailed[P.Region] != 0u &&
						P.State == static_cast<uint8>(Population::LifeState::Alive))
					{
						++Alive[P.Region];
						People.emplace_back(P.Region, H);
					}
				});
		std::stable_sort(People.begin(), People.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		std::vector<std::pair<uint32, EntityHandle>> Houses; // (region, family entity carrying a HouseStock)
		W.Components()
			.GetPool(Economy.House)
			.ForEach(
				[&](EntityHandle H, const HouseStock&)
				{
					const Population::FamilyInfo* F = W.Components().GetPool(Families.Family).TryGet(H);
					if (F != nullptr && F->Region < N)
					{
						Houses.emplace_back(F->Region, H);
					}
				});
		std::stable_sort(Houses.begin(), Houses.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		std::vector<uint8> Sheltered(N, 0u);
		if (HasSettlements)
		{
			W.Components()
				.GetPool(Settlements)
				.ForEach(
					[&](EntityHandle, const SettlementInfo& S)
					{
						if (S.Region < N && S.Abandoned == 0)
						{
							Sheltered[S.Region] = 1u;
						}
					});
		}

		usize NextPerson = 0;
		usize NextHouse = 0;
		for (uint32 Region = 1; Region < N; ++Region)
		{
			// Advance the two sorted lists to this region.
			const usize FirstPerson = [&]
			{
				while (NextPerson < People.size() && People[NextPerson].first < Region)
				{
					++NextPerson;
				}
				return NextPerson;
			}();
			const usize FirstHouse = [&]
			{
				while (NextHouse < Houses.size() && Houses[NextHouse].first < Region)
				{
					++NextHouse;
				}
				return NextHouse;
			}();
			const EntityHandle RH = Regions[Region];
			if (RH.IsNull())
			{
				continue;
			}
			const WorldGen::YearShape& Y = Lain[Region];
			const uint32 Severity = WorldGen::WinterSeverity(Y, Rules.Climate);
			const uint32 UsualSeverity = WorldGen::WinterSeverity(Usual[Region], Rules.Climate);
			const uint32 ColdSum = FloorToUnsigned(Y.ColdSum);
			History::RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			const bool Fine = Detailed[Region] != 0u;
			const uint32 PeopleHere = Fine ? Alive[Region] : (Counts != nullptr ? Counts->Total : 0u);

			// 1. The coarse dead, before the event so the payload carries them.
			// A detailed region's deaths are the need system's: it judges the
			// chill below and names this winter (Needs.cpp).
			uint32 Deaths = 0;
			if (Severity >= 1u && !Fine && Counts != nullptr && Counts->Total > 0u)
			{
				const uint32 Wanted =
					static_cast<uint32>(uint64{Counts->Total} * Rules.ColdDeathsPerMille[Severity] / 1000u);
				Deaths = History::KillShare(*Counts, Wanted);
				if (Deaths > 0u)
				{
					History::RegionFaith* F = W.Components().GetPool(Types.Religion.Faith).TryGet(RH);
					if (F != nullptr && F->Total() > 0u)
					{
						History::TrimBelieversToTheLiving(*F, Counts->Total);
					}
				}
			}

			// 2. The winter lain, on the record for a cause to point at.
			PersistentId Winter;
			if (Severity >= 1u)
			{
				Winter = Context.Events->Publish(
					Context.Tick, WorldGen::WinterEvent,
					WorldGen::WinterPayload{Region, Severity, ColdSum, Deaths, PeopleHere, UsualSeverity},
					W.Entities().GetId(RH));
			}

			// 3. The fuel and the grain, through the ledger.
			RegionStock* Common = W.Components().GetPool(Economy.Region).TryGet(RH);
			uint32 CoveredPerMille = 1000u;
			if (Common != nullptr && PeopleHere > 0u && ColdSum > 0u)
			{
				const uint64 Wanted =
					Rules.DegreeDaysPerTimber > 0u && Rules.TimberPerPersons > 0u
						? uint64{ColdSum / Rules.DegreeDaysPerTimber} * (PeopleHere / Rules.TimberPerPersons)
						: 0u;
				if (Wanted > 0u)
				{
					const uint32 Taken = MoveStock(W, &Common->Amount[G_TIMBER], RH, Region, 0u, Good::Timber,
												   Take(Wanted), Context.Tick, Winter);
					CoveredPerMille = static_cast<uint32>(uint64{Taken} * 1000u / Wanted);
				}
			}
			if (Severity >= 1u && Common != nullptr)
			{
				const uint32 PerMille = Rules.WinterGrainPerMille[Severity - 1u];
				MoveStock(W, &Common->Amount[G_GRAIN], RH, Region, 0u, Good::Grain,
						  Take(uint64{Common->Amount[G_GRAIN]} * PerMille / 1000u), Context.Tick, Winter);
				for (usize i = FirstHouse; i < Houses.size() && Houses[i].first == Region; ++i)
				{
					HouseStock* Stock = W.Components().GetPool(Economy.House).TryGet(Houses[i].second);
					const Population::FamilyInfo* F = W.Components().GetPool(Families.Family).TryGet(Houses[i].second);
					if (Stock != nullptr && F != nullptr)
					{
						MoveStock(W, &Stock->Amount[G_GRAIN], Houses[i].second, Region, F->Index, Good::Grain,
								  Take(uint64{Stock->Amount[G_GRAIN]} * PerMille / 1000u), Context.Tick, Winter);
					}
				}
			}

			// 4. The chill on the people simulated one by one, by what the
			// fuel, the roof and the cloth left exposed.
			if (Fine && Region != Rules.DailyRegion && ColdSum > 0u && PeopleHere > 0u)
			{
				const uint32 Shelter = Sheltered[Region] != 0u ? Rules.ShelterPerMille : 0u;
				const uint32 ClothWanted = Rules.ClothPerPersons > 0u ? PeopleHere / Rules.ClothPerPersons : 0u;
				const uint32 Cover =
					Common != nullptr && Common->Amount[G_CLOTH] >= ClothWanted ? Rules.ClothWarmthPerMille : 0u;
				const uint64 Exposure = uint64{1000u - std::min(1000u, CoveredPerMille)} *
										(1000u - std::min(1000u, Shelter + Cover)) / 1000u;
				const uint64 Chill =
					Rules.DegreeDaysPerChill > 0u ? Exposure * ColdSum / 1000u / Rules.DegreeDaysPerChill : 0u;
				if (Chill > 0u)
				{
					const uint8 Amount = static_cast<uint8>(std::min<uint64>(Chill, 255u));
					for (usize i = FirstPerson; i < People.size() && People[i].first == Region; ++i)
					{
						Population::PersonWarmth* C = W.Components().GetPool(Warmth.Warmth).TryGet(People[i].second);
						if (C != nullptr)
						{
							C->Chill = static_cast<uint8>(std::min<uint32>(255u, uint32{C->Chill} + Amount));
						}
					}
				}
			}

			// 5. The winter coming: news only when it is, by the chronicle's rule.
			const uint32 Next = WorldGen::WinterSeverity(Coming[Region], Rules.Climate);
			if (Next >= 2u && Next > UsualSeverity && PeopleHere > 0u)
			{
				Context.Events->Publish(Context.Tick, WorldGen::WinterForeseenEvent,
										WorldGen::WinterPayload{Region, Next, FloorToUnsigned(Coming[Region].ColdSum),
																0u, PeopleHere, UsualSeverity},
										W.Entities().GetId(RH));
			}
		}
	}

	WinterStats MeasureWinters(const World& W, uint32 Region)
	{
		WinterStats S;
		const EventLog& Log = W.Log();
		for (const Event& E : Log.All())
		{
			if (E.Is(WorldGen::WinterEvent) || E.Is(WorldGen::WinterForeseenEvent))
			{
				const WorldGen::WinterPayload P = E.Get<WorldGen::WinterPayload>();
				if (Region != 0 && P.Region != Region)
				{
					continue;
				}
				if (E.Is(WorldGen::WinterForeseenEvent))
				{
					++S.Foreseen;
				}
				else
				{
					++S.Winters[P.Severity > 3u ? 3u : P.Severity];
					S.ColdDeaths += P.Deaths;
				}
			}
			else if (E.Is(StockTakenEvent) && E.Cause.IsValid())
			{
				const StockPayload P = E.Get<StockPayload>();
				if (Region != 0 && P.Region != Region)
				{
					continue;
				}
				const Event* Cause = History::FindEvent(Log, E.Cause);
				if (Cause == nullptr || !Cause->Is(WorldGen::WinterEvent))
				{
					continue;
				}
				if (P.Good == G_TIMBER)
				{
					S.TimberTaken += P.Amount;
				}
				else if (P.Good == G_GRAIN)
				{
					S.GrainTaken += P.Amount;
					S.HousesTaken += P.House != 0u ? 1u : 0u;
				}
			}
		}
		return S;
	}
} // namespace Vaelen::Economy
