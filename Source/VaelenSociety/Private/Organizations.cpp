// VAELEN - VaelenSociety
// Phase 05.01: organisations.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic/edge tests in Tests/Society

#include "Vaelen/Society/Organizations.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/Religion.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Society
{
	using Population::LifeState;
	using Population::PersonInfo;

	namespace
	{
		constexpr uint64 OrganizationSalt = 0x4f52475aull; // "ORGZ"

		struct Ref
		{
			EntityHandle Handle;
			PersonInfo Info;
			uint8 Piety = 0;
			uint8 Craft = 0;
			uint8 Fighting = 0;
		};

		Ref MakeRef(const World& W, const Population::TraitTypes& Traits, EntityHandle H, const PersonInfo& P)
		{
			const Population::PersonTraits* T = W.Components().GetPool(Traits.Traits).TryGet(H);
			Ref R{H, P, 128, 0, 0};
			if (T != nullptr)
			{
				R.Piety = T->Traits[static_cast<uint32>(Population::Trait::Piety)];
				R.Craft = T->Skills[static_cast<uint32>(Population::Skill::Craft)];
				R.Fighting = T->Skills[static_cast<uint32>(Population::Skill::Fighting)];
			}
			return R;
		}

		struct OrgRef
		{
			EntityHandle Handle;
			OrganizationInfo Info;
		};

		bool IsAlive(const PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(LifeState::Alive);
		}
	} // namespace

	const char* OrganizationKindName(OrganizationKind Kind) noexcept
	{
		switch (Kind)
		{
		case OrganizationKind::Council:
			return "council";
		case OrganizationKind::Temple:
			return "temple";
		case OrganizationKind::Guild:
			return "guild";
		case OrganizationKind::Warband:
			return "warband";
		case OrganizationKind::Clan:
			return "clan";
		case OrganizationKind::Count:
		default:
			return "?";
		}
	}

	OrganizationTypes OrganizationTypes::Declare(World& W)
	{
		OrganizationTypes T;
		T.Organization = W.Types().Register<OrganizationInfo>("OrganizationInfo");
		T.Member = W.Types().Register<Membership>("Membership");
		W.Components().CreatePool(T.Organization);
		W.Components().CreatePool(T.Member);
		return T;
	}

	void OrganizationSystem::Tick(TickContext& Context)
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
		uint32 NextIndex = 0;
		W.Components()
			.GetPool(Organizations.Organization)
			.ForEach([&](EntityHandle, const OrganizationInfo& O) { NextIndex = std::max(NextIndex, O.Index); });

		for (const uint32 Region : Regions)
		{
			if (Region >= RegionHandles.size() || RegionHandles[Region].IsNull())
			{
				continue;
			}
			const EntityHandle RH = RegionHandles[Region];
			const History::RegionPopulation* Counts = W.Components().GetPool(Types.Population.Population).TryGet(RH);
			const History::RegionFaith* Faith = W.Components().GetPool(Types.Religion.Faith).TryGet(RH);
			// The living of the region, in index order, with their piety.
			std::vector<Ref> People;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region == Region && IsAlive(P))
						{
							People.push_back(MakeRef(W, Traits, H, P));
						}
					});
			std::sort(People.begin(), People.end(),
					  [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
			// Living members per house.
			std::vector<uint32> HouseSize;
			for (const Ref& R : People)
			{
				if (R.Info.Family != 0)
				{
					if (R.Info.Family >= HouseSize.size())
					{
						HouseSize.resize(usize{R.Info.Family} + 1u, 0u);
					}
					++HouseSize[R.Info.Family];
				}
			}
			// Organisations seated here, alive, in index order.
			std::vector<OrgRef> Seated;
			W.Components()
				.GetPool(Organizations.Organization)
				.ForEach(
					[&](EntityHandle H, const OrganizationInfo& O)
					{
						if (O.Region == Region && O.Disbanded == 0)
						{
							Seated.push_back(OrgRef{H, O});
						}
					});
			std::sort(Seated.begin(), Seated.end(),
					  [](const OrgRef& A, const OrgRef& B) { return A.Info.Index < B.Info.Index; });
			auto HasKind = [&](OrganizationKind Kind, uint32 Religion)
			{
				for (const OrgRef& O : Seated)
				{
					if (O.Info.Kind == static_cast<uint32>(Kind) && O.Info.Religion == Religion)
					{
						return true;
					}
				}
				return false;
			};
			auto Found = [&](OrganizationKind Kind, uint32 Religion, uint32 Seats)
			{
				OrganizationInfo O;
				O.Index = ++NextIndex;
				O.Kind = static_cast<uint32>(Kind);
				O.Region = Region;
				O.Culture = Counts != nullptr ? Counts->Majority : 0u;
				O.Religion = Religion;
				O.Seats = Seats;
				O.Founded = Context.Tick;
				O.Identity = Noise::LatticeHash(W.Config().Seed ^ OrganizationSalt, static_cast<int32>(O.Index),
												static_cast<int32>(Region));
				const EntityHandle H = W.CreateEntity(IdKind::Organization);
				W.Components().GetPool(Organizations.Organization).Add(H, O);
				Context.Events->Publish(Context.Tick, OrganizationFoundedEvent,
										OrganizationPayload{O.Index, O.Kind, Region, 0}, W.Entities().GetId(H));
				Seated.push_back(OrgRef{H, O});
			};
			// 1. Foundings.
			if (Counts != nullptr && Counts->Total >= Rules.CouncilFromPeople && !HasKind(OrganizationKind::Council, 0))
			{
				Found(OrganizationKind::Council, 0, Rules.CouncilSeats);
			}
			uint32 Crafters = 0;
			uint32 Fighters = 0;
			for (const Ref& R : People)
			{
				Crafters += R.Craft >= Rules.SkilledFrom ? 1u : 0u;
				Fighters += R.Fighting >= Rules.SkilledFrom ? 1u : 0u;
			}
			if (Crafters >= Rules.GuildFromCrafters && Rules.GuildFromCrafters != 0 &&
				!HasKind(OrganizationKind::Guild, 0))
			{
				Found(OrganizationKind::Guild, 0, std::min(Rules.GuildMaxSeats, std::max(1u, Crafters / 2u)));
			}
			if (Fighters >= Rules.WarbandFromFighters && Rules.WarbandFromFighters != 0 &&
				!HasKind(OrganizationKind::Warband, 0))
			{
				Found(OrganizationKind::Warband, 0, std::min(Rules.WarbandMaxSeats, std::max(1u, Fighters / 2u)));
			}
			if (Faith != nullptr && Faith->Majority != 0)
			{
				const uint32 Believers = Faith->Adherents[Faith->SlotOf(Faith->Majority)];
				if (Believers >= Rules.TempleFromBelievers && !HasKind(OrganizationKind::Temple, Faith->Majority))
				{
					const uint32 Seats = std::min(
						Rules.TempleMaxSeats,
						std::max(1u, static_cast<uint32>(uint64{Believers} * Rules.TempleSeatsPerMille / 1000u)));
					Found(OrganizationKind::Temple, Faith->Majority, Seats);
				}
			}
			// 2. Seats: memberships of the dead and the departed released, the empty seats filled, a head seated.
			for (OrgRef& Org : Seated)
			{
				OrganizationInfo& O = W.Components().GetPool(Organizations.Organization).Get(Org.Handle);
				std::vector<Ref> Members;
				std::vector<EntityHandle> Released;
				W.Components()
					.GetPool(Organizations.Member)
					.ForEach(
						[&](EntityHandle H, const Membership& M)
						{
							if (M.Organization != O.Index)
							{
								return;
							}
							const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
							if (P == nullptr || !IsAlive(*P) || P->Region != Region)
							{
								Released.push_back(H);
								return;
							}
							Members.push_back(MakeRef(W, Traits, H, *P));
						});
				std::sort(Released.begin(), Released.end(),
						  [](EntityHandle A, EntityHandle B) { return A.Index() < B.Index(); });
				for (const EntityHandle H : Released)
				{
					const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					Context.Events->Publish(Context.Tick, MemberLeftEvent,
											OrganizationPayload{O.Index, O.Kind, Region, P != nullptr ? P->Index : 0u},
											W.Entities().GetId(H));
					W.Components().GetPool(Organizations.Member).Remove(H);
				}
				std::sort(Members.begin(), Members.end(),
						  [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
				// Candidates: free persons of age; heads of the largest houses for a
				// council, the most pious of the faith for a temple.
				if (Members.size() < O.Seats)
				{
					std::vector<Ref> Candidates;
					for (const Ref& R : People)
					{
						if (Population::AgeYears(R.Info, Context.Tick) < Rules.MemberFromAge ||
							W.Components().GetPool(Organizations.Member).TryGet(R.Handle) != nullptr)
						{
							continue;
						}
						if (O.Kind == static_cast<uint32>(OrganizationKind::Council))
						{
							const Population::FamilyInfo* F = nullptr;
							if (R.Info.Family != 0)
							{
								W.Components()
									.GetPool(Families.Family)
									.ForEach(
										[&](EntityHandle, const Population::FamilyInfo& Fam)
										{
											if (Fam.Index == R.Info.Family && F == nullptr)
											{
												F = &Fam;
											}
										});
							}
							if (F == nullptr || F->Head != R.Info.Index)
							{
								continue;
							}
						}
						else if (O.Kind == static_cast<uint32>(OrganizationKind::Temple) &&
								 R.Info.Religion != O.Religion)
						{
							continue;
						}
						else if (O.Kind == static_cast<uint32>(OrganizationKind::Guild) && R.Craft < Rules.SkilledFrom)
						{
							continue;
						}
						else if (O.Kind == static_cast<uint32>(OrganizationKind::Warband) &&
								 R.Fighting < Rules.SkilledFrom)
						{
							continue;
						}
						Candidates.push_back(R);
					}
					if (O.Kind == static_cast<uint32>(OrganizationKind::Council))
					{
						std::sort(Candidates.begin(), Candidates.end(),
								  [&](const Ref& A, const Ref& B)
								  {
									  const uint32 SA =
										  A.Info.Family < HouseSize.size() ? HouseSize[A.Info.Family] : 0u;
									  const uint32 SB =
										  B.Info.Family < HouseSize.size() ? HouseSize[B.Info.Family] : 0u;
									  return SA != SB ? SA > SB : A.Info.Index < B.Info.Index;
								  });
					}
					else if (O.Kind == static_cast<uint32>(OrganizationKind::Guild))
					{
						std::sort(Candidates.begin(), Candidates.end(), [](const Ref& A, const Ref& B)
								  { return A.Craft != B.Craft ? A.Craft > B.Craft : A.Info.Index < B.Info.Index; });
					}
					else if (O.Kind == static_cast<uint32>(OrganizationKind::Warband))
					{
						std::sort(Candidates.begin(), Candidates.end(),
								  [](const Ref& A, const Ref& B) {
									  return A.Fighting != B.Fighting ? A.Fighting > B.Fighting
																	  : A.Info.Index < B.Info.Index;
								  });
					}
					else
					{
						std::sort(Candidates.begin(), Candidates.end(), [](const Ref& A, const Ref& B)
								  { return A.Piety != B.Piety ? A.Piety > B.Piety : A.Info.Index < B.Info.Index; });
					}
					for (usize i = 0; i < Candidates.size() && Members.size() < O.Seats; ++i)
					{
						Membership M;
						M.Organization = O.Index;
						M.Since = Context.Tick;
						W.Components().GetPool(Organizations.Member).Add(Candidates[i].Handle, M);
						Context.Events->Publish(Context.Tick, MemberJoinedEvent,
												OrganizationPayload{O.Index, O.Kind, Region, Candidates[i].Info.Index},
												W.Entities().GetId(Candidates[i].Handle));
						Members.push_back(Candidates[i]);
					}
				}
				// The head: a living member, the eldest of a council, the most pious of a temple.
				bool HeadAlive = false;
				for (const Ref& M : Members)
				{
					HeadAlive = HeadAlive || M.Info.Index == O.Head;
				}
				if (!HeadAlive && !Members.empty())
				{
					const Ref* Best = &Members.front();
					for (const Ref& M : Members)
					{
						auto Key = [&](const Ref& X) -> uint32
						{
							switch (static_cast<OrganizationKind>(O.Kind))
							{
							case OrganizationKind::Temple:
								return X.Piety;
							case OrganizationKind::Guild:
								return X.Craft;
							case OrganizationKind::Warband:
								return X.Fighting;
							default:
								return 0;
							}
						};
						const bool ByKey = O.Kind != static_cast<uint32>(OrganizationKind::Council);
						const bool Better =
							ByKey ? (Key(M) > Key(*Best) || (Key(M) == Key(*Best) && M.Info.Index < Best->Info.Index))
								  : (M.Info.Born < Best->Info.Born ||
									 (M.Info.Born == Best->Info.Born && M.Info.Index < Best->Info.Index));
						if (Better)
						{
							Best = &M;
						}
					}
					O.Head = Best->Info.Index;
					Context.Events->Publish(Context.Tick, HeadSeatedEvent,
											OrganizationPayload{O.Index, O.Kind, Region, O.Head},
											W.Entities().GetId(Best->Handle));
				}
				for (const Ref& M : Members)
				{
					W.Components().GetPool(Organizations.Member).Get(M.Handle).Role = M.Info.Index == O.Head ? 1u : 0u;
				}
				if (Members.empty())
				{
					O.Head = 0;
				}
				O.Members = static_cast<uint32>(Members.size());
				// 3. Empty for too long: disbanded.
				O.Reserved[0] = Members.empty() ? O.Reserved[0] + 1u : 0u;
				if (Members.empty() && O.Reserved[0] >= Rules.DisbandAfterYears)
				{
					O.Disbanded = Context.Tick;
					Context.Events->Publish(Context.Tick, OrganizationDisbandedEvent,
											OrganizationPayload{O.Index, O.Kind, Region, 0},
											W.Entities().GetId(Org.Handle));
				}
			}
		}
	}

	void OrganizationsOf(const World& W, const OrganizationTypes& Types, uint32 Region,
						 std::vector<OrganizationInfo>& Out)
	{
		Out.clear();
		W.Components()
			.GetPool(Types.Organization)
			.ForEach(
				[&](EntityHandle, const OrganizationInfo& O)
				{
					if (O.Region == Region)
					{
						Out.push_back(O);
					}
				});
		std::sort(Out.begin(), Out.end(),
				  [](const OrganizationInfo& A, const OrganizationInfo& B) { return A.Index < B.Index; });
	}

	void MembersOf(const World& W, const Population::PersonTypes& Persons, const OrganizationTypes& Types,
				   uint32 Organization, std::vector<uint32>& Out)
	{
		Out.clear();
		W.Components()
			.GetPool(Types.Member)
			.ForEach(
				[&](EntityHandle H, const Membership& M)
				{
					const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					if (M.Organization == Organization && P != nullptr && IsAlive(*P))
					{
						Out.push_back(P->Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	uint32 OrganizationOf(const World& W, const Population::PersonTypes& Persons, const OrganizationTypes& Types,
						  uint32 Person)
	{
		uint32 Found = 0;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index == Person && Found == 0)
					{
						const Membership* M = W.Components().GetPool(Types.Member).TryGet(H);
						Found = M != nullptr ? M->Organization : 0u;
					}
				});
		return Found;
	}

	OrganizationStats MeasureOrganizations(const World& W, const History::PreHistoryTypes& Types,
										   const Population::PersonTypes& Persons, const OrganizationTypes& Types_)
	{
		OrganizationStats S;
		std::vector<OrganizationInfo> All;
		W.Components()
			.GetPool(Types_.Organization)
			.ForEach([&](EntityHandle, const OrganizationInfo& O) { All.push_back(O); });
		std::sort(All.begin(), All.end(),
				  [](const OrganizationInfo& A, const OrganizationInfo& B) { return A.Index < B.Index; });
		std::vector<uint32> Live;	  // memberships per organisation index
		std::vector<uint32> HeadSeen; // 1 when the head is a living member
		for (const OrganizationInfo& O : All)
		{
			if (O.Index >= Live.size())
			{
				Live.resize(usize{O.Index} + 1u, 0u);
				HeadSeen.resize(usize{O.Index} + 1u, 0u);
			}
		}
		W.Components()
			.GetPool(Types_.Member)
			.ForEach(
				[&](EntityHandle H, const Membership& M)
				{
					const PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
					const OrganizationInfo* O = nullptr;
					for (const OrganizationInfo& Candidate : All)
					{
						if (Candidate.Index == M.Organization)
						{
							O = &Candidate;
						}
					}
					if (P == nullptr || !IsAlive(*P) || O == nullptr || O->Disbanded != 0)
					{
						++S.Astray;
						return;
					}
					++S.Members;
					++Live[M.Organization];
					HeadSeen[M.Organization] = HeadSeen[M.Organization] | (P->Index == O->Head ? 1u : 0u);
				});
		Hash64 D = HashString("Organizations");
		for (const OrganizationInfo& O : All)
		{
			++S.Total;
			if (O.Kind < static_cast<uint32>(OrganizationKind::Count))
			{
				++S.PerKind[O.Kind];
			}
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&O), sizeof(O)));
			if (O.Disbanded != 0)
			{
				continue;
			}
			++S.Alive;
			S.HeadsAlive += O.Head != 0 && HeadSeen[O.Index] != 0 ? 1u : 0u;
			if (Population::IsDetailed(W, Types, Persons, O.Region) && Live[O.Index] != O.Members)
			{
				++S.CountMismatch;
			}
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Society
