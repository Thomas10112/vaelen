// VAELEN - VaelenGameplay
// Phase 12 task 12.06: what the world does about a name.
//
// Every layer under this one has produced a reputation that costs nobody
// anything. 12.02 gave people opinions, 12.03 wrote them down, 12.05 sent them
// down the roads - and at the end of it a man with the worst name in the world
// eats the same dinner as a man with the best. A repute that changes no life is
// a number in a component, and this task is where it stops being one.
//
// What acts, and why these and not others. The bodies that already act on an
// INDIVIDUAL in this project are few: 05.04 binds and frees, 05.01 seats and
// unseats. 07.02's dues are laid on a REGION and not on a person, so a polity
// cannot charge a thief more without inventing an axis this world does not
// have - it is left alone deliberately. So the consequence is bondage: the
// worst-named of the people a place still speaks of are bound to that place,
// and the best-named among the bound are let go.
//
// The one thing worth reading twice: this judges on what the PLACE says
// (12.05's RegionNames), never on the person's own record. A place carries
// eight names. Somebody whose name is not among them is not judged at all -
// they are a nobody, and nobodies are left alone. That is a real consequence of
// 12.05's shape rather than a rule invented here, and it is what makes a
// reputation worth having or worth hiding.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/deterministic tests in Tests/Gameplay/Test_Judgement.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Gameplay/Fame.h"
#include "Vaelen/Gameplay/GameplayApi.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/ComponentType.h"
#include "Vaelen/Sim/Event.h"
#include "Vaelen/Sim/System.h"
#include "Vaelen/Society/Bondage.h"

#include <string>
#include <vector>

namespace Vaelen
{
	class World;
}

namespace Vaelen::Gameplay
{
	struct JudgementRules
	{
		/// A name this bad, where the person stands, is answered for.
		int32 BindUnder = -150;
		/// And a name this good buys a bound person out.
		int32 FreeOver = 400;
		/// Nobody is judged before this age. The same line 12.01 uses to decide
		/// who acts for themselves: a world that binds children for a bad name
		/// is a different world and would need saying so.
		uint32 FromAge = 12;
		/// People bound and people freed in one place in one year, so that a bad
		/// year cannot empty a region into bondage in a single pass.
		uint32 MostPerYear = 4;
	};

	/// Somebody answering for their name (Person, the Region that did it, what
	/// was said of them there, and how many roads it had crossed to get there).
	inline constexpr EventType<FamePayload> CondemnedEvent = MakeEventType<FamePayload>("Condemned");
	/// And somebody let go for it.
	inline constexpr EventType<FamePayload> PardonedEvent = MakeEventType<FamePayload>("Pardoned");

	/// Yearly, after Fame and after Bondage: every place that carries names acts
	/// on the ones it carries, on the people standing in it.
	///
	/// After Bondage on purpose. 05.04 runs its own entries and exits first at
	/// its own rates, and this adds the ones somebody DECIDED on top - so a
	/// world with this system has 05.04's bondage plus judgement, and never
	/// 05.04's bondage replaced by it.
	class VAELEN_GAMEPLAY_API JudgementSystem final : public ISystem
	{
	public:
		JudgementSystem(World& InWorld, const History::PreHistoryTypes& InTypes, Population::PersonTypes InPersons,
						Society::BondageTypes InBondage, FameTypes InFame, JudgementRules InRules) noexcept
			: Owner(&InWorld), Types(InTypes), Persons(InPersons), Bondage(InBondage), Fame_(InFame), Rules(InRules)
		{
		}
		const char* GetName() const noexcept override { return "Judgement"; }
		SimLod GetLod() const noexcept override { return SimLod::World; }
		std::vector<std::string_view> GetDependencies() const override
		{
			std::vector<std::string_view> Out;
			for (const std::string& Name : After)
			{
				Out.push_back(Name);
			}
			return Out;
		}
		void RunAfter(std::string_view Name) { After.emplace_back(Name); }
		void Tick(TickContext& Context) override;

	private:
		std::vector<std::string> After;
		World* Owner;
		History::PreHistoryTypes Types;
		Population::PersonTypes Persons;
		Society::BondageTypes Bondage;
		FameTypes Fame_;
		JudgementRules Rules;
	};

	struct JudgementStats
	{
		uint32 Condemned = 0; ///< from the log, over the world's life
		uint32 Pardoned = 0;
		uint32 BoundNow = 0;  ///< people bound by judgement and still bound
		int32 WorstBound = 0; ///< what was said of the worst-named of them
		Hash64 Digest = 0;
	};
	VAELEN_GAMEPLAY_API JudgementStats MeasureJudgement(const World& W, const Population::PersonTypes& Persons,
														const Society::BondageTypes& Bondage);
} // namespace Vaelen::Gameplay
