// VAELEN - VaelenPopulation
// Phase 04.05: traits, skills and names of persons.
//
// STATUS: VALIDATED (Phase 04) - unit/deterministic/edge tests in Tests/Population

#include "Vaelen/Population/Traits.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Core/Random.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Population
{
	namespace
	{
		constexpr uint32 TraitCount = static_cast<uint32>(Trait::Count);
		constexpr uint32 SkillCount = static_cast<uint32>(Skill::Count);
		constexpr uint64 TraitSalt = 0x5452414954ull; // "TRAIT"

		uint8 Clamp255(uint32 V) noexcept
		{
			return static_cast<uint8>(V > 255u ? 255u : V);
		}

		struct Ref
		{
			EntityHandle Handle;
			PersonInfo Info;
		};

		struct LanguageSounds
		{
			uint32 Index = 0;
			uint32 Culture = 0;
			uint32 Generation = 0;
			uint64 Founded = 0;
			History::Phonology Sounds;
			EntityHandle Handle;
		};
	} // namespace

	const char* TraitName(Trait T) noexcept
	{
		switch (T)
		{
		case Trait::Vigour:
			return "Vigour";
		case Trait::Wit:
			return "Wit";
		case Trait::Will:
			return "Will";
		case Trait::Charm:
			return "Charm";
		case Trait::Boldness:
			return "Boldness";
		case Trait::Piety:
			return "Piety";
		case Trait::Count:
		default:
			return "?";
		}
	}

	const char* SkillName(Skill S) noexcept
	{
		switch (S)
		{
		case Skill::Farming:
			return "Farming";
		case Skill::Craft:
			return "Craft";
		case Skill::Fighting:
			return "Fighting";
		case Skill::Lore:
			return "Lore";
		case Skill::Count:
		default:
			return "?";
		}
	}

	Trait TraitBehind(Skill S) noexcept
	{
		switch (S)
		{
		case Skill::Farming:
			return Trait::Vigour;
		case Skill::Craft:
			return Trait::Wit;
		case Skill::Fighting:
			return Trait::Boldness;
		case Skill::Lore:
			return Trait::Piety;
		case Skill::Count:
		default:
			return Trait::Will;
		}
	}

	TraitTypes TraitTypes::Declare(World& W)
	{
		TraitTypes T;
		T.Traits = W.Types().Register<PersonTraits>("PersonTraits");
		W.Components().CreatePool(T.Traits);
		return T;
	}

	PersonTraits TraitsFromIdentity(Hash64 Identity) noexcept
	{
		PersonTraits T;
		for (uint32 K = 0; K < TraitCount; ++K)
		{
			// Three bytes of a lattice draw averaged: a bell around 128.
			const uint64 H = Noise::LatticeHash(Identity ^ TraitSalt, static_cast<int32>(K), 0);
			const uint32 Sum = static_cast<uint32>(H & 0xffu) + static_cast<uint32>((H >> 8) & 0xffu) +
							   static_cast<uint32>((H >> 16) & 0xffu);
			T.Traits[K] = static_cast<uint8>(Sum / 3u);
		}
		return T;
	}

	PersonTraits TraitsFromParents(Hash64 Identity, const PersonTraits& Mother, const PersonTraits& Father,
								   const TraitRules& Rules) noexcept
	{
		PersonTraits T = TraitsFromIdentity(Identity);
		const uint32 H = Rules.HeritabilityPerMille > 1000u ? 1000u : Rules.HeritabilityPerMille;
		for (uint32 K = 0; K < TraitCount; ++K)
		{
			const uint32 Parents = (uint32{Mother.Traits[K]} + uint32{Father.Traits[K]}) / 2u;
			T.Traits[K] = Clamp255((uint32{T.Traits[K]} * (1000u - H) + Parents * H) / 1000u);
		}
		return T;
	}

	void TraitSystem::Tick(TickContext& Context)
	{
		if (Context.Random == nullptr)
		{
			return;
		}
		World& W = *Owner;
		RandomStream& Random = *Context.Random;
		std::vector<Ref> People;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach([&](EntityHandle H, const PersonInfo& P) { People.push_back(Ref{H, P}); });
		if (People.empty())
		{
			return;
		}
		std::sort(People.begin(), People.end(), [](const Ref& A, const Ref& B) { return A.Info.Index < B.Info.Index; });
		// Person index -> position, for parents.
		const uint32 MaxIndex = People.back().Info.Index;
		std::vector<uint32> Position(usize{MaxIndex} + 1u, 0xffffffffu);
		for (usize i = 0; i < People.size(); ++i)
		{
			Position[People[i].Info.Index] = static_cast<uint32>(i);
		}
		auto TraitsOf = [&](uint32 Index) -> const PersonTraits*
		{
			if (Index == 0 || Index > MaxIndex || Position[Index] == 0xffffffffu)
			{
				return nullptr;
			}
			return W.Components().GetPool(Traits.Traits).TryGet(People[Position[Index]].Handle);
		};

		// 1. Traits for everyone who lacks them, parents first (index order).
		for (const Ref& R : People)
		{
			if (W.Components().GetPool(Traits.Traits).TryGet(R.Handle) != nullptr)
			{
				continue;
			}
			const PersonTraits* M = TraitsOf(R.Info.Mother);
			const PersonTraits* F = TraitsOf(R.Info.Father);
			PersonTraits T;
			if (M != nullptr && F != nullptr)
			{
				T = TraitsFromParents(R.Info.Identity, *M, *F, Rules);
			}
			else if (M != nullptr || F != nullptr)
			{
				const PersonTraits& One = M != nullptr ? *M : *F;
				T = TraitsFromParents(R.Info.Identity, One, One, Rules);
			}
			else
			{
				T = TraitsFromIdentity(R.Info.Identity);
			}
			W.Components().GetPool(Traits.Traits).Add(R.Handle, T);
		}

		// 2. Names in the person's language (or its culture's), once.
		if (Rules.NamePersons != 0)
		{
			std::vector<LanguageSounds> Languages;
			W.Components()
				.GetPool(Types.Languages.Language)
				.ForEach(
					[&](EntityHandle H, const History::LanguageInfo& L)
					{ Languages.push_back(LanguageSounds{L.Index, L.Culture, L.Generation, L.Founded, L.Sounds, H}); });
			std::sort(Languages.begin(), Languages.end(),
					  [](const LanguageSounds& A, const LanguageSounds& B) { return A.Index < B.Index; });
			auto Find = [&](uint32 Language, uint32 Culture) -> const LanguageSounds*
			{
				const LanguageSounds* Best = nullptr;
				for (const LanguageSounds& L : Languages)
				{
					if (L.Index == Language)
					{
						return &L;
					}
					if (L.Culture == Culture && (Best == nullptr || L.Founded >= Best->Founded))
					{
						Best = &L;
					}
				}
				return Best;
			};
			for (const Ref& R : People)
			{
				PersonTraits& T = W.Components().GetPool(Traits.Traits).Get(R.Handle);
				if (T.Named != 0)
				{
					continue;
				}
				if (W.Components().GetPool(Types.Languages.Name).TryGet(R.Handle) != nullptr)
				{
					T.Named = 1;
					continue;
				}
				const LanguageSounds* L = Find(R.Info.Language, R.Info.Culture);
				if (L == nullptr)
				{
					continue; // no language yet (a culture just split): tried again next year
				}
				T.Named = 1;
				History::NameInfo N;
				N.Language = L->Index;
				N.Scope = static_cast<uint32>(History::NameScope::Person);
				N.Key = R.Info.Identity;
				N.Salt = 0;
				N.Generation = L->Generation;
				N.Text = History::GenerateName(L->Sounds, History::NameScope::Person, N.Key, N.Salt);
				W.Components().GetPool(Types.Languages.Name).Add(R.Handle, N);
				History::LanguageInfo* Stored = W.Components().GetPool(Types.Languages.Language).TryGet(L->Handle);
				if (Stored != nullptr)
				{
					++Stored->Names;
				}
			}
		}

		// 3. A year of skill for the living: growth through youth and adult
		// life, an apprenticeship at the parents' side, a slow fading in old age.
		for (const Ref& R : People)
		{
			if (R.Info.State != static_cast<uint8>(LifeState::Alive))
			{
				continue;
			}
			const uint32 Age = AgeYears(R.Info, Context.Tick);
			if (Age < Rules.SkillFrom)
			{
				continue;
			}
			const PersonTraits* M = Age < Rules.ApprenticeTo ? TraitsOf(R.Info.Mother) : nullptr;
			const PersonTraits* F = Age < Rules.ApprenticeTo ? TraitsOf(R.Info.Father) : nullptr;
			PersonTraits& T = W.Components().GetPool(Traits.Traits).Get(R.Handle);
			for (uint32 S = 0; S < SkillCount; ++S)
			{
				if (Age >= Rules.DeclineFrom)
				{
					T.Skills[S] = static_cast<uint8>(T.Skills[S] > 0 ? T.Skills[S] - 1 : 0);
					continue;
				}
				if (Age >= Rules.SkillTo)
				{
					continue;
				}
				const uint32 Behind = uint32{T.Traits[static_cast<uint32>(TraitBehind(static_cast<Skill>(S)))]};
				const uint32 Draw = static_cast<uint32>(Random.Below(uint64{Rules.SkillGrowth} + 1u));
				uint32 Gain = Draw * (64u + Behind) / 192u;
				const uint32 Taught =
					std::max(M != nullptr ? uint32{M->Skills[S]} : 0u, F != nullptr ? uint32{F->Skills[S]} : 0u);
				Gain += Taught / 64u;
				const uint32 Cap = 128u + Behind / 2u;
				const uint32 Next = uint32{T.Skills[S]} + Gain;
				T.Skills[S] = Clamp255(Next > Cap ? std::max(Cap, uint32{T.Skills[S]}) : Next);
			}
		}
	}

	History::NameText PersonName(const World& W, const History::LanguageTypes& Languages, const PersonTypes& Persons,
								 uint32 Person)
	{
		History::NameText Out;
		if (Person == 0)
		{
			return Out;
		}
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					if (P.Index != Person)
					{
						return;
					}
					const History::NameInfo* N = W.Components().GetPool(Languages.Name).TryGet(H);
					if (N != nullptr)
					{
						Out = N->Text;
					}
				});
		return Out;
	}

	TraitStats MeasureTraits(const World& W, const History::PreHistoryTypes& Types, const PersonTypes& Persons,
							 const TraitTypes& Traits, uint32 Region)
	{
		TraitStats S;
		struct Row
		{
			uint32 Index;
			PersonTraits T;
		};
		std::vector<Row> Rows;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle H, const PersonInfo& P)
				{
					const PersonTraits* T = W.Components().GetPool(Traits.Traits).TryGet(H);
					if (T == nullptr)
					{
						return;
					}
					Rows.push_back(Row{P.Index, *T});
					if ((Region != 0 && P.Region != Region) || P.State != static_cast<uint8>(LifeState::Alive))
					{
						return;
					}
					++S.WithTraits;
					const bool Named = W.Components().GetPool(Types.Languages.Name).TryGet(H) != nullptr;
					S.Named += Named ? 1u : 0u;
					S.Unnamed += Named ? 0u : 1u;
					for (uint32 K = 0; K < TraitCount; ++K)
					{
						S.TraitSum[K] += T->Traits[K];
						S.TraitMin = std::min(S.TraitMin, T->Traits[K]);
						S.TraitMax = std::max(S.TraitMax, T->Traits[K]);
					}
					for (uint32 K = 0; K < SkillCount; ++K)
					{
						S.SkillSum[K] += T->Skills[K];
						S.SkillMax = std::max(S.SkillMax, T->Skills[K]);
					}
				});
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Index < B.Index; });
		Hash64 D = HashString("PersonTraits");
		for (const Row& R : Rows)
		{
			D = HashCombine(D, HashUInt64(R.Index));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R.T), sizeof(R.T)));
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Population
