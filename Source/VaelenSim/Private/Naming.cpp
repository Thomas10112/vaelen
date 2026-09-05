// VAELEN - VaelenSim
// Phase 03.03: languages and deterministic naming.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim

#include "Vaelen/Sim/Naming.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/Hydrology.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <bit>

namespace Vaelen::History
{
	namespace
	{
		// Sound tables. Order is part of the frozen contract: names change if
		// an entry moves.
		constexpr const char* OnsetTable[Phonology::OnsetCount] = {
			"b",  "d",	"f",  "g",	"h",  "k",	"l",  "m", "n", "p", "r", "s", "t", "v", "w", "y", "z", // single
			"th", "sh", "kh", "dr", "kr", "st", "gr",													// clusters
		};
		constexpr const char* CodaTable[Phonology::CodaCount] = {
			"n",  "r",	"l",  "s",	"m",  "k",	"t",			  // single
			"th", "sh", "nd", "st", "rn", "ld", "nt", "rd", "ss", // clusters
		};
		constexpr const char* VowelTable[Phonology::VowelCount] = {
			"a",  "e",	"i",  "o",	"u",			  // single
			"ae", "ai", "ei", "ou", "ia", "io", "ua", // digraphs
		};
		// Scope suffixes: consonant-initial when the stem ends with a vowel,
		// vowel-initial when it ends with a consonant.
		constexpr const char* RiverSuffixC[4] = {"nar", "ren", "vis", "dun"};
		constexpr const char* RiverSuffixV[4] = {"ar", "en", "is", "un"};
		constexpr const char* LakeSuffixC[4] = {"mer", "wen", "lin", "nis"};
		constexpr const char* LakeSuffixV[4] = {"amer", "ewen", "ilin", "onis"};

		constexpr uint32 MaxStemChars = 12;

		constexpr uint32 Mask(uint32 Bits) noexcept
		{
			return Bits >= 32 ? 0xffffffffu : ((1u << Bits) - 1u);
		}

		bool IsVowelChar(char C) noexcept
		{
			return C == 'a' || C == 'e' || C == 'i' || C == 'o' || C == 'u' || C == 'A' || C == 'E' || C == 'I' ||
				   C == 'O' || C == 'U';
		}

		bool IsLetter(char C) noexcept
		{
			return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z');
		}

		// Small deterministic draw stream over a 64-bit state.
		struct Draw
		{
			uint64 State;
			uint32 Next(uint32 Bound) noexcept
			{
				State = HashUInt64(State + 0x9e3779b97f4a7c15ull);
				return Bound == 0 ? 0u : static_cast<uint32>((State >> 17) % Bound);
			}
			bool Chance(uint32 Percent) noexcept { return Next(100) < Percent; }
		};

		// Picks the i-th set bit (i < popcount).
		uint32 NthSetBit(uint32 Bits, uint32 N) noexcept
		{
			for (uint32 B = 0; B < 32; ++B)
			{
				if ((Bits >> B) & 1u)
				{
					if (N == 0)
					{
						return B;
					}
					--N;
				}
			}
			return 0;
		}

		// Adds set bits from a range until the count within the range reaches Want,
		// choosing by the filler hash so the result is deterministic.
		uint32 FillBits(uint32 Bits, uint32 RangeBits, uint32 Want, uint64& Filler) noexcept
		{
			const uint32 Range = Mask(RangeBits);
			while (std::popcount(Bits & Range) < static_cast<int>(Want) && (Bits & Range) != Range)
			{
				Filler = HashUInt64(Filler + 1u);
				const uint32 Missing = Range & ~Bits;
				const uint32 Pick =
					NthSetBit(Missing, static_cast<uint32>(Filler % static_cast<uint64>(std::popcount(Missing))));
				Bits |= 1u << Pick;
			}
			return Bits;
		}

		void Append(NameText& T, uint32& Length, const char* Piece) noexcept
		{
			for (const char* C = Piece; *C != '\0' && Length + 1 < NameText::Capacity; ++C)
			{
				T.Chars[Length++] = *C;
			}
			T.Chars[Length] = '\0';
		}

		uint32 PieceLength(const char* Piece) noexcept
		{
			uint32 N = 0;
			while (Piece[N] != '\0')
			{
				++N;
			}
			return N;
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

		struct LanguageRef
		{
			EntityHandle Handle;
			LanguageInfo Info;
		};

		// Languages indexed by the culture that speaks them (index 0 unused).
		std::vector<LanguageRef> LanguagesByCulture(World& W, const LanguageTypes& Types)
		{
			std::vector<LanguageRef> Out;
			W.Components()
				.GetPool(Types.Language)
				.ForEach(
					[&](EntityHandle H, const LanguageInfo& L)
					{
						if (Out.size() <= L.Culture)
						{
							Out.resize(L.Culture + 1u);
						}
						Out[L.Culture] = LanguageRef{H, L};
					});
			return Out;
		}

		// Gives a unique name in a scope; returns false when no salt is free.
		bool GiveName(World& W, const LanguageTypes& Types, const LanguageRules& Rules, TickContext& Context,
					  LanguageRef& Language, EntityHandle Target, NameScope Scope, uint64 Key)
		{
			for (uint32 Salt = 0; Salt < Rules.MaxSalt; ++Salt)
			{
				const NameText Text = GenerateName(Language.Info.Sounds, Scope, Key, Salt);
				if (IsNameUsed(W, Types, Scope, Text))
				{
					continue;
				}
				NameInfo N;
				N.Language = Language.Info.Index;
				N.Scope = static_cast<uint32>(Scope);
				N.Key = Key;
				N.Salt = Salt;
				N.Generation = Language.Info.Generation;
				N.Text = Text;
				W.Components().GetPool(Types.Name).Add(Target, N);
				LanguageInfo& Stored = W.Components().GetPool(Types.Language).Get(Language.Handle);
				++Stored.Names;
				Language.Info.Names = Stored.Names;
				Context.Events->Publish(Context.Tick, NamedEvent, NamePayload{N.Language, N.Scope, Key},
										W.Entities().GetId(Target));
				return true;
			}
			return false;
		}
	} // namespace

	// ── Types ────────────────────────────────────────────────────────────────

	LanguageTypes LanguageTypes::Declare(World& W)
	{
		LanguageTypes T;
		T.Language = W.Types().Register<LanguageInfo>("LanguageInfo");
		T.Name = W.Types().Register<NameInfo>("NameInfo");
		W.Components().CreatePool(T.Language);
		W.Components().CreatePool(T.Name);
		return T;
	}

	// ── Phonology ────────────────────────────────────────────────────────────

	bool IsNormalised(const Phonology& P) noexcept
	{
		const bool Onsets = std::popcount(P.Onsets & Mask(Phonology::SingleOnsets)) >= 4 &&
							std::popcount(P.Onsets & Mask(Phonology::OnsetCount)) >= 6 &&
							(P.Onsets & ~Mask(Phonology::OnsetCount)) == 0;
		const bool Codas = std::popcount(P.Codas & Mask(Phonology::SingleCodas)) >= 2 &&
						   std::popcount(P.Codas & Mask(Phonology::CodaCount)) >= 3 &&
						   (P.Codas & ~Mask(Phonology::CodaCount)) == 0;
		const bool Vowels = std::popcount(P.Vowels & Mask(Phonology::SingleVowels)) >= 3 &&
							(P.Vowels & ~Mask(Phonology::VowelCount)) == 0;
		uint32 Sum = 0;
		for (uint8 Wt : P.Weight)
		{
			Sum += Wt;
		}
		const bool Shapes = Sum > 0 && (P.Weight[0] > 0 || P.Weight[1] > 0);
		const bool Range = P.MinSyllables >= 1 && P.MinSyllables <= P.MaxSyllables && P.MaxSyllables <= 4;
		return Onsets && Codas && Vowels && Shapes && Range && P.Reserved[0] == 0 && P.Reserved[1] == 0;
	}

	Phonology NormalisePhonology(Phonology P, Hash64 Filler) noexcept
	{
		P.Onsets &= Mask(Phonology::OnsetCount);
		P.Codas &= Mask(Phonology::CodaCount);
		P.Vowels &= Mask(Phonology::VowelCount);
		P.Onsets = FillBits(P.Onsets, Phonology::SingleOnsets, 4, Filler);
		P.Onsets = FillBits(P.Onsets, Phonology::OnsetCount, 6, Filler);
		P.Codas = FillBits(P.Codas, Phonology::SingleCodas, 2, Filler);
		P.Codas = FillBits(P.Codas, Phonology::CodaCount, 3, Filler);
		P.Vowels = FillBits(P.Vowels, Phonology::SingleVowels, 3, Filler);
		if (P.Weight[0] == 0 && P.Weight[1] == 0)
		{
			P.Weight[0] = 1;
		}
		P.MinSyllables = P.MinSyllables < 1 ? 1 : (P.MinSyllables > 4 ? 4 : P.MinSyllables);
		P.MaxSyllables = P.MaxSyllables > 4 ? 4 : P.MaxSyllables;
		if (P.MaxSyllables < P.MinSyllables)
		{
			P.MaxSyllables = P.MinSyllables;
		}
		P.Reserved[0] = 0;
		P.Reserved[1] = 0;
		return P;
	}

	Phonology DerivePhonology(Hash64 Identity) noexcept
	{
		Draw D{HashCombine(Identity, HashString("Phonology"))};
		Phonology P;
		for (uint32 B = 0; B < Phonology::OnsetCount; ++B)
		{
			if (D.Chance(B < Phonology::SingleOnsets ? 55u : 35u))
			{
				P.Onsets |= 1u << B;
			}
		}
		for (uint32 B = 0; B < Phonology::CodaCount; ++B)
		{
			if (D.Chance(B < Phonology::SingleCodas ? 50u : 25u))
			{
				P.Codas |= 1u << B;
			}
		}
		for (uint32 B = 0; B < Phonology::VowelCount; ++B)
		{
			if (D.Chance(B < Phonology::SingleVowels ? 70u : 30u))
			{
				P.Vowels |= 1u << B;
			}
		}
		for (uint8& Wt : P.Weight)
		{
			Wt = static_cast<uint8>(D.Next(8));
		}
		P.MinSyllables = static_cast<uint8>(1u + D.Next(2));
		P.MaxSyllables = static_cast<uint8>(P.MinSyllables + 1u + D.Next(2));
		return NormalisePhonology(P, HashCombine(Identity, HashString("Filler")));
	}

	Phonology MutatePhonology(const Phonology& P, Hash64 Salt) noexcept
	{
		Draw D{HashCombine(Salt, HashString("Drift"))};
		Phonology Q = P;
		switch (D.Next(4))
		{
		case 0:
			Q.Onsets ^= 1u << D.Next(Phonology::OnsetCount);
			break;
		case 1:
			Q.Codas ^= 1u << D.Next(Phonology::CodaCount);
			break;
		case 2:
			Q.Vowels ^= 1u << D.Next(Phonology::VowelCount);
			break;
		default:
		{
			const uint32 Shape = D.Next(Phonology::ShapeCount);
			Q.Weight[Shape] = static_cast<uint8>((Q.Weight[Shape] + 1u + D.Next(3)) % 8u);
			break;
		}
		}
		return NormalisePhonology(Q, HashCombine(Salt, HashString("Filler")));
	}

	// ── Names ────────────────────────────────────────────────────────────────

	uint32 NameLength(const NameText& T) noexcept
	{
		uint32 N = 0;
		while (N < NameText::Capacity && T.Chars[N] != '\0')
		{
			++N;
		}
		return N;
	}

	bool NameEquals(const NameText& A, const NameText& B) noexcept
	{
		for (uint32 i = 0; i < NameText::Capacity; ++i)
		{
			if (A.Chars[i] != B.Chars[i])
			{
				return false;
			}
			if (A.Chars[i] == '\0')
			{
				return true;
			}
		}
		return true;
	}

	bool IsPronounceable(const NameText& T) noexcept
	{
		const uint32 Length = NameLength(T);
		if (Length < 2 || Length >= NameText::Capacity)
		{
			return false;
		}
		if (T.Chars[0] < 'A' || T.Chars[0] > 'Z')
		{
			return false;
		}
		uint32 Consonants = 0;
		uint32 Vowels = 0;
		uint32 AnyVowel = 0;
		for (uint32 i = 0; i < Length; ++i)
		{
			const char C = T.Chars[i];
			if (!IsLetter(C) || (i > 0 && C >= 'A' && C <= 'Z'))
			{
				return false;
			}
			if (IsVowelChar(C))
			{
				Consonants = 0;
				++Vowels;
				++AnyVowel;
				if (Vowels > 2)
				{
					return false;
				}
			}
			else
			{
				Vowels = 0;
				++Consonants;
				if (Consonants > 3)
				{
					return false;
				}
			}
		}
		for (uint32 i = Length; i < NameText::Capacity; ++i)
		{
			if (T.Chars[i] != '\0')
			{
				return false; // no bytes hidden after the terminator
			}
		}
		return AnyVowel > 0;
	}

	NameText GenerateName(const Phonology& In, NameScope Scope, uint64 Key, uint32 Salt) noexcept
	{
		const Phonology P = IsNormalised(In) ? In : NormalisePhonology(In, HashUInt64(Key));
		// The stream depends on the whole phonology, so two languages name the
		// same key differently.
		Hash64 Seed = HashBytes(reinterpret_cast<const char*>(&P), sizeof(P));
		Seed = HashCombine(Seed, HashUInt64(static_cast<uint64>(Scope)));
		Seed = HashCombine(Seed, HashUInt64(Key));
		Seed = HashCombine(Seed, HashUInt64(Salt));
		Draw D{Seed};

		uint32 WeightSum = 0;
		for (uint8 Wt : P.Weight)
		{
			WeightSum += Wt;
		}
		const uint32 Singles = P.Onsets & Mask(Phonology::SingleOnsets);
		const uint32 SingleVowels = P.Vowels & Mask(Phonology::SingleVowels);

		NameText T;
		uint32 Length = 0;
		// Stems that carry no suffix have at least two syllables.
		const bool Suffixed = Scope == NameScope::River || Scope == NameScope::Lake;
		const uint32 MinSyl = Suffixed || P.MinSyllables >= 2 ? P.MinSyllables : 2u;
		const uint32 MaxSyl = P.MaxSyllables < MinSyl ? MinSyl : P.MaxSyllables;
		const uint32 Syllables = MinSyl + D.Next(MaxSyl - MinSyl + 1u);
		bool PreviousCoda = false;
		bool PreviousCluster = false;
		char PreviousLast = '\0';
		bool PreviousVowelEnd = false;
		const char* PreviousOnset = "";
		uint32 PreviousVowel = 0xffffffffu;
		for (uint32 S = 0; S < Syllables; ++S)
		{
			// Shape: 0 CV, 1 CVC, 2 V, 3 VC.
			uint32 Pick = D.Next(WeightSum);
			uint32 Shape = 0;
			for (uint32 K = 0; K < Phonology::ShapeCount; ++K)
			{
				if (Pick < P.Weight[K])
				{
					Shape = K;
					break;
				}
				Pick -= P.Weight[K];
			}
			if (Syllables == 1 && Shape == 2)
			{
				Shape = 0; // a lone vowel is not a name
			}
			bool WantOnset = Shape == 0 || Shape == 1;
			const bool WantCoda = Shape == 1 || Shape == 3;
			if (PreviousVowelEnd)
			{
				WantOnset = true; // never two vowels across a boundary
			}
			if (PreviousCluster)
			{
				WantOnset = false; // a cluster coda is followed by a vowel
			}
			const char* Onset = "";
			if (WantOnset)
			{
				// After a single coda only a single consonant, and never the
				// same letter twice.
				uint32 Pool = P.Onsets;
				if (PreviousCoda)
				{
					Pool = Singles;
					for (uint32 B = 0; B < Phonology::SingleOnsets; ++B)
					{
						if (OnsetTable[B][0] == PreviousLast)
						{
							Pool &= ~(1u << B);
						}
					}
					if (Pool == 0)
					{
						Pool = Singles;
					}
				}
				Onset = OnsetTable[NthSetBit(Pool, D.Next(static_cast<uint32>(std::popcount(Pool))))];
			}
			// A digraph vowel is not followed by a vowel-initial syllable, and
			// the last syllable prefers single vowels so suffixes can attach.
			uint32 VowelPool = (S + 1 == Syllables) ? SingleVowels : P.Vowels;
			// No syllable repeats its predecessor (Lolol, Shosh): after the same
			// onset the vowel changes when the inventory allows it.
			if (S > 0 && Onset[0] == PreviousOnset[0] && Onset[1] == PreviousOnset[1] && PreviousVowel < 32 &&
				std::popcount(VowelPool & ~(1u << PreviousVowel)) > 0)
			{
				VowelPool &= ~(1u << PreviousVowel);
			}
			const uint32 VowelIndex = NthSetBit(VowelPool, D.Next(static_cast<uint32>(std::popcount(VowelPool))));
			const char* Vowel = VowelTable[VowelIndex];
			const char* Coda = "";
			if (WantCoda)
			{
				Coda = CodaTable[NthSetBit(P.Codas, D.Next(static_cast<uint32>(std::popcount(P.Codas))))];
			}
			const uint32 Added = PieceLength(Onset) + PieceLength(Vowel) + PieceLength(Coda);
			if (Length + Added > MaxStemChars)
			{
				break;
			}
			Append(T, Length, Onset);
			Append(T, Length, Vowel);
			Append(T, Length, Coda);
			PreviousOnset = Onset;
			PreviousVowel = VowelIndex;
			PreviousCoda = Coda[0] != '\0';
			PreviousCluster = PreviousCoda && Coda[1] != '\0';
			PreviousLast = PreviousCoda ? Coda[PieceLength(Coda) - 1] : '\0';
			PreviousVowelEnd = !PreviousCoda;
		}
		if (Length == 0)
		{
			// Cannot happen with a normalised phonology (one syllable always
			// fits); kept as a guard so the function never returns "".
			Append(T, Length, OnsetTable[NthSetBit(Singles, 0)]);
			Append(T, Length, VowelTable[NthSetBit(SingleVowels, 0)]);
			PreviousVowelEnd = true;
		}
		if (Length < 2)
		{
			// A single vowel survived the length cap: close it with a coda.
			Append(T, Length, CodaTable[NthSetBit(P.Codas & Mask(Phonology::SingleCodas), 0)]);
			PreviousVowelEnd = false;
		}
		if (Scope == NameScope::River || Scope == NameScope::Lake)
		{
			const uint32 Which = D.Next(4);
			const bool VowelEnd = IsVowelChar(T.Chars[Length - 1]);
			const char* Suffix = Scope == NameScope::River ? (VowelEnd ? RiverSuffixC[Which] : RiverSuffixV[Which])
														   : (VowelEnd ? LakeSuffixC[Which] : LakeSuffixV[Which]);
			Append(T, Length, Suffix);
		}
		T.Chars[0] = static_cast<char>(T.Chars[0] - 'a' + 'A');
		VAELEN_CHECKF(IsPronounceable(T), "generated name '%s' breaks the pronounceability invariant", T.Chars);
		return T;
	}

	// ── Queries ──────────────────────────────────────────────────────────────

	const NameInfo* NameOf(const World& W, const LanguageTypes& Types, EntityHandle H) noexcept
	{
		return W.Components().GetPool(Types.Name).TryGet(H);
	}

	bool IsNameUsed(const World& W, const LanguageTypes& Types, NameScope Scope, const NameText& Text) noexcept
	{
		bool Used = false;
		W.Components()
			.GetPool(Types.Name)
			.ForEach(
				[&](EntityHandle, const NameInfo& N)
				{
					if (!Used && N.Scope == static_cast<uint32>(Scope) && NameEquals(N.Text, Text))
					{
						Used = true;
					}
				});
		return Used;
	}

	NamingStats MeasureNames(const World& W, const LanguageTypes& Types)
	{
		NamingStats S;
		W.Components()
			.GetPool(Types.Language)
			.ForEach(
				[&](EntityHandle, const LanguageInfo& L)
				{
					++S.Languages;
					S.MaxGeneration = L.Generation > S.MaxGeneration ? L.Generation : S.MaxGeneration;
				});
		std::vector<NameInfo> All;
		W.Components().GetPool(Types.Name).ForEach([&](EntityHandle, const NameInfo& N) { All.push_back(N); });
		std::sort(All.begin(), All.end(),
				  [](const NameInfo& A, const NameInfo& B)
				  {
					  if (A.Scope != B.Scope)
					  {
						  return A.Scope < B.Scope;
					  }
					  for (uint32 i = 0; i < NameText::Capacity; ++i)
					  {
						  if (A.Text.Chars[i] != B.Text.Chars[i])
						  {
							  return A.Text.Chars[i] < B.Text.Chars[i];
						  }
					  }
					  return false;
				  });
		for (usize i = 0; i < All.size(); ++i)
		{
			++S.Names;
			if (All[i].Scope < static_cast<uint32>(NameScope::Count))
			{
				++S.PerScope[All[i].Scope];
			}
			S.MaxSalt = All[i].Salt > S.MaxSalt ? All[i].Salt : S.MaxSalt;
			if (i > 0 && All[i].Scope == All[i - 1].Scope && NameEquals(All[i].Text, All[i - 1].Text))
			{
				++S.Duplicates;
			}
		}
		return S;
	}

	void ExportNames(const World& W, const LanguageTypes& Types, NameScope Scope, std::string& Out)
	{
		std::vector<NameInfo> Names;
		W.Components()
			.GetPool(Types.Name)
			.ForEach(
				[&](EntityHandle, const NameInfo& N)
				{
					if (N.Scope == static_cast<uint32>(Scope))
					{
						Names.push_back(N);
					}
				});
		std::sort(Names.begin(), Names.end(), [](const NameInfo& A, const NameInfo& B) { return A.Key < B.Key; });
		Out.clear();
		for (const NameInfo& N : Names)
		{
			Out += std::to_string(N.Key);
			Out += '\t';
			Out += N.Text.Chars;
			Out += '\n';
		}
	}

	// ── The system ───────────────────────────────────────────────────────────

	void LanguageSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		std::vector<LanguageRef> ByCulture = LanguagesByCulture(W, Types);
		uint32 LanguageCounter = 0;
		for (const LanguageRef& L : ByCulture)
		{
			LanguageCounter = L.Info.Index > LanguageCounter ? L.Info.Index : LanguageCounter;
		}

		// 1. Every culture speaks a language; children drift from the parent's.
		struct CultureRef
		{
			EntityHandle Handle;
			CultureInfo Info;
		};
		std::vector<CultureRef> Cultures;
		W.Components()
			.GetPool(Population.Culture)
			.ForEach([&](EntityHandle H, const CultureInfo& C) { Cultures.push_back(CultureRef{H, C}); });
		std::sort(Cultures.begin(), Cultures.end(),
				  [](const CultureRef& A, const CultureRef& B) { return A.Info.Index < B.Info.Index; });
		for (const CultureRef& C : Cultures)
		{
			if (C.Info.Index < ByCulture.size() && !ByCulture[C.Info.Index].Handle.IsNull())
			{
				continue;
			}
			LanguageInfo L;
			L.Index = ++LanguageCounter;
			L.Culture = C.Info.Index;
			L.Founded = Context.Tick;
			L.Identity = HashCombine(C.Info.Identity, HashString("Language"));
			const bool HasParent =
				C.Info.Parent != 0 && C.Info.Parent < ByCulture.size() && !ByCulture[C.Info.Parent].Handle.IsNull();
			if (HasParent)
			{
				const LanguageInfo& Parent = ByCulture[C.Info.Parent].Info;
				L.Parent = Parent.Index;
				L.Sounds = MutatePhonology(Parent.Sounds, L.Identity);
			}
			else
			{
				L.Sounds = DerivePhonology(L.Identity);
			}
			const EntityHandle H = W.CreateEntity(IdKind::Language);
			W.Components().GetPool(Types.Language).Add(H, L);
			if (ByCulture.size() <= L.Culture)
			{
				ByCulture.resize(L.Culture + 1u);
			}
			ByCulture[L.Culture] = LanguageRef{H, L};
			Context.Events->Publish(Context.Tick, LanguageFoundedEvent,
									LanguagePayload{L.Index, L.Culture, L.Parent, 0}, W.Entities().GetId(H));
		}

		// 2. Drift: one sound change per DriftTicks since founding.
		for (LanguageRef& L : ByCulture)
		{
			if (L.Handle.IsNull() || Rules.DriftTicks == 0)
			{
				continue;
			}
			const uint64 Due = (Context.Tick - L.Info.Founded) / Rules.DriftTicks;
			if (Due > L.Info.Generation)
			{
				LanguageInfo& Stored = W.Components().GetPool(Types.Language).Get(L.Handle);
				++Stored.Generation;
				Stored.Sounds =
					MutatePhonology(Stored.Sounds, HashCombine(Stored.Identity, HashUInt64(Stored.Generation)));
				L.Info = Stored;
				Context.Events->Publish(Context.Tick, LanguageDriftedEvent,
										LanguagePayload{L.Info.Index, L.Info.Culture, L.Info.Parent, L.Info.Generation},
										W.Entities().GetId(L.Handle));
			}
		}

		// 3. Names: cultures and languages first, then the world.
		for (const CultureRef& C : Cultures)
		{
			LanguageRef& L = ByCulture[C.Info.Index];
			if (NameOf(W, Types, C.Handle) == nullptr)
			{
				GiveName(W, Types, Rules, Context, L, C.Handle, NameScope::Culture, C.Info.Index);
			}
			if (NameOf(W, Types, L.Handle) == nullptr)
			{
				GiveName(W, Types, Rules, Context, L, L.Handle, NameScope::Language, L.Info.Index);
			}
		}

		const std::vector<EntityHandle> Regions = RegionHandles(W, Setup);
		auto LanguageOfRegion = [&](uint32 RegionIndex) -> LanguageRef*
		{
			if (RegionIndex == 0 || RegionIndex >= Regions.size() || Regions[RegionIndex].IsNull())
			{
				return nullptr;
			}
			const RegionPopulation* P = W.Components().GetPool(Population.Population).TryGet(Regions[RegionIndex]);
			if (P == nullptr || P->Majority == 0 || P->Majority >= ByCulture.size() ||
				ByCulture[P->Majority].Handle.IsNull())
			{
				return nullptr;
			}
			return &ByCulture[P->Majority];
		};
		for (uint32 R = 1; R < Regions.size(); ++R)
		{
			if (Regions[R].IsNull() || NameOf(W, Types, Regions[R]) != nullptr)
			{
				continue;
			}
			if (LanguageRef* L = LanguageOfRegion(R))
			{
				GiveName(W, Types, Rules, Context, *L, Regions[R], NameScope::Region, R);
			}
		}

		const TileLayer<uint16>& RegionIx = W.Map().GetLayer(Setup.Regions.RegionIndex);
		const uint32 TileCount = W.Map().Grid().Width * W.Map().Grid().Height;
		auto RegionOfTile = [&](uint32 Tile) -> uint32 { return Tile < TileCount ? uint32{RegionIx[Tile]} : 0u; };
		if (Rules.NameRivers != 0)
		{
			struct Water
			{
				EntityHandle Handle;
				uint32 Index;
				uint32 Tile;
			};
			std::vector<Water> Rivers;
			W.Components()
				.GetPool(Setup.Types.River)
				.ForEach([&](EntityHandle H, const WorldGen::RiverInfo& Rv)
						 { Rivers.push_back(Water{H, Rv.Index, Rv.SourceTile}); });
			std::sort(Rivers.begin(), Rivers.end(), [](const Water& A, const Water& B) { return A.Index < B.Index; });
			for (const Water& Rv : Rivers)
			{
				if (NameOf(W, Types, Rv.Handle) != nullptr)
				{
					continue;
				}
				if (LanguageRef* L = LanguageOfRegion(RegionOfTile(Rv.Tile)))
				{
					GiveName(W, Types, Rules, Context, *L, Rv.Handle, NameScope::River, Rv.Index);
				}
			}
		}
		if (Rules.NameLakes != 0)
		{
			struct Water
			{
				EntityHandle Handle;
				uint32 Index;
				uint32 Tile;
			};
			std::vector<Water> Lakes;
			W.Components()
				.GetPool(Setup.Types.Lake)
				.ForEach([&](EntityHandle H, const WorldGen::LakeInfo& Lk)
						 { Lakes.push_back(Water{H, Lk.Index, Lk.OutletTile}); });
			std::sort(Lakes.begin(), Lakes.end(), [](const Water& A, const Water& B) { return A.Index < B.Index; });
			for (const Water& Lk : Lakes)
			{
				if (NameOf(W, Types, Lk.Handle) != nullptr)
				{
					continue;
				}
				if (LanguageRef* L = LanguageOfRegion(RegionOfTile(Lk.Tile)))
				{
					GiveName(W, Types, Rules, Context, *L, Lk.Handle, NameScope::Lake, Lk.Index);
				}
			}
		}

		// 4. Eras, in the language of the largest culture at the time.
		if (HasEra && Rules.NameEras != 0)
		{
			std::vector<uint64> People(ByCulture.size(), 0);
			W.Components()
				.GetPool(Population.Population)
				.ForEach(
					[&](EntityHandle, const RegionPopulation& P)
					{
						for (uint32 S = 0; S < RegionPopulation::MaxCultures; ++S)
						{
							if (P.Culture[S] != 0 && P.Culture[S] < People.size())
							{
								People[P.Culture[S]] += P.Count[S];
							}
						}
					});
			uint32 Largest = 0;
			for (uint32 C = 1; C < People.size(); ++C)
			{
				if (!ByCulture[C].Handle.IsNull() && (Largest == 0 || People[C] > People[Largest]))
				{
					Largest = C;
				}
			}
			if (Largest != 0)
			{
				struct EraRef
				{
					EntityHandle Handle;
					uint32 Index;
				};
				std::vector<EraRef> Eras;
				W.Components().GetPool(Era).ForEach([&](EntityHandle H, const EraInfo& E)
													{ Eras.push_back(EraRef{H, E.Index}); });
				std::sort(Eras.begin(), Eras.end(), [](const EraRef& A, const EraRef& B) { return A.Index < B.Index; });
				for (const EraRef& E : Eras)
				{
					if (NameOf(W, Types, E.Handle) == nullptr)
					{
						GiveName(W, Types, Rules, Context, ByCulture[Largest], E.Handle, NameScope::Era, E.Index);
					}
				}
			}
		}
	}
} // namespace Vaelen::History
