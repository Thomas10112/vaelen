// VAELEN - VaelenSim
// Phase 03.05: disasters and omens.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim

#include "Vaelen/Sim/Disasters.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::History
{
	namespace
	{
		constexpr uint32 KindCount = static_cast<uint32>(DisasterKind::Count);

		DisasterState* FindState(World& W, const DisasterTypes& Types) noexcept
		{
			DisasterState* Found = nullptr;
			W.Components()
				.GetPool(Types.State)
				.ForEach(
					[&](EntityHandle, DisasterState& S)
					{
						if (Found == nullptr)
						{
							Found = &S;
						}
					});
			return Found;
		}

		std::vector<EntityHandle> RegionHandles(World& W, const WorldGen::WorldSetup& Setup)
		{
			std::vector<EntityHandle> Handles;
			W.Components()
				.GetPool(Setup.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (Handles.size() <= R.Index)
						{
							Handles.resize(R.Index + 1u);
						}
						Handles[R.Index] = H;
					});
			return Handles;
		}

		uint32 Clamp1000(uint64 Value) noexcept
		{
			return Value > 1000u ? 1000u : static_cast<uint32>(Value);
		}

		// Kills People of a region, per culture in proportion. Returns the deaths.
		uint32 Kill(RegionPopulation& P, uint32 Deaths) noexcept
		{
			const uint32 TotalBefore = P.Total;
			if (TotalBefore == 0 || Deaths == 0)
			{
				return 0;
			}
			Deaths = Deaths > TotalBefore ? TotalBefore : Deaths;
			uint32 Killed = 0;
			for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
			{
				if (P.Culture[S] != 0 && P.Count[S] > 0)
				{
					const uint32 Share = static_cast<uint32>(uint64{Deaths} * P.Count[S] / TotalBefore);
					Killed += P.Remove(P.Culture[S], Share);
				}
			}
			// Rounding leaves a few alive; take them from the majority.
			if (Killed < Deaths && P.Majority != 0)
			{
				Killed += P.Remove(P.Majority, Deaths - Killed);
			}
			P.Recount();
			return Killed;
		}
	} // namespace

	const char* DisasterName(DisasterKind Kind) noexcept
	{
		switch (Kind)
		{
		case DisasterKind::Drought:
			return "Drought";
		case DisasterKind::Flood:
			return "Flood";
		case DisasterKind::Eruption:
			return "Eruption";
		case DisasterKind::Plague:
			return "Plague";
		case DisasterKind::Count:
			break;
		}
		return "Unknown";
	}

	DisasterTypes DisasterTypes::Declare(World& W)
	{
		DisasterTypes T;
		T.Disaster = W.Types().Register<DisasterInfo>("DisasterInfo");
		T.State = W.Types().Register<DisasterState>("DisasterState");
		W.Components().CreatePool(T.Disaster);
		W.Components().CreatePool(T.State);
		return T;
	}

	EntityHandle InitializeDisasters(World& W, const DisasterTypes& Types)
	{
		const bool Fresh = FindState(W, Types) == nullptr;
		VAELEN_CHECKF(Fresh, "InitializeDisasters called twice");
		if (!Fresh)
		{
			return EntityHandle{};
		}
		const EntityHandle H = W.CreateEntity(IdKind::Entity);
		W.Components().GetPool(Types.State).Add(H, DisasterState{});
		return H;
	}

	// ── Hazards ──────────────────────────────────────────────────────────────

	uint32 PlagueRisk(uint32 People, uint32 Tiles, const DisasterRules& Rules) noexcept
	{
		if (Tiles == 0 || People == 0 || Rules.PlagueDensity == 0)
		{
			return 0;
		}
		return Clamp1000(uint64{People} * 1000u / (uint64{Tiles} * Rules.PlagueDensity));
	}

	std::vector<RegionHazard> ComputeHazards(const World& W, const WorldGen::WorldSetup& Setup,
											 const DisasterRules& Rules)
	{
		std::vector<RegionHazard> Out;
		const WorldMap& Map = W.Map();
		if (Map.GetLayerBase(Setup.Regions.RegionIndex.Index) == nullptr ||
			Map.GetLayerBase(Setup.Layers.Moisture.Index) == nullptr ||
			Map.GetLayerBase(Setup.Layers.Elevation.Index) == nullptr)
		{
			return Out;
		}
		W.Components()
			.GetPool(Setup.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle, const WorldGen::RegionInfo& R)
				{
					if (Out.size() <= R.Index)
					{
						Out.resize(R.Index + 1u);
					}
					Out[R.Index].Tiles = R.Tiles;
					Out[R.Index].RiverTiles = R.RiverTiles;
				});
		if (Out.empty())
		{
			return Out;
		}
		const TileLayer<uint16>& RegionIx = Map.GetLayer(Setup.Regions.RegionIndex);
		const TileLayer<int64>& Moisture = Map.GetLayer(Setup.Layers.Moisture);
		const TileLayer<int64>& Elevation = Map.GetLayer(Setup.Layers.Elevation);
		const int64 MountainLine =
			Map.Config().SeaLevel + Fix64::FromInt(static_cast<int32>(Rules.MountainElevation)).Raw;
		std::vector<uint64> MoistureSum(Out.size(), 0);
		std::vector<uint32> Counted(Out.size(), 0);
		const uint32 TileCount = Map.Grid().Width * Map.Grid().Height;
		for (uint32 T = 0; T < TileCount; ++T)
		{
			const uint16 R = RegionIx[T];
			if (R == 0 || R >= Out.size())
			{
				continue;
			}
			const int64 M = Moisture[T];
			MoistureSum[R] += static_cast<uint64>(M < 0 ? 0 : M);
			++Counted[R];
			if (Elevation[T] >= MountainLine)
			{
				++Out[R].MountainTiles;
			}
		}
		for (uint32 R = 1; R < Out.size(); ++R)
		{
			RegionHazard& H = Out[R];
			if (Counted[R] == 0)
			{
				continue;
			}
			// Fix64 in [0, 1]: per mille = raw * 1000 / 2^32.
			H.MoisturePerMille = Clamp1000((MoistureSum[R] / Counted[R]) * 1000u >> 32);
			H.Risk[static_cast<uint32>(DisasterKind::Drought)] =
				H.MoisturePerMille < Rules.DroughtMoisture && Rules.DroughtMoisture > 0
					? (Rules.DroughtMoisture - H.MoisturePerMille) * 1000u / Rules.DroughtMoisture
					: 0u;
			const uint32 RiverShare = H.Tiles > 0 ? Clamp1000(uint64{H.RiverTiles} * 1000u / H.Tiles) : 0u;
			H.Risk[static_cast<uint32>(DisasterKind::Flood)] =
				Rules.FloodRiverShare > 0 ? Clamp1000(uint64{RiverShare} * 1000u / Rules.FloodRiverShare) : 0u;
			const uint32 MountainShare = H.Tiles > 0 ? Clamp1000(uint64{H.MountainTiles} * 1000u / H.Tiles) : 0u;
			H.Risk[static_cast<uint32>(DisasterKind::Eruption)] =
				Rules.EruptionMountainShare > 0 ? Clamp1000(uint64{MountainShare} * 1000u / Rules.EruptionMountainShare)
												: 0u;
			H.Risk[static_cast<uint32>(DisasterKind::Plague)] = 0; // per year
		}
		return Out;
	}

	// ── The system ───────────────────────────────────────────────────────────

	void DisasterSystem::Tick(TickContext& Context)
	{
		DisasterState* S = FindState(*Owner, Types);
		if (S == nullptr || Context.Events == nullptr || Context.Random == nullptr)
		{
			return;
		}
		World& W = *Owner;
		const ITileLayer* RegionLayer = W.Map().GetLayerBase(Setup.Regions.RegionIndex.Index);
		if (RegionLayer == nullptr)
		{
			return;
		}
		// The region layer is a pure function of the world seed and the generation
		// config, so the config is the cache key: hashing the layer itself costs a
		// pass over every tile per tick (2 MB at 1024).
		const Hash64 Digest = HashBytes(reinterpret_cast<const char*>(&W.Map().Config()), sizeof(WorldGenConfig));
		if (Digest != HazardDigest || Hazards.empty())
		{
			Hazards = ComputeHazards(W, Setup, Rules);
			HazardDigest = Digest;
		}
		const std::vector<EntityHandle> Regions = RegionHandles(W, Setup);
		RandomStream& Random = *Context.Random;
		auto IsDetailed = [&](EntityHandle E)
		{
			const RegionLod* L = HasLod ? W.Components().GetPool(Lod).TryGet(E) : nullptr;
			return L != nullptr && L->Level <= RegionLod::DetailedLevel;
		};

		// 1. Last year's omens strike or pass.
		const uint32 PendingCount = S->PendingCount;
		PendingOmen Pending[DisasterState::MaxPending];
		for (uint32 i = 0; i < PendingCount; ++i)
		{
			Pending[i] = S->Pending[i];
			S->Pending[i] = PendingOmen{};
		}
		S->PendingCount = 0;
		for (uint32 i = 0; i < PendingCount; ++i)
		{
			const PendingOmen& O = Pending[i];
			if (O.Region == 0 || O.Region >= Regions.size() || Regions[O.Region].IsNull() || O.Kind >= KindCount)
			{
				continue;
			}
			if (Random.Below(1000) >= Rules.StrikePerMille)
			{
				continue;
			}
			// Severity 1..3: the higher the risk, the likelier the escalation.
			uint32 Severity = 1;
			if (Random.Below(1000) < Clamp1000(O.Risk) / 2u)
			{
				Severity = 2;
				if (Random.Below(1000) < Clamp1000(O.Risk) / 4u)
				{
					Severity = 3;
				}
			}
			RegionPopulation* P = W.Components().GetPool(Population.Population).TryGet(Regions[O.Region]);
			const uint32 PeopleBefore = P != nullptr ? P->Total : 0u;
			const uint32 Wanted =
				static_cast<uint32>(uint64{PeopleBefore} * Rules.DeathsPerMille[O.Kind][Severity - 1] / 1000u);
			// A detailed region has persons: its deaths belong to the life systems.
			const uint32 Deaths = P != nullptr && !IsDetailed(Regions[O.Region]) ? Kill(*P, Wanted) : 0u;

			DisasterInfo D;
			D.Index = ++S->Count;
			D.Kind = O.Kind;
			D.Region = O.Region;
			D.Severity = Severity;
			D.Struck = Context.Tick;
			D.Omen = O.Event;
			D.Deaths = Deaths;
			D.PeopleBefore = PeopleBefore;
			++S->PerKind[O.Kind];
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(Types.Disaster).Add(H, D);
			const PersistentId Struck = Context.Events->Publish(
				Context.Tick, DisasterStruckEvent, DisasterPayload{O.Region, O.Kind, Severity, Deaths},
				W.Entities().GetId(Regions[O.Region]), PersistentId{O.Event});

			if (HasReligion)
			{
				RegionFaith* F = W.Components().GetPool(Religion.Faith).TryGet(Regions[O.Region]);
				if (F != nullptr && F->Total() > 0)
				{
					// The dead were believers too: every faith loses its share of the
					// deaths, so believers never exceed the living.
					const uint32 Living = PeopleBefore > Deaths ? PeopleBefore - Deaths : 0u;
					const uint32 Believers = F->Total();
					if (Believers > Living)
					{
						const uint32 Excess = Believers - Living;
						for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
						{
							if (F->Religion[K] != 0)
							{
								F->Remove(F->Religion[K],
										  static_cast<uint32>(uint64{Excess} * F->Adherents[K] / Believers));
							}
						}
						while (F->Total() > Living && F->Majority != 0)
						{
							F->Remove(F->Majority, F->Total() - Living);
						}
					}
				}
				if (F != nullptr && F->Majority != 0)
				{
					const uint32 Slot = F->SlotOf(F->Majority);
					const uint32 Lost =
						static_cast<uint32>(uint64{F->Adherents[Slot]} * Rules.FaithShakenPerMille / 1000u);
					F->Remove(F->Majority, Lost);
				}
				F = W.Components().GetPool(Religion.Faith).TryGet(Regions[O.Region]);
				if (Religions != nullptr && Severity >= Rules.FoundingSeverity && (F == nullptr || F->Majority == 0) &&
					PeopleBefore > Deaths)
				{
					Religions->RequestFounding(O.Region, Struck, FoundingKind::Requested);
				}
			}
			if (Eras != nullptr && Severity >= Rules.EraSeverity && Deaths >= Rules.EraDeaths)
			{
				Eras->RequestEra(Struck);
			}
		}

		// 2. This year's omens, region by region and kind by kind.
		for (uint32 R = 1; R < Regions.size() && R < Hazards.size(); ++R)
		{
			if (Regions[R].IsNull())
			{
				continue;
			}
			const RegionPopulation* P = W.Components().GetPool(Population.Population).TryGet(Regions[R]);
			const uint32 People = P != nullptr ? P->Total : 0u;
			for (uint32 K = 0; K < KindCount; ++K)
			{
				const uint32 Risk = K == static_cast<uint32>(DisasterKind::Plague)
										? PlagueRisk(People, Hazards[R].Tiles, Rules)
										: Hazards[R].Risk[K];
				if (Risk == 0)
				{
					continue;
				}
				const uint64 Chance = uint64{Rules.OmenPerMille[K]} * Risk / 1000u;
				if (Random.Below(1000) >= Chance)
				{
					continue;
				}
				const PersistentId Id = Context.Events->Publish(Context.Tick, OmenEvent, OmenPayload{R, K, Risk, 0},
																W.Entities().GetId(Regions[R]));
				++S->Omens;
				if (S->PendingCount < DisasterState::MaxPending)
				{
					S->Pending[S->PendingCount] = PendingOmen{R, K, Risk, 0, Id.Value};
					++S->PendingCount;
				}
				else
				{
					++S->Dropped;
				}
			}
		}
	}

	// ── Queries ──────────────────────────────────────────────────────────────

	DisasterStats MeasureDisasters(const World& W, const DisasterTypes& Types)
	{
		DisasterStats S;
		std::vector<uint32> Regions;
		W.Components()
			.GetPool(Types.Disaster)
			.ForEach(
				[&](EntityHandle, const DisasterInfo& D)
				{
					++S.Total;
					if (D.Kind < KindCount)
					{
						++S.PerKind[D.Kind];
					}
					if (D.Severity >= 1 && D.Severity <= 3)
					{
						++S.PerSeverity[D.Severity - 1];
					}
					S.Deaths += D.Deaths;
					Regions.push_back(D.Region);
				});
		std::sort(Regions.begin(), Regions.end());
		S.RegionsStruck = static_cast<uint32>(std::unique(Regions.begin(), Regions.end()) - Regions.begin());
		W.Components()
			.GetPool(Types.State)
			.ForEach(
				[&](EntityHandle, const DisasterState& St)
				{
					S.Omens = St.Omens;
					S.Dropped = St.Dropped;
					S.Pending = St.PendingCount;
				});
		return S;
	}
} // namespace Vaelen::History
