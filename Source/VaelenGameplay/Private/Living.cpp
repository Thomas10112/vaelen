// VAELEN - VaelenGameplay
// Phase 12 task 12.01: a person nobody is playing.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/deterministic/edge tests in Tests/Gameplay
#include "Vaelen/Gameplay/Living.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Gameplay
{
	namespace
	{
		bool IsAlive(const Population::PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(Population::LifeState::Alive);
		}
	} // namespace

	LivingTypes LivingTypes::Declare(World& W)
	{
		LivingTypes T;
		T.Lively = W.Types().Register<RegionLively>("RegionLively");
		W.Components().CreatePool(T.Lively);
		return T;
	}

	bool MakeLively(World& W, const History::PreHistoryTypes& Types, const LivingTypes& Living, uint32 Region)
	{
		if (Region == 0)
		{
			return false;
		}
		EntityHandle RH;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index == Region && RH.IsNull())
					{
						RH = H;
					}
				});
		if (RH.IsNull() || W.Components().GetPool(Living.Lively).TryGet(RH) != nullptr)
		{
			return false;
		}
		W.Components().GetPool(Living.Lively).Add(RH, RegionLively{});
		return true;
	}

	void LivingSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr || Context.Random == nullptr || Doing == nullptr)
		{
			return; // nobody to carry an act out, or no world to carry it in
		}
		World& W = *Owner;
		RandomStream& Random = *Context.Random;
		// The lively ground, from the pool rather than from a rule: ground
		// becomes lively at a tick (ADR-0090).
		std::vector<uint32> Lively;
		W.Components()
			.GetPool(Living.Lively)
			.ForEach(
				[&](EntityHandle H, const RegionLively&)
				{
					const WorldGen::RegionInfo* R = W.Components().GetPool(Types.World.RegionTypes_.Region).TryGet(H);
					if (R != nullptr && R->Index != 0)
					{
						Lively.push_back(R->Index);
					}
				});
		if (Lively.empty())
		{
			return;
		}
		std::sort(Lively.begin(), Lively.end());
		for (const uint32 Region : Lively)
		{
			// Everybody of the region who is old enough to act for themselves,
			// in person-index order so the same seed picks the same people.
			std::vector<uint32> Here;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const Population::PersonInfo& P)
					{
						if (P.Region != Region || !IsAlive(P) || Population::AgeYears(P, Context.Tick) < Rules.FromAge)
						{
							return;
						}
						Here.push_back(P.Index);
					});
			if (Here.size() < 2)
			{
				continue; // speaking and giving both want somebody to aim at
			}
			std::sort(Here.begin(), Here.end());
			for (usize i = 0; i < Here.size(); ++i)
			{
				if (Random.Below(1000) >= Rules.ActPerMille)
				{
					continue;
				}
				// Somebody else on the same ground. Thin on purpose: 12.02 is
				// what a person KNOWS, and until they know anybody this can only
				// be who is near.
				usize At = static_cast<usize>(Random.Below(static_cast<uint32>(Here.size())));
				if (At == i)
				{
					At = (At + 1u) % Here.size();
				}
				Player::PlayerCommand C;
				C.Issued = Context.Tick;
				C.Target = Here[At];
				C.Amount = Rules.GiveMost;
				// Only the conservative verbs. Work, Eat and Rest are already
				// done for everybody by 06.02 and 04.04, and doing them again
				// here would count a life's labour and food twice.
				C.Kind = static_cast<uint8>(Random.Below(1000) < Rules.SpeakSharePerMille ? Player::Intent::Speak
																						  : Player::Intent::Give);
				if (Doing->Allows(W, Here[i], C) != Player::Refusal::None)
				{
					continue; // the world would not have it, and it costs nothing
				}
				const PersistentId Act = Context.Events->Publish(
					Context.Tick, Player::PlayerActedEvent, Player::ActPayload{Here[i], C.Kind, C.Target, C.Amount});
				Doing->Do(W, Here[i], C, Context.Tick, Act);
			}
		}
	}

	LivingStats MeasureLiving(const World& W, const LivingTypes& Living, uint32 Region)
	{
		(void)Region;
		LivingStats Out;
		W.Components().GetPool(Living.Lively).ForEach([&](EntityHandle, const RegionLively&) { ++Out.Regions; });
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const Event& E : W.Log().All())
		{
			if (!E.Is(Player::PlayerActedEvent))
			{
				continue;
			}
			const Player::ActPayload& P = E.Get<Player::ActPayload>();
			++Out.Acts;
			Out.Spoke += P.Kind == static_cast<uint32>(Player::Intent::Speak) ? 1u : 0u;
			Out.Gave += P.Kind == static_cast<uint32>(Player::Intent::Give) ? 1u : 0u;
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&P), sizeof(Player::ActPayload)));
		}
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Gameplay
