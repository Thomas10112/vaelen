// VAELEN - VaelenPopulation
// Phase 04.03: families and lineage.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population

#include "Vaelen/Population/Families.h"

#include "Vaelen/Core/Random.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Population
{
	namespace
	{
		struct Ref
		{
			EntityHandle Handle;
			PersonInfo Info;
		};

		bool Alive(const PersonInfo& P) noexcept
		{
			return P.State == static_cast<uint8>(LifeState::Alive);
		}

		// Persons of a region alive, in index order, plus an index -> position map.
		void Gather(const World& W, const PersonTypes& Persons, uint32 Region, std::vector<Ref>& Out)
		{
			Out.clear();
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const PersonInfo& P)
					{
						if (P.Region == Region && Alive(P))
						{
							Out.push_back(Ref{H, P});
						}
					});
			std::sort(Out.begin(), Out.end(), [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
		}

		// Persons by index (copies), built once per query or per tick so that the
		// lineage walks cost a lookup, not a pool scan.
		struct Index
		{
			std::vector<PersonInfo> ByIndex;
			explicit Index(const World& W, const PersonTypes& Persons)
			{
				W.Components()
					.GetPool(Persons.Person)
					.ForEach(
						[&](EntityHandle, const PersonInfo& P)
						{
							if (ByIndex.size() <= P.Index)
							{
								ByIndex.resize(P.Index + 1u);
							}
							ByIndex[P.Index] = P;
						});
			}
			const PersonInfo* Find(uint32 I) const noexcept
			{
				return I != 0 && I < ByIndex.size() && ByIndex[I].Index == I ? &ByIndex[I] : nullptr;
			}
		};

		void AncestorsIn(const Index& Ix, uint32 Person, uint32 Depth, std::vector<uint32>& Out)
		{
			Out.clear();
			std::vector<uint32> Frontier;
			Frontier.push_back(Person);
			for (uint32 D = 0; D < Depth && !Frontier.empty(); ++D)
			{
				std::vector<uint32> Next;
				for (const uint32 P : Frontier)
				{
					const PersonInfo* Info = Ix.Find(P);
					if (Info == nullptr)
					{
						continue;
					}
					for (const uint32 Parent : {Info->Mother, Info->Father})
					{
						if (Parent != 0 && std::find(Out.begin(), Out.end(), Parent) == Out.end() &&
							std::find(Next.begin(), Next.end(), Parent) == Next.end())
						{
							Next.push_back(Parent);
						}
					}
				}
				std::sort(Next.begin(), Next.end());
				Out.insert(Out.end(), Next.begin(), Next.end());
				Frontier = Next;
			}
		}

		bool KinIn(const Index& Ix, uint32 A, uint32 B, uint32 Depth)
		{
			if (A == 0 || B == 0)
			{
				return false;
			}
			if (A == B)
			{
				return true;
			}
			std::vector<uint32> OfA;
			std::vector<uint32> OfB;
			AncestorsIn(Ix, A, Depth, OfA);
			AncestorsIn(Ix, B, Depth, OfB);
			OfA.push_back(A);
			OfB.push_back(B);
			for (const uint32 X : OfA)
			{
				if (std::find(OfB.begin(), OfB.end(), X) != OfB.end())
				{
					return true;
				}
			}
			return false;
		}
	} // namespace

	FamilyTypes FamilyTypes::Declare(World& W)
	{
		FamilyTypes T;
		T.Family = W.Types().Register<FamilyInfo>("FamilyInfo");
		W.Components().CreatePool(T.Family);
		return T;
	}

	void FamilySystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr || Context.Random == nullptr)
		{
			return;
		}
		World& W = *Owner;
		RandomStream& Random = *Context.Random;
		std::vector<uint32> Regions;
		W.Components()
			.GetPool(Persons.Detail)
			.ForEach([&](EntityHandle, const RegionDetail& D) { Regions.push_back(D.Region); });
		std::sort(Regions.begin(), Regions.end());
		uint32 FamilyCounter = 0;
		W.Components()
			.GetPool(Families.Family)
			.ForEach([&](EntityHandle, const FamilyInfo& F)
					 { FamilyCounter = F.Index > FamilyCounter ? F.Index : FamilyCounter; });
		const Index Ix(W, Persons);

		// 1. The dead release their spouses (any region, the dead may lie anywhere).
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, PersonInfo& P)
				{
					if (!Alive(P) || P.Spouse == 0)
					{
						return;
					}
					const PersonInfo* S = Ix.Find(P.Spouse);
					if (S == nullptr || !Alive(*S))
					{
						P.Spouse = 0;
					}
				});

		for (const uint32 Region : Regions)
		{
			std::vector<Ref> People;
			Gather(W, Persons, Region, People);

			// 2. Marriages: every unmarried adult who seeks a spouse this year takes
			//    the n-th eligible partner in index order.
			std::vector<uint8> Taken(People.size(), 0);
			for (usize i = 0; i < People.size(); ++i)
			{
				const PersonInfo& A = People[i].Info;
				const uint32 AgeA = AgeYears(A, Context.Tick);
				if (Taken[i] != 0 || A.Spouse != 0 || AgeA < Rules.MarryFrom || AgeA >= Rules.MarryTo ||
					A.Sex != static_cast<uint8>(Sex::Male))
				{
					continue; // grooms seek; brides are chosen, so each pair is drawn once
				}
				if (Random.Below(1000) >= Rules.MarriagesPerMille)
				{
					continue;
				}
				uint32 Eligible = 0;
				for (usize j = 0; j < People.size(); ++j)
				{
					const PersonInfo& B = People[j].Info;
					const uint32 AgeB = AgeYears(B, Context.Tick);
					const uint32 Gap = AgeA > AgeB ? AgeA - AgeB : AgeB - AgeA;
					if (Taken[j] == 0 && B.Spouse == 0 && B.Sex == static_cast<uint8>(Sex::Female) &&
						B.Culture == A.Culture && AgeB >= Rules.MarryFrom && AgeB < Rules.MarryTo &&
						Gap <= Rules.MaxAgeGap && (Rules.FaithMatters == 0 || B.Religion == A.Religion) &&
						!KinIn(Ix, A.Index, B.Index, 2))
					{
						++Eligible;
					}
				}
				if (Eligible == 0)
				{
					continue;
				}
				uint32 Pick = static_cast<uint32>(Random.Below(Eligible));
				for (usize j = 0; j < People.size(); ++j)
				{
					const PersonInfo& B = People[j].Info;
					const uint32 AgeB = AgeYears(B, Context.Tick);
					const uint32 Gap = AgeA > AgeB ? AgeA - AgeB : AgeB - AgeA;
					if (Taken[j] == 0 && B.Spouse == 0 && B.Sex == static_cast<uint8>(Sex::Female) &&
						B.Culture == A.Culture && AgeB >= Rules.MarryFrom && AgeB < Rules.MarryTo &&
						Gap <= Rules.MaxAgeGap && (Rules.FaithMatters == 0 || B.Religion == A.Religion) &&
						!KinIn(Ix, A.Index, B.Index, 2))
					{
						if (Pick > 0)
						{
							--Pick;
							continue;
						}
						Taken[i] = 1;
						Taken[j] = 1;
						PersonInfo& Groom = W.Components().GetPool(Persons.Person).Get(People[i].Handle);
						PersonInfo& Bride = W.Components().GetPool(Persons.Person).Get(People[j].Handle);
						Groom.Spouse = Bride.Index;
						Bride.Spouse = Groom.Index;
						// 3. A family for the couple: the groom's, founded now when he has none.
						if (Groom.Family == 0 && Rules.FoundOnMarriage != 0)
						{
							FamilyInfo F;
							F.Index = ++FamilyCounter;
							F.Culture = Groom.Culture;
							F.Region = Region;
							F.Head = Groom.Index;
							F.Founder = Groom.Index;
							F.Founded = Context.Tick;
							F.Identity = Noise::LatticeHash(W.Config().Seed ^ 0x46414d49ull,
															static_cast<int32>(F.Index), static_cast<int32>(Region));
							const EntityHandle FH = W.CreateEntity(IdKind::Family);
							W.Components().GetPool(Families.Family).Add(FH, F);
							Groom.Family = F.Index;
							Context.Events->Publish(Context.Tick, FamilyFoundedEvent,
													FamilyPayload{F.Index, Region, F.Head, F.Culture},
													W.Entities().GetId(FH));
						}
						Bride.Family = Groom.Family;
						People[i].Info = Groom;
						People[j].Info = Bride;
						Context.Events->Publish(Context.Tick, PersonMarriedEvent,
												MarriagePayload{Groom.Index, Bride.Index, Region, Groom.Family},
												W.Entities().GetId(People[i].Handle));
						break;
					}
				}
			}
		}

		// 4. Heads and extinction, every family in index order.
		std::vector<std::pair<EntityHandle, FamilyInfo>> All;
		W.Components()
			.GetPool(Families.Family)
			.ForEach([&](EntityHandle H, const FamilyInfo& F) { All.push_back({H, F}); });
		std::sort(All.begin(), All.end(), [](const auto& A, const auto& B) { return A.second.Index < B.second.Index; });
		for (auto& [FH, Info] : All)
		{
			if (Info.Extinct != 0)
			{
				continue;
			}
			std::vector<uint32> Members;
			FamilyMembers(W, Persons, Info.Index, Members);
			FamilyInfo& F = W.Components().GetPool(Families.Family).Get(FH);
			if (Members.empty())
			{
				F.Head = 0;
				F.Extinct = Context.Tick;
				Context.Events->Publish(Context.Tick, FamilyExtinctEvent,
										FamilyPayload{F.Index, F.Region, 0, F.Culture}, W.Entities().GetId(FH));
				continue;
			}
			const PersonInfo* Head = Ix.Find(F.Head);
			if (Head == nullptr || !Alive(*Head) || Head->Family != F.Index)
			{
				// The eldest living member leads.
				uint32 Eldest = Members.front();
				uint64 Born = 0xffffffffffffffffull;
				for (const uint32 M : Members)
				{
					const PersonInfo* P = Ix.Find(M);
					if (P != nullptr && P->Born < Born)
					{
						Born = P->Born;
						Eldest = M;
					}
				}
				F.Head = Eldest;
			}
			// Generation: the deepest known descent from the founder among the living.
			uint32 Deepest = 0;
			for (const uint32 M : Members)
			{
				uint32 Depth = 0;
				uint32 Cursor = M;
				while (Cursor != 0 && Cursor != F.Founder && Depth < 16)
				{
					const PersonInfo* P = Ix.Find(Cursor);
					if (P == nullptr)
					{
						break;
					}
					Cursor = P->Father != 0 ? P->Father : P->Mother;
					++Depth;
				}
				if (Cursor == F.Founder)
				{
					Deepest = Depth > Deepest ? Depth : Deepest;
				}
			}
			F.Generation = Deepest;
		}
	}

	// ── Queries ──────────────────────────────────────────────────────────────

	const PersonInfo* FindPerson(const World& W, const PersonTypes& Persons, uint32 Index)
	{
		const PersonInfo* Found = nullptr;
		if (Index == 0)
		{
			return nullptr;
		}
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					if (P.Index == Index && Found == nullptr)
					{
						Found = &P;
					}
				});
		return Found;
	}

	void Ancestors(const World& W, const PersonTypes& Persons, uint32 Person, uint32 Depth, std::vector<uint32>& Out)
	{
		const Index Ix(W, Persons);
		AncestorsIn(Ix, Person, Depth, Out);
	}

	void Descendants(const World& W, const PersonTypes& Persons, uint32 Person, uint32 Depth, std::vector<uint32>& Out)
	{
		Out.clear();
		std::vector<uint32> Frontier;
		Frontier.push_back(Person);
		for (uint32 D = 0; D < Depth && !Frontier.empty(); ++D)
		{
			std::vector<uint32> Next;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle, const PersonInfo& P)
					{
						for (const uint32 Parent : Frontier)
						{
							if (Parent != 0 && (P.Mother == Parent || P.Father == Parent) &&
								std::find(Out.begin(), Out.end(), P.Index) == Out.end() &&
								std::find(Next.begin(), Next.end(), P.Index) == Next.end())
							{
								Next.push_back(P.Index);
							}
						}
					});
			std::sort(Next.begin(), Next.end());
			Out.insert(Out.end(), Next.begin(), Next.end());
			Frontier = Next;
		}
	}

	void Siblings(const World& W, const PersonTypes& Persons, uint32 Person, std::vector<uint32>& Out)
	{
		Out.clear();
		const PersonInfo* Me = FindPerson(W, Persons, Person);
		if (Me == nullptr || (Me->Mother == 0 && Me->Father == 0))
		{
			return;
		}
		const uint32 Mother = Me->Mother;
		const uint32 Father = Me->Father;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					if (P.Index != Person &&
						((Mother != 0 && P.Mother == Mother) || (Father != 0 && P.Father == Father)))
					{
						Out.push_back(P.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	bool AreKin(const World& W, const PersonTypes& Persons, uint32 A, uint32 B, uint32 Depth)
	{
		const Index Ix(W, Persons);
		return KinIn(Ix, A, B, Depth);
	}

	void FamilyMembers(const World& W, const PersonTypes& Persons, uint32 Family, std::vector<uint32>& Out)
	{
		Out.clear();
		if (Family == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					if (P.Family == Family && Alive(P))
					{
						Out.push_back(P.Index);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	FamilyStats MeasureFamilies(const World& W, const PersonTypes& Persons, const FamilyTypes& Families,
								const FamilyRules& Rules, uint64 Tick)
	{
		FamilyStats S;
		std::vector<uint32> Sizes;
		W.Components()
			.GetPool(Families.Family)
			.ForEach(
				[&](EntityHandle, const FamilyInfo& F)
				{
					++S.Families;
					S.Extinct += F.Extinct != 0 ? 1u : 0u;
					if (Sizes.size() <= F.Index)
					{
						Sizes.resize(F.Index + 1u, 0);
					}
				});
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const PersonInfo& P)
				{
					if (!Alive(P))
					{
						return;
					}
					const uint32 Age = AgeYears(P, Tick);
					S.Adults += Age >= Rules.MarryFrom ? 1u : 0u;
					S.InAFamily += P.Family != 0 ? 1u : 0u;
					if (P.Family != 0 && P.Family < Sizes.size())
					{
						++Sizes[P.Family];
					}
					if (P.Spouse != 0)
					{
						++S.Married;
						const PersonInfo* Sp = FindPerson(W, Persons, P.Spouse);
						S.Broken += Sp == nullptr || !Alive(*Sp) || Sp->Spouse != P.Index ? 1u : 0u;
					}
				});
		for (const uint32 N : Sizes)
		{
			S.Largest = N > S.Largest ? N : S.Largest;
		}
		return S;
	}
} // namespace Vaelen::Population
