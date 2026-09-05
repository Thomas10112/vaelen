// VAELEN - VaelenSim
// Phase 03.03: languages and deterministic naming.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim
//
// Every culture speaks a language: a phonology (inventories of onsets, codas
// and vowels drawn from fixed tables, syllable-shape weights, a syllable
// range) derived from the culture's identity hash, or mutated from the
// parent's language when the culture split. Names are built syllable by
// syllable from the phonology and a (scope, key, salt) triple, so they are
// pronounceable by construction, reproducible on every compiler, and unique
// per scope by retrying the salt. Names are components on the named entity
// (region, river, lake, culture, language, era); the yearly LanguageSystem
// founds languages, drifts them slowly and names whatever is nameless. No
// system holds state: everything lives in components and events.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/SimApi.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Sim/WorldGenPipeline.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::History
{
	/// Sound inventory of a language. Bit i of each mask selects entry i of
	/// the fixed table in Naming.cpp (OnsetTable, CodaTable, VowelTable).
	struct Phonology
	{
		static constexpr uint32 OnsetCount = 24; ///< entries 0-16 are single letters
		static constexpr uint32 SingleOnsets = 17;
		static constexpr uint32 CodaCount = 16; ///< entries 0-6 are single letters
		static constexpr uint32 SingleCodas = 7;
		static constexpr uint32 VowelCount = 12; ///< entries 0-4 are single letters
		static constexpr uint32 SingleVowels = 5;
		static constexpr uint32 ShapeCount = 4; ///< CV, CVC, V, VC

		uint32 Onsets = 0;
		uint32 Codas = 0;
		uint32 Vowels = 0;
		uint8 Weight[ShapeCount] = {}; ///< relative weight of each syllable shape
		uint8 MinSyllables = 2;
		uint8 MaxSyllables = 3;
		uint8 Reserved[2] = {};
	};
	static_assert(sizeof(Phonology) == 20, "Phonology must stay padding free");

	/// Component of a language entity (ids of kind Language).
	struct LanguageInfo
	{
		uint32 Index = 0;	   ///< 1-based, in order of founding
		uint32 Culture = 0;	   ///< culture that speaks it
		uint32 Parent = 0;	   ///< language it drifted from (0 for a root language)
		uint32 Generation = 0; ///< number of drifts applied since founding
		uint64 Founded = 0;	   ///< tick
		Hash64 Identity = 0;   ///< derived from the culture's identity
		Phonology Sounds;
		uint32 Names = 0; ///< names given in this language
	};
	static_assert(sizeof(LanguageInfo) == 56, "LanguageInfo must stay padding free");

	/// Fixed-size, NUL-terminated name.
	struct NameText
	{
		static constexpr uint32 Capacity = 24; ///< including the terminator
		char Chars[Capacity] = {};
	};

	enum class NameScope : uint32
	{
		Culture = 0,
		Language,
		Region,
		River,
		Lake,
		Era,
		Person,
		Religion,
		Count
	};

	/// Component on the named entity.
	struct NameInfo
	{
		uint32 Language = 0;   ///< language index the name was built in
		uint32 Scope = 0;	   ///< NameScope
		uint64 Key = 0;		   ///< scope-local key (region index, river index, era index...)
		uint32 Salt = 0;	   ///< retries needed for uniqueness
		uint32 Generation = 0; ///< language generation at naming time
		NameText Text;
	};
	static_assert(sizeof(NameInfo) == 48, "NameInfo must stay padding free");

	struct LanguageTypes
	{
		ComponentType<LanguageInfo> Language;
		ComponentType<NameInfo> Name;
		static LanguageTypes Declare(World& W);
	};

	struct LanguageRules
	{
		uint64 DriftTicks = 8640ull * 150; ///< one sound change per span (150 years)
		uint32 MaxSalt = 256;			   ///< uniqueness retries before giving up
		uint32 NameRivers = 1;
		uint32 NameLakes = 1;
		uint32 NameEras = 1;
	};

	struct LanguagePayload
	{
		uint32 Language = 0;
		uint32 Culture = 0;
		uint32 Parent = 0;
		uint32 Generation = 0;
	};
	struct NamePayload
	{
		uint32 Language = 0;
		uint32 Scope = 0;
		uint64 Key = 0;
	};

	inline constexpr EventType<LanguagePayload> LanguageFoundedEvent =
		MakeEventType<LanguagePayload>("LanguageFounded");
	inline constexpr EventType<LanguagePayload> LanguageDriftedEvent =
		MakeEventType<LanguagePayload>("LanguageDrifted");
	inline constexpr EventType<NamePayload> NamedEvent = MakeEventType<NamePayload>("Named");

	// ── Phonology and name construction (pure functions) ─────────────────────

	/// A phonology drawn from an identity hash; always normalised.
	VAELEN_SIM_API Phonology DerivePhonology(Hash64 Identity) noexcept;
	/// One sound change (an onset, coda or vowel toggled, or a shape weight
	/// moved) chosen by the salt; the result is normalised.
	VAELEN_SIM_API Phonology MutatePhonology(const Phonology& P, Hash64 Salt) noexcept;
	/// Enforces the inventory minimums (enough single-letter onsets, codas
	/// and vowels, at least one shape with weight, 1 <= Min <= Max <= 4).
	VAELEN_SIM_API Phonology NormalisePhonology(Phonology P, Hash64 Filler) noexcept;
	/// True when every minimum of NormalisePhonology holds.
	VAELEN_SIM_API bool IsNormalised(const Phonology& P) noexcept;

	/// Builds the name of (Scope, Key) in a language; Salt picks an alternative.
	/// Pronounceable by construction (IsPronounceable holds).
	VAELEN_SIM_API NameText GenerateName(const Phonology& P, NameScope Scope, uint64 Key, uint32 Salt) noexcept;
	/// Letters only, capital then lower case, 2 to 23 letters, at most three
	/// consonants or two vowels in a row, at least one vowel.
	VAELEN_SIM_API bool IsPronounceable(const NameText& T) noexcept;
	VAELEN_SIM_API uint32 NameLength(const NameText& T) noexcept;
	VAELEN_SIM_API bool NameEquals(const NameText& A, const NameText& B) noexcept;

	// ── The system ───────────────────────────────────────────────────────────

	/// Yearly: founds a language for every culture without one (a child of the
	/// parent culture's language after a split), applies drift, then names
	/// nameless cultures, languages, settled regions, rivers and lakes whose
	/// source region is settled, and eras (in the language of the largest
	/// culture). Names are unique within their scope.
	class VAELEN_SIM_API LanguageSystem final : public ISystem
	{
	public:
		LanguageSystem(World& InWorld, WorldGen::WorldSetup InSetup, PopulationTypes InPopulation,
					   LanguageTypes InTypes, LanguageRules InRules) noexcept
			: Owner(&InWorld), Setup(InSetup), Population(InPopulation), Types(InTypes), Rules(InRules)
		{
		}
		/// Optional: when set, eras are named too.
		void NameEras(ComponentType<EraInfo> InEra) noexcept
		{
			Era = InEra;
			HasEra = true;
		}
		const char* GetName() const noexcept override { return "Languages"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Population"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		WorldGen::WorldSetup Setup;
		PopulationTypes Population;
		LanguageTypes Types;
		LanguageRules Rules;
		ComponentType<EraInfo> Era;
		bool HasEra = false;
	};

	// ── Queries ──────────────────────────────────────────────────────────────

	/// The name on an entity, or null.
	VAELEN_SIM_API const NameInfo* NameOf(const World& W, const LanguageTypes& Types, EntityHandle H) noexcept;
	/// True when a name is already used in a scope.
	VAELEN_SIM_API bool IsNameUsed(const World& W, const LanguageTypes& Types, NameScope Scope,
								   const NameText& Text) noexcept;

	struct NamingStats
	{
		uint32 Languages = 0;
		uint32 Names = 0;
		uint32 PerScope[static_cast<uint32>(NameScope::Count)] = {};
		uint32 MaxGeneration = 0;
		uint32 MaxSalt = 0;
		uint32 Duplicates = 0; ///< names equal to another of the same scope (should be 0)
	};
	VAELEN_SIM_API NamingStats MeasureNames(const World& W, const LanguageTypes& Types);

	/// One line per name of a scope, "key<TAB>name", sorted by key.
	VAELEN_SIM_API void ExportNames(const World& W, const LanguageTypes& Types, NameScope Scope, std::string& Out);
} // namespace Vaelen::History
