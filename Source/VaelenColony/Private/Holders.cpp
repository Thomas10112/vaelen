// VAELEN - VaelenColony
// Phase 11 task 11.04: who holds a colony.
//
// STATUS: PROTOTYPE (Phase 11) - unit/integration/deterministic/edge tests in Tests/Colony
#include "Vaelen/Colony/Holders.h"

#include "Vaelen/Sim/World.h"

namespace Vaelen::Colony
{
	namespace
	{
		bool IsColony(const World& W, const ColonyTypes& Colony, uint32 Region)
		{
			bool Found = false;
			W.Components()
				.GetPool(Colony.Colony)
				.ForEach([&](EntityHandle, const ColonyInfo& C) { Found = Found || C.Region == Region; });
			return Found;
		}
	} // namespace

	uint32 BindColony(World& W, const History::PreHistoryTypes& Types, const Population::PersonTypes& Persons,
					  const Society::BondageTypes& Bondage, const Society::StandingTypes& Standing,
					  const ColonyTypes& Colony, uint32 Region, SimTick Tick, BondingRules Rules)
	{
		(void)Types;
		if (Region == 0 || !IsColony(W, Colony, Region))
		{
			return 0;
		}
		// Collected first, bound after: BindPerson writes to the bond pool, and
		// nothing walks a pool it is adding to.
		std::vector<uint32> ToBind;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (P.Region != Region || P.State != static_cast<uint8>(Population::LifeState::Alive))
					{
						return;
					}
					if (Population::AgeYears(P, Tick) < Rules.FromAge)
					{
						return;
					}
					if (W.Components().GetPool(Bondage.Bond).TryGet(H) != nullptr)
					{
						return; // already bound: 05.04 owns them, not this
					}
					if (Rules.SpareTheElite != 0)
					{
						const Society::PersonStanding* S = W.Components().GetPool(Standing.Standing).TryGet(H);
						if (S != nullptr && S->Tier_ == static_cast<uint8>(Society::Tier::Elite))
						{
							return; // somebody has to hold the rest
						}
					}
					ToBind.push_back(P.Index);
				});
		uint32 Bound = 0;
		for (usize i = 0; i < ToBind.size(); ++i)
		{
			// Held by the colony itself, which is what Holder == 0 has meant
			// since 05.04. A colony is not a place of masters with twelve each;
			// it is one holding with hundreds in it, and the overseers of 05.01
			// are an organisation rather than a list of owners.
			const bool Enslave = uint64{i} * 1000u / (ToBind.empty() ? 1u : ToBind.size()) < Rules.EnslavedPerMille;
			Bound += Society::BindPerson(W, Persons, Bondage, ToBind[i],
										 Enslave ? Society::BondKind::Enslaved : Society::BondKind::Bonded,
										 Society::BondEntry::Promotion, 0u, Tick)
						 ? 1u
						 : 0u;
		}
		return Bound;
	}

	uint32 RaiseOverseers(World& W, const History::PreHistoryTypes& Types,
						  const Society::OrganizationTypes& Organizations, const ColonyTypes& Colony, uint32 Region,
						  uint32 Seats, SimTick Tick)
	{
		if (Region == 0 || !IsColony(W, Colony, Region))
		{
			return 0;
		}
		bool Already = false;
		W.Components()
			.GetPool(Organizations.Organization)
			.ForEach(
				[&](EntityHandle, const Society::OrganizationInfo& O)
				{
					Already = Already || (O.Region == Region && O.Disbanded == 0 &&
										  O.Kind == static_cast<uint32>(Society::OrganizationKind::Overseers));
				});
		if (Already)
		{
			return 0;
		}
		return Society::FoundOrganization(W, Types, Organizations, Society::OrganizationKind::Overseers, Region, Seats,
										  Tick);
	}

	HoldingStats MeasureHolding(const World& W, const Population::PersonTypes& Persons,
								const Society::BondageTypes& Bondage, const Society::StandingTypes& Standing,
								const Society::BondageRules& Rules, uint32 Region)
	{
		HoldingStats Out;
		std::vector<std::pair<uint32, uint32>> Held;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const Population::PersonInfo& P)
				{
					if (P.Region != Region || P.State != static_cast<uint8>(Population::LifeState::Alive))
					{
						return;
					}
					++Out.People;
					const Society::PersonStanding* S = W.Components().GetPool(Standing.Standing).TryGet(H);
					const Society::BondState* B = W.Components().GetPool(Bondage.Bond).TryGet(H);
					if (B == nullptr)
					{
						Out.Elites += S != nullptr && S->Tier_ == static_cast<uint8>(Society::Tier::Elite) ? 1u : 0u;
						return;
					}
					++Out.Bound;
					Out.Enslaved += B->Kind == static_cast<uint8>(Society::BondKind::Enslaved) ? 1u : 0u;
					if (B->Holder == 0)
					{
						++Out.ByRegion;
						return;
					}
					++Out.ByPerson;
					bool Seen = false;
					for (auto& [Who, Count] : Held)
					{
						if (Who == B->Holder)
						{
							++Count;
							Seen = true;
						}
					}
					if (!Seen)
					{
						Held.push_back({B->Holder, 1u});
					}
				});
		for (const auto& [Who, Count] : Held)
		{
			(void)Who;
			Out.Fullest = Count > Out.Fullest ? Count : Out.Fullest;
		}
		Out.Capacity = Out.Elites * Rules.MaxHeldPerHolder;
		return Out;
	}
} // namespace Vaelen::Colony
