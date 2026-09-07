// VAELEN - VaelenEconomy
// Phase 06.05: wealth and inheritance.
//
// STATUS: VALIDATED (Phase 06) - unit/integration/deterministic tests in Tests/Economy

#include "Vaelen/Economy/Wealth.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Economy
{
	namespace
	{
		struct House
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Region = 0;
			uint32 Head = 0;
			uint32 Culture = 0;
			uint64 Value = 0;
		};

		struct Child
		{
			uint32 Person = 0;
			uint32 Family = 0;
			uint64 Born = 0;
			uint8 Sex = 0;
		};
	} // namespace

	WealthTypes WealthTypes::Declare(World& W)
	{
		WealthTypes T;
		T.Wealth = W.Types().Register<Society::HouseWealth>("HouseWealth");
		T.Heir = W.Types().Register<HouseHeir>("HouseHeir");
		W.Components().CreatePool(T.Wealth);
		W.Components().CreatePool(T.Heir);
		return T;
	}

	void WealthSystem::Tick(TickContext& Context)
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
		// A house whose region went coarse again, or that died out, keeps neither
		// wealth nor heir: both are readings of a detailed region, and the stock
		// system folded its goods the same tick (06.01). This runs even when no
		// region is detailed, so that the last demotion leaves nothing behind.
		{
			std::vector<EntityHandle> Forget;
			W.Components()
				.GetPool(Families.Family)
				.ForEach(
					[&](EntityHandle H, const Population::FamilyInfo& F)
					{
						const bool Detailed =
							F.Extinct == 0 && std::find(Regions.begin(), Regions.end(), F.Region) != Regions.end();
						if (!Detailed && (W.Components().GetPool(Wealth.Wealth).TryGet(H) != nullptr ||
										  W.Components().GetPool(Wealth.Heir).TryGet(H) != nullptr))
						{
							Forget.push_back(H);
						}
					});
			for (const EntityHandle H : Forget)
			{
				W.Components().GetPool(Wealth.Wealth).Remove(H);
				W.Components().GetPool(Wealth.Heir).Remove(H);
			}
		}
		if (Regions.empty())
		{
			return;
		}
		// The markets of those regions, once.
		std::vector<const RegionMarket*> Market;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= Market.size())
					{
						Market.resize(usize{R.Index} + 1u, nullptr);
					}
					Market[R.Index] = W.Components().GetPool(Markets.Market).TryGet(H);
				});
		// The descent custom of every culture, once.
		std::vector<uint32> Line;
		W.Components()
			.GetPool(Types.Population.Culture)
			.ForEach(
				[&](EntityHandle H, const History::CultureInfo& C)
				{
					const Society::NormSet* N = W.Components().GetPool(Norms.Norms).TryGet(H);
					if (C.Index >= Line.size())
					{
						Line.resize(usize{C.Index} + 1u, 0u);
					}
					Line[C.Index] = N != nullptr ? N->Descent_ : 0u;
				});
		// A house that died out, or whose region went coarse, keeps neither wealth
		// nor heir: both are truths about the living.
		std::vector<EntityHandle> Gone;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					const bool Here =
						F.Extinct == 0 && std::find(Regions.begin(), Regions.end(), F.Region) != Regions.end();
					if (!Here && (W.Components().GetPool(Wealth.Wealth).TryGet(H) != nullptr ||
								  W.Components().GetPool(Wealth.Heir).TryGet(H) != nullptr))
					{
						Gone.push_back(H);
					}
				});
		for (const EntityHandle H : Gone)
		{
			W.Components().GetPool(Wealth.Wealth).Remove(H);
			W.Components().GetPool(Wealth.Heir).Remove(H);
		}
		// The living houses of the detailed regions, valued at their market.
		std::vector<House> Houses;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					if (F.Extinct != 0 || std::find(Regions.begin(), Regions.end(), F.Region) == Regions.end())
					{
						return;
					}
					const HouseStock* S = W.Components().GetPool(Economy.House).TryGet(H);
					const RegionMarket* M = F.Region < Market.size() ? Market[F.Region] : nullptr;
					if (S == nullptr || M == nullptr)
					{
						return;
					}
					Houses.push_back(House{H, F.Index, F.Region, F.Head, F.Culture, ValueOf(S->Amount, *M)});
				});
		std::sort(Houses.begin(), Houses.end(), [](const House& A, const House& B) { return A.Index < B.Index; });
		// The living children of every head, for the heirs.
		std::vector<uint32> HeadOf; // person index -> the house they head
		for (const House& Hs : Houses)
		{
			if (Hs.Head == 0)
			{
				continue;
			}
			if (Hs.Head >= HeadOf.size())
			{
				HeadOf.resize(usize{Hs.Head} + 1u, 0u);
			}
			HeadOf[Hs.Head] = Hs.Index;
		}
		std::vector<std::vector<Child>> Children(Houses.size());
		std::vector<uint32> HouseAt; // family index -> position + 1
		for (usize i = 0; i < Houses.size(); ++i)
		{
			if (Houses[i].Index >= HouseAt.size())
			{
				HouseAt.resize(usize{Houses[i].Index} + 1u, 0u);
			}
			HouseAt[Houses[i].Index] = static_cast<uint32>(i + 1);
		}
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& P)
				{
					if (P.State != static_cast<uint8>(Population::LifeState::Alive) || P.Family == 0 ||
						Population::AgeYears(P, Context.Tick) < Rules.HeirFromAge)
					{
						return;
					}
					for (const uint32 Parent : {P.Father, P.Mother})
					{
						const uint32 Home = Parent != 0 && Parent < HeadOf.size() ? HeadOf[Parent] : 0u;
						if (Home == 0 || Home == P.Family)
						{
							continue; // the head's own house is not its heir
						}
						const uint32 At = Home < HouseAt.size() ? HouseAt[Home] : 0u;
						if (At != 0 && HouseAt.size() > P.Family && HouseAt[P.Family] != 0)
						{
							Children[At - 1].push_back(Child{P.Index, P.Family, P.Born, P.Sex});
						}
					}
				});

		for (const uint32 Region : Regions)
		{
			// Rank the region's houses by value: 0 the poorest, 255 the richest.
			std::vector<usize> Here;
			for (usize i = 0; i < Houses.size(); ++i)
			{
				if (Houses[i].Region == Region)
				{
					Here.push_back(i);
				}
			}
			if (Here.empty())
			{
				continue;
			}
			// Built by hand rather than copied: gcc reads a vector copy under -O2 as a
			// possible null dereference and the kernel builds with warnings as errors.
			std::vector<usize> Order;
			Order.reserve(Here.size());
			for (const usize At : Here)
			{
				Order.push_back(At);
			}
			std::sort(Order.begin(), Order.end(),
					  [&](usize A, usize B) {
						  return Houses[A].Value != Houses[B].Value ? Houses[A].Value < Houses[B].Value
																	: Houses[A].Index < Houses[B].Index;
					  });
			const usize N = Order.size();
			for (usize Position = 0; Position < N; ++Position)
			{
				const House& Hs = Houses[Order[Position]];
				Society::HouseWealth Fresh;
				Fresh.Rank = static_cast<uint32>(N > 1 ? (Position * 255u) / (N - 1) : 255u);
				Fresh.Value = Hs.Value > 0xffffffffull ? 0xffffffffu : static_cast<uint32>(Hs.Value);
				Society::HouseWealth* Purse = W.Components().GetPool(Wealth.Wealth).TryGet(Hs.Handle);
				if (Purse != nullptr)
				{
					const uint32 Gap = Fresh.Rank > Purse->Rank ? Fresh.Rank - Purse->Rank : Purse->Rank - Fresh.Rank;
					if (uint64{Gap} * 1000u >= 255ull * Rules.ChangePerMille)
					{
						Context.Events->Publish(Context.Tick, FortuneChangedEvent,
												WealthPayload{Hs.Index, Region, Purse->Rank, Fresh.Rank},
												W.Entities().GetId(Hs.Handle));
					}
					*Purse = Fresh;
				}
				else
				{
					W.Components().GetPool(Wealth.Wealth).Add(Hs.Handle, Fresh);
				}
			}
			// Name the heirs: the eldest living child of the head who carries the
			// line and has a house of their own.
			for (const usize i : Here)
			{
				const House& Hs = Houses[i];
				const uint32 Descent =
					Hs.Culture < Line.size() ? Line[Hs.Culture] : static_cast<uint32>(Society::Descent::Patrilineal);
				const uint8 Carries = Descent == static_cast<uint32>(Society::Descent::Matrilineal)
										  ? static_cast<uint8>(Population::Sex::Female)
										  : static_cast<uint8>(Population::Sex::Male);
				const Child* Best = nullptr;
				for (const Child& C : Children[i])
				{
					if (C.Sex != Carries || Houses[HouseAt[C.Family] - 1].Region != Region)
					{
						continue;
					}
					if (Best == nullptr || C.Born < Best->Born || (C.Born == Best->Born && C.Person < Best->Person))
					{
						Best = &C;
					}
				}
				// Read the heir out once: the event below must not have to prove to a
				// compiler that a family index of zero implies a null child.
				HouseHeir Fresh;
				uint32 HeirPerson = 0;
				if (Best != nullptr)
				{
					Fresh.Family = Best->Family;
					HeirPerson = Best->Person;
				}
				HouseHeir* Named = W.Components().GetPool(Wealth.Heir).TryGet(Hs.Handle);
				if (Named == nullptr)
				{
					W.Components().GetPool(Wealth.Heir).Add(Hs.Handle, Fresh);
				}
				else if (Named->Family == Fresh.Family)
				{
					continue;
				}
				else
				{
					*Named = Fresh;
				}
				if (Fresh.Family != 0)
				{
					Context.Events->Publish(Context.Tick, HeirNamedEvent,
											WealthPayload{Hs.Index, Region, Fresh.Family, HeirPerson},
											W.Entities().GetId(Hs.Handle));
				}
			}
		}
	}

	const Society::HouseWealth* WealthOf(const World& W, const Population::FamilyTypes& Families,
										 const WealthTypes& Wealth, uint32 Family)
	{
		const Society::HouseWealth* Found = nullptr;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					if (F.Index == Family && Found == nullptr)
					{
						Found = W.Components().GetPool(Wealth.Wealth).TryGet(H);
					}
				});
		return Found;
	}

	const HouseHeir* HeirOf(const World& W, const Population::FamilyTypes& Families, const WealthTypes& Wealth,
							uint32 Family)
	{
		const HouseHeir* Found = nullptr;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					if (F.Index == Family && Found == nullptr)
					{
						Found = W.Components().GetPool(Wealth.Heir).TryGet(H);
					}
				});
		return Found;
	}

	void RichestOf(const World& W, const Population::FamilyTypes& Families, const WealthTypes& Wealth, uint32 Region,
				   std::vector<uint32>& Out)
	{
		Out.clear();
		std::vector<std::pair<uint32, uint32>> All; // value, index
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					const Society::HouseWealth* P = F.Region == Region && F.Extinct == 0
														? W.Components().GetPool(Wealth.Wealth).TryGet(H)
														: nullptr;
					if (P != nullptr)
					{
						All.push_back({P->Value, F.Index});
					}
				});
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B)
				  { return A.first != B.first ? A.first > B.first : A.second < B.second; });
		for (const auto& [Value, Index] : All)
		{
			Out.push_back(Index);
		}
	}

	WealthStats MeasureWealth(const World& W, const Population::FamilyTypes& Families, const WealthTypes& Wealth,
							  uint32 Region)
	{
		WealthStats S;
		struct Row
		{
			uint32 Index = 0;
			Society::HouseWealth Purse;
			HouseHeir Heir;
			bool HasPurse = false;
			bool HasHeir = false;
		};
		std::vector<Row> Rows;
		std::vector<uint32> Living;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle H, const Population::FamilyInfo& F)
				{
					if (F.Extinct == 0)
					{
						Living.push_back(F.Index);
					}
					if (Region != 0 && F.Region != Region)
					{
						return;
					}
					const Society::HouseWealth* P = W.Components().GetPool(Wealth.Wealth).TryGet(H);
					const HouseHeir* Hr = W.Components().GetPool(Wealth.Heir).TryGet(H);
					if (P == nullptr && Hr == nullptr)
					{
						return;
					}
					Row R;
					R.Index = F.Index;
					R.HasPurse = P != nullptr;
					R.HasHeir = Hr != nullptr;
					R.Purse = P != nullptr ? *P : Society::HouseWealth{};
					R.Heir = Hr != nullptr ? *Hr : HouseHeir{};
					S.Stale += F.Extinct != 0 && (P != nullptr || Hr != nullptr) ? 1u : 0u;
					Rows.push_back(R);
				});
		std::sort(Living.begin(), Living.end());
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Index < B.Index; });
		Hash64 D = HashString("Wealth");
		for (const Row& R : Rows)
		{
			if (R.HasPurse)
			{
				++S.Valued;
				S.Value += R.Purse.Value;
				S.Richest = std::max(S.Richest, R.Purse.Value);
				D = HashCombine(D, HashUInt64(R.Index));
				D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R.Purse), sizeof(R.Purse)));
			}
			if (R.HasHeir && R.Heir.Family != 0)
			{
				++S.WithHeir;
				S.Stale += std::binary_search(Living.begin(), Living.end(), R.Heir.Family) ? 0u : 1u;
				D = HashCombine(D, HashUInt64(R.Index));
				D = HashCombine(D, HashUInt64(R.Heir.Family));
			}
		}
		const WealthRules Rules;
		S.Rich = S.Valued * Rules.RichPerMille / 1000u;
		for (const Event& E : W.Log().All())
		{
			if (E.Is(FortuneChangedEvent) || E.Is(HeirNamedEvent))
			{
				const WealthPayload P = E.Get<WealthPayload>();
				if (Region != 0 && P.Region != Region)
				{
					continue;
				}
				S.Changes += E.Is(FortuneChangedEvent) ? 1u : 0u;
				S.HeirsNamed += E.Is(HeirNamedEvent) ? 1u : 0u;
			}
			else if (E.Is(StockInheritedEvent))
			{
				if (Region == 0 || E.Get<StockPayload>().Region == Region)
				{
					++S.Inheritances;
				}
			}
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Economy
