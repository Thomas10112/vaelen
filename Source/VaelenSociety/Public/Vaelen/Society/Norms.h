// VAELEN - VaelenSociety
// Phase 05.03: norms - the customs of a culture that parametrise how its
// persons marry, descend, tolerate other faiths, move and hold others.
//
// STATUS: VALIDATED (Phase 05) - unit/deterministic tests in Tests/Society
//
// Every culture carries a NormSet drawn once from its identity (a split
// culture takes its parent's with one custom nudged) and drifted by what
// happens to it: a schism hardens the faith, a great disaster loosens the
// people. The marriage customs are mirrored into the MarriageNorms the
// family system observes, so persons of different cultures marry by
// different rules; descent, tolerance, mobility and the bondage allowed are
// read by the later tasks of the phase.
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/SocietyApi.h"

#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Society
{
	enum class Descent : uint32
	{
		Patrilineal = 0,
		Matrilineal = 1,
	};

	/// Bondage institutions a culture allows (05.04), as bits.
	enum class Bondage : uint32
	{
		Debt = 1,
		Capture = 2,
		Birth = 4,
	};

	/// Component on a culture entity.
	struct NormSet
	{
		Population::MarriageNorms Marriage;
		uint32 Descent_ = 0;			   ///< Descent
		uint32 FaithTolerancePerMille = 0; ///< how far another faith is borne
		uint32 MobilityPerMille = 0;	   ///< how readily the people move
		uint32 BondageAllowed = 0;		   ///< Bondage bits
		uint32 Drifts = 0;				   ///< customs changed by events
		uint32 Reserved = 0;
		uint64 Since = 0; ///< tick of the last change
	};
	static_assert(sizeof(NormSet) == 56, "NormSet must stay padding free");

	struct NormTypes
	{
		ComponentType<NormSet> Norms;
		ComponentType<Population::MarriageNorms> Marriage; ///< the mirror the family system observes
		static NormTypes Declare(World& W);
	};

	struct NormRules
	{
		uint32 MarryFromMin = 16, MarryFromMax = 22;
		uint32 MarryToMin = 40, MarryToMax = 55;
		uint32 GapMin = 8, GapMax = 20;
		uint32 FaithMattersPerMille = 700;
		uint32 MarriagesMin = 250, MarriagesMax = 450;
		uint32 MatrilinealPerMille = 200;
		uint32 ToleranceMin = 100, ToleranceMax = 600;
		uint32 MobilityMin = 100, MobilityMax = 500;
		uint32 BondageBitPerMille = 500;  ///< each institution allowed with this chance
		uint32 SchismToleranceLoss = 100; ///< a schism in the culture's region: faith matters, tolerance falls
		uint32 DisasterMobilityGain = 50; ///< a disaster of DriftSeverity or worse: mobility rises
		uint32 DriftSeverity = 2;
	};

	enum class NormField : uint32
	{
		MarryFrom = 0,
		MarryTo,
		MaxAgeGap,
		FaithMatters,
		Marriages,
		Descent,
		Tolerance,
		Mobility,
		Bondage,
		Count
	};
	VAELEN_SOCIETY_API const char* NormFieldName(NormField F) noexcept;

	struct NormPayload
	{
		uint32 Culture = 0;
		uint32 Field = 0; ///< NormField
		uint32 Before = 0;
		uint32 After = 0;
	};
	inline constexpr EventType<NormPayload> NormChangedEvent = MakeEventType<NormPayload>("NormChanged");

	/// The customs of a root culture, from its identity (pure).
	VAELEN_SOCIETY_API NormSet NormsFromIdentity(Hash64 Identity, const NormRules& Rules) noexcept;
	/// The customs of a split culture: the parent's, one nudged by the identity (pure).
	VAELEN_SOCIETY_API NormSet NormsOfChild(const NormSet& Parent, Hash64 Identity, const NormRules& Rules) noexcept;
	/// Reads one field.
	VAELEN_SOCIETY_API uint32 NormValue(const NormSet& N, NormField F) noexcept;

	/// Yearly: customs for every culture that lacks them, drifts from last
	/// year's schisms and disasters, the marriage mirror kept equal.
	class VAELEN_SOCIETY_API NormSystem final : public ISystem
	{
	public:
		NormSystem(World& InWorld, const History::PreHistoryTypes& InTypes, NormTypes InNorms,
				   NormRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Norms(InNorms), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Norms"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override { return {"Population"}; }
		void Tick(TickContext& Context) override;

	private:
		World* Owner;
		History::PreHistoryTypes Types;
		NormTypes Norms;
		NormRules Rules;
	};

	/// The customs of a culture (nullptr when none yet).
	VAELEN_SOCIETY_API const NormSet* NormsOf(const World& W, const History::PreHistoryTypes& Types,
											  const NormTypes& Norms, uint32 Culture);
	/// Writes a culture's customs (and the mirror); false when the culture is unknown.
	VAELEN_SOCIETY_API bool SetNorms(World& W, const History::PreHistoryTypes& Types, const NormTypes& Norms,
									 uint32 Culture, const NormSet& Value);

	struct NormStats
	{
		uint32 Cultures = 0;
		uint32 WithNorms = 0;
		uint32 MirrorMismatch = 0; ///< cultures whose MarriageNorms differ from their NormSet
		uint32 Matrilineal = 0;
		uint32 FaithMatters = 0;
		uint32 Drifts = 0;
		Hash64 Digest = 0; ///< every NormSet in culture order
	};
	VAELEN_SOCIETY_API NormStats MeasureNorms(const World& W, const History::PreHistoryTypes& Types,
											  const NormTypes& Norms);
} // namespace Vaelen::Society
