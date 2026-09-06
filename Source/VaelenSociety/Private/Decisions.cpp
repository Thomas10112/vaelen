// VAELEN - VaelenSociety
// Phase 05.05: organisations acting.
//
// STATUS: VALIDATED (Phase 05) - unit/integration tests in Tests/Society

#include "Vaelen/Society/Decisions.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Disasters.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Society
{
	using Population::LifeState;
	using Population::PersonInfo;

	namespace
	{
		bool IsAlive(const PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(LifeState::Alive);
		}
	} // namespace

	const char* DecisionKindName(DecisionKind K) noexcept
	{
		switch (K)
		{
		case DecisionKind::StoreGrain:
			return "laid in grain";
		case DecisionKind::Preach:
			return "preached";
		case DecisionKind::Train:
			return "trained";
		case DecisionKind::Raid:
			return "planned a raid";
		case DecisionKind::Count:
		default:
			return "?";
		}
	}

	DecisionTypes DecisionTypes::Declare(World& W)
	{
		DecisionTypes T;
		T.Stores = W.Types().Register<Population::RegionStores>("RegionStores");
		W.Components().CreatePool(T.Stores);
		return T;
	}

	void DecisionSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		std::vector<uint32> Regions;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const Population::RegionDetail& D) { Regions.push_back(D.Region); });
		std::sort(Regions.begin(), Regions.end());
		if (Regions.empty())
		{
			return;
		}
		std::vector<EntityHandle> RegionHandles;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= RegionHandles.size())
					{
						RegionHandles.resize(usize{R.Index} + 1u);
					}
					RegionHandles[R.Index] = H;
				});
		const Hash64 Key = HashBytes(reinterpret_cast<const char*>(&W.Map().Config()), sizeof(W.Map().Config()));
		if (Graph.Neighbours.empty() || GraphDigest != Key)
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Types.World.Regions);
			GraphDigest = Key;
		}
		// The droughts of the last years, per region: the latest event.
		const uint64 Memory = uint64{History::TicksPerYear} * std::max(1u, Rules.StoreAfterDroughtYears);
		std::vector<PersistentId> LastDrought(RegionHandles.size());
		const std::vector<Event>& Events = W.Log().All();
		for (usize i = Events.size(); i > 0; --i)
		{
			const Event& E = Events[i - 1];
			if (E.Tick + Memory <= Context.Tick)
			{
				break;
			}
			if (E.Is(History::DisasterStruckEvent))
			{
				const History::DisasterPayload P = E.Get<History::DisasterPayload>();
				if (P.Kind == static_cast<uint32>(History::DisasterKind::Drought) && P.Region < LastDrought.size() &&
					!LastDrought[P.Region].IsValid())
				{
					LastDrought[P.Region] = E.Id;
				}
			}
		}
		for (const uint32 Region : Regions)
		{
			if (Region >= RegionHandles.size() || RegionHandles[Region].IsNull())
			{
				continue;
			}
			const EntityHandle RH = RegionHandles[Region];
			std::vector<std::pair<EntityHandle, OrganizationInfo>> Seated;
			W.Components()
				.GetPool(Organizations.Organization)
				.ForEach(
					[&](EntityHandle H, const OrganizationInfo& O)
					{
						if (O.Region == Region && O.Disbanded == 0 && O.Members > 0)
						{
							Seated.push_back({H, O});
						}
					});
			std::sort(Seated.begin(), Seated.end(),
					  [](const auto& A, const auto& B) { return A.second.Index < B.second.Index; });
			bool Stored = false;
			for (const auto& [OH, O] : Seated)
			{
				const PersistentId Subject = W.Entities().GetId(OH);
				switch (static_cast<OrganizationKind>(O.Kind))
				{
				case OrganizationKind::Council:
				{
					if (!LastDrought[Region].IsValid())
					{
						break;
					}
					Population::RegionStores* St = W.Components().GetPool(Decisions.Stores).TryGet(RH);
					if (St != nullptr)
					{
						St->GrainPerMille = Rules.StorePerMille;
					}
					else
					{
						Population::RegionStores Fresh;
						Fresh.GrainPerMille = Rules.StorePerMille;
						W.Components().GetPool(Decisions.Stores).Add(RH, Fresh);
					}
					Stored = true;
					Context.Events->Publish(Context.Tick, DecisionMadeEvent,
											DecisionPayload{O.Index, static_cast<uint32>(DecisionKind::StoreGrain),
															Region, Rules.StorePerMille},
											Subject, LastDrought[Region]);
					break;
				}
				case OrganizationKind::Temple:
				{
					// Persons of other faiths, in index order; a share of them turns.
					std::vector<std::pair<uint32, EntityHandle>> Others;
					W.Components()
						.GetPool(Persons.Person)
						.ForEach(
							[&](EntityHandle H, const PersonInfo& P)
							{
								if (P.Region == Region && IsAlive(P) && P.Religion != O.Religion)
								{
									Others.push_back({P.Index, H});
								}
							});
					std::sort(Others.begin(), Others.end());
					const uint32 Wanted =
						std::min(Rules.PreachMax, static_cast<uint32>(uint64{static_cast<uint32>(Others.size())} *
																	  Rules.PreachPerMille / 1000u));
					if (Wanted == 0)
					{
						break;
					}
					// The least pious turn first.
					std::stable_sort(
						Others.begin(), Others.end(),
						[&](const auto& A, const auto& B)
						{
							const Population::PersonTraits* TA = W.Components().GetPool(Traits.Traits).TryGet(A.second);
							const Population::PersonTraits* TB = W.Components().GetPool(Traits.Traits).TryGet(B.second);
							const uint32 PA =
								TA != nullptr ? TA->Traits[static_cast<uint32>(Population::Trait::Piety)] : 128u;
							const uint32 PB =
								TB != nullptr ? TB->Traits[static_cast<uint32>(Population::Trait::Piety)] : 128u;
							return PA != PB ? PA < PB : A.first < B.first;
						});
					for (uint32 i = 0; i < Wanted; ++i)
					{
						W.Components().GetPool(Persons.Person).Get(Others[i].second).Religion = O.Religion;
					}
					Population::ReconcileRegion(W, Types, Persons, Region);
					Context.Events->Publish(
						Context.Tick, DecisionMadeEvent,
						DecisionPayload{O.Index, static_cast<uint32>(DecisionKind::Preach), Region, Wanted}, Subject);
					break;
				}
				case OrganizationKind::Guild:
				{
					uint32 Trained = 0;
					W.Components()
						.GetPool(Organizations.Member)
						.ForEach(
							[&](EntityHandle H, const Membership& M)
							{
								if (M.Organization != O.Index)
								{
									return;
								}
								Population::PersonTraits* T = W.Components().GetPool(Traits.Traits).TryGet(H);
								if (T == nullptr)
								{
									return;
								}
								uint8& Craft = T->Skills[static_cast<uint32>(Population::Skill::Craft)];
								Craft = static_cast<uint8>(std::min(255u, uint32{Craft} + Rules.TrainGain));
								++Trained;
							});
					Context.Events->Publish(
						Context.Tick, DecisionMadeEvent,
						DecisionPayload{O.Index, static_cast<uint32>(DecisionKind::Train), Region, Trained}, Subject);
					break;
				}
				case OrganizationKind::Warband:
				{
					const uint32 Year = static_cast<uint32>(Context.Tick / History::TicksPerYear);
					const uint32 FoundedYear = static_cast<uint32>(O.Founded / History::TicksPerYear);
					if (Rules.RaidEveryYears == 0 || (Year - FoundedYear) % Rules.RaidEveryYears != 0 ||
						Region >= Graph.Neighbours.size())
					{
						break;
					}
					uint32 Target = 0;
					uint32 Most = 0;
					for (const uint16 N : Graph.Neighbours[Region])
					{
						if (N >= RegionHandles.size() || RegionHandles[N].IsNull())
						{
							continue;
						}
						const History::RegionPopulation* P =
							W.Components().GetPool(Types.Population.Population).TryGet(RegionHandles[N]);
						if (P != nullptr && P->Total > Most)
						{
							Most = P->Total;
							Target = N;
						}
					}
					if (Target == 0)
					{
						break;
					}
					const uint32 Strength = O.Members * Rules.RaidStrengthPerMember;
					const PersistentId Raid = Context.Events->Publish(
						Context.Tick, RaidPlannedEvent, RaidPayload{O.Index, Region, Target, Strength}, Subject);
					Context.Events->Publish(
						Context.Tick, DecisionMadeEvent,
						DecisionPayload{O.Index, static_cast<uint32>(DecisionKind::Raid), Region, Strength}, Subject,
						Raid);
					break;
				}
				case OrganizationKind::Clan:
				case OrganizationKind::Count:
				default:
					break;
				}
			}
			// Stores not renewed this year are spent.
			if (!Stored)
			{
				Population::RegionStores* St = W.Components().GetPool(Decisions.Stores).TryGet(RH);
				if (St != nullptr && St->GrainPerMille > 0)
				{
					St->GrainPerMille = 0;
				}
			}
		}
	}

	const Population::RegionStores* StoresOf(const World& W, const History::PreHistoryTypes& Types,
											 const DecisionTypes& Decisions, uint32 Region)
	{
		const Population::RegionStores* Found = nullptr;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && Found == nullptr)
					{
						Found = W.Components().GetPool(Decisions.Stores).TryGet(H);
					}
				});
		return Found;
	}

	DecisionStats MeasureDecisions(const World& W, const History::PreHistoryTypes& Types,
								   const DecisionTypes& Decisions)
	{
		DecisionStats S;
		for (const Event& E : W.Log().All())
		{
			if (E.Is(DecisionMadeEvent))
			{
				const DecisionPayload P = E.Get<DecisionPayload>();
				++S.Decisions;
				if (P.Kind < static_cast<uint32>(DecisionKind::Count))
				{
					++S.PerKind[P.Kind];
				}
				S.Caused += E.Cause.IsValid() ? 1u : 0u;
				S.Converted += P.Kind == static_cast<uint32>(DecisionKind::Preach) ? P.Value : 0u;
			}
			else if (E.Is(RaidPlannedEvent))
			{
				++S.Raids;
			}
		}
		std::vector<std::pair<uint32, Population::RegionStores>> All;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const Population::RegionStores* St = W.Components().GetPool(Decisions.Stores).TryGet(H);
					if (St != nullptr)
					{
						All.push_back({R.Index, *St});
						S.RegionsWithGrain += St->GrainPerMille > 0 ? 1u : 0u;
					}
				});
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		Hash64 D = HashString("RegionStores");
		for (const auto& [Index, St] : All)
		{
			D = HashCombine(D, HashUInt64(Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&St), sizeof(St)));
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Society
