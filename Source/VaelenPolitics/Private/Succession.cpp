// VAELEN - VaelenPolitics
// Phase 07.04: succession.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics

#include "Vaelen/Politics/Succession.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Politics
{
	SuccessionTypes SuccessionTypes::Declare(World& W)
	{
		SuccessionTypes T;
		T.Line = W.Types().Register<PolityLine>("PolityLine");
		W.Components().CreatePool(T.Line);
		return T;
	}

	void SuccessionSystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		struct Seat
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 Region = 0;
			uint32 Ruler = 0;
			uint32 Culture = 0;
			bool Standing = false;
		};
		std::vector<Seat> Seats;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach([&](EntityHandle H, const PolityInfo& P)
					 { Seats.push_back(Seat{H, P.Index, P.Seat, P.Ruler, P.Culture, P.Dissolved == 0}); });
		std::sort(Seats.begin(), Seats.end(), [](const Seat& A, const Seat& B) { return A.Index < B.Index; });
		if (Seats.empty())
		{
			return;
		}

		// The rulers whose children matter this year, in person order. There are
		// as many as there are standing polities, so the pass below is one walk
		// of the persons, not one per polity.
		std::vector<uint32> Watched;
		for (const Seat& S : Seats)
		{
			if (S.Standing && S.Ruler != 0)
			{
				Watched.push_back(S.Ruler);
			}
		}
		std::sort(Watched.begin(), Watched.end());
		Watched.erase(std::unique(Watched.begin(), Watched.end()), Watched.end());

		// For every watched ruler, the eldest living child of age on each line of
		// descent: born earliest wins, and the lower person index breaks a tie so
		// that twins never make the world depend on pool order.
		struct Child
		{
			uint32 Person = 0;
			uint64 Born = 0;
		};
		std::vector<Child> ByFather(Watched.size());
		std::vector<Child> ByMother(Watched.size());
		const uint64 Now = Context.Tick;
		const uint64 OfAge = uint64{Rules.HeirFromAge} * History::TicksPerYear;
		W.Components()
			.GetPool(Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& P)
				{
					if (P.State != static_cast<uint8>(Population::LifeState::Alive) || P.Born + OfAge > Now)
					{
						return;
					}
					auto Better = [&](Child& Slot)
					{
						if (Slot.Person == 0 || P.Born < Slot.Born || (P.Born == Slot.Born && P.Index < Slot.Person))
						{
							Slot.Person = P.Index;
							Slot.Born = P.Born;
						}
					};
					const auto Father = std::lower_bound(Watched.begin(), Watched.end(), P.Father);
					if (P.Father != 0 && Father != Watched.end() && *Father == P.Father)
					{
						Better(ByFather[static_cast<usize>(Father - Watched.begin())]);
					}
					const auto Mother = std::lower_bound(Watched.begin(), Watched.end(), P.Mother);
					if (P.Mother != 0 && Mother != Watched.end() && *Mother == P.Mother)
					{
						Better(ByMother[static_cast<usize>(Mother - Watched.begin())]);
					}
				});

		for (const Seat& S : Seats)
		{
			PolityLine* Line = W.Components().GetPool(Lines.Line).TryGet(S.Handle);
			if (Line == nullptr)
			{
				if (!S.Standing)
				{
					continue; // a polity that ended before this system ever saw it has no line
				}
				PolityLine Fresh;
				Fresh.Polity = S.Index;
				Fresh.Sitting = S.Ruler;
				Fresh.Rulers = S.Ruler != 0 ? 1u : 0u;
				Fresh.Seated = S.Ruler != 0 ? Context.Tick : 0u;
				Fresh.Vacant = S.Ruler != 0 ? 0u : Context.Tick;
				W.Components().GetPool(Lines.Line).Add(S.Handle, Fresh);
				Line = W.Components().GetPool(Lines.Line).TryGet(S.Handle);
				if (Line == nullptr)
				{
					continue;
				}
			}
			if (!S.Standing)
			{
				// A polity that is gone has no seat to pass and no unrest to bear;
				// the line stays as the record of who sat in it.
				Line->Unrest = 0;
				Line->Claimant = 0;
				Line->Sitting = 0;
				continue;
			}

			// 1. The seat, compared against what this system last saw in it.
			if (S.Ruler != Line->Sitting)
			{
				if (S.Ruler == 0)
				{
					++Line->Interregna;
					Line->Vacant = Context.Tick;
					Line->Unrest += Rules.UnrestOnVacancy;
					Context.Events->Publish(Context.Tick, SeatFellVacantEvent,
											PolityPayload{S.Index, S.Region, Line->Sitting, 0},
											W.Entities().GetId(S.Handle));
				}
				else
				{
					++Line->Rulers;
					Line->Seated = Context.Tick;
					Line->Vacant = 0;
					const uint32 Named = Line->Claimant;
					if (Named != 0 && Named != S.Ruler)
					{
						++Line->Disputed;
						Line->Unrest += Rules.UnrestOnDisputed;
						Context.Events->Publish(Context.Tick, SuccessionDisputedEvent,
												PolityPayload{S.Index, S.Region, S.Ruler, Named},
												W.Entities().GetId(S.Handle));
					}
					else
					{
						Context.Events->Publish(Context.Tick, SuccessionSettledEvent,
												PolityPayload{S.Index, S.Region, S.Ruler, 1},
												W.Entities().GetId(S.Handle));
					}
				}
				Line->Sitting = S.Ruler;
			}

			// 2. The unrest fades, and never stands higher than the ceiling.
			Line->Unrest = std::min(Line->Unrest, Rules.UnrestCeiling);
			Line->Unrest = Line->Unrest > Rules.UnrestFadePerYear ? Line->Unrest - Rules.UnrestFadePerYear : 0u;

			// 3. Whom the custom names for the seat next: the eldest living child
			//    of age of whoever sits, on the culture's line of descent.
			Line->Claimant = 0;
			if (S.Ruler == 0)
			{
				continue;
			}
			const auto At = std::lower_bound(Watched.begin(), Watched.end(), S.Ruler);
			if (At == Watched.end() || *At != S.Ruler)
			{
				continue;
			}
			const usize Slot = static_cast<usize>(At - Watched.begin());
			const Society::NormSet* Custom = Society::NormsOf(W, Types, Norms, S.Culture);
			const bool Father =
				Custom == nullptr || Custom->Descent_ == static_cast<uint32>(Society::Descent::Patrilineal);
			Line->Claimant = Father ? ByFather[Slot].Person : ByMother[Slot].Person;
		}
	}

	const PolityLine* LineOf(const World& W, const PolityTypes& Polities, const SuccessionTypes& Lines, uint32 Polity)
	{
		const PolityLine* Found = nullptr;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle H, const PolityInfo& P)
				{
					if (P.Index == Polity && Found == nullptr)
					{
						Found = W.Components().GetPool(Lines.Line).TryGet(H);
					}
				});
		return Found;
	}

	SuccessionStats MeasureSuccession(const World& W, const PolityTypes& Polities,
									  const Population::PersonTypes& Persons, const SuccessionTypes& Lines,
									  const SuccessionRules& Rules)
	{
		SuccessionStats S;
		struct Row
		{
			uint32 Index = 0;
			PolityLine Line;
			uint32 Ruler = 0;
			bool Standing = false;
		};
		std::vector<Row> Rows;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle H, const PolityInfo& P)
				{
					const PolityLine* L = W.Components().GetPool(Lines.Line).TryGet(H);
					if (L != nullptr)
					{
						Rows.push_back(Row{P.Index, *L, P.Ruler, P.Dissolved == 0});
					}
				});
		std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.Index < B.Index; });

		Hash64 D = HashString("Succession");
		for (const Row& R : Rows)
		{
			++S.Lines;
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R.Line), sizeof(R.Line)));
			S.Rulers += R.Line.Rulers;
			S.Interregna += R.Line.Interregna;
			S.Disputes += R.Line.Disputed;
			S.Bad += R.Line.Unrest > Rules.UnrestCeiling ? 1u : 0u;
			if (!R.Standing)
			{
				S.Bad += R.Line.Unrest != 0 || R.Line.Sitting != 0 ? 1u : 0u;
				continue;
			}
			// The line must say what the polity says, or it is remembering a
			// world that is not this one.
			S.Bad += R.Line.Sitting != R.Ruler ? 1u : 0u;
			S.Empty += R.Ruler == 0 ? 1u : 0u;
			S.Troubled += R.Line.Unrest != 0 ? 1u : 0u;
			S.Unrest = std::max(S.Unrest, R.Line.Unrest);
			if (R.Line.Claimant != 0)
			{
				const Population::PersonInfo* Heir = Population::FindPerson(W, Persons, R.Line.Claimant);
				S.Bad += Heir == nullptr || Heir->State != static_cast<uint8>(Population::LifeState::Alive) ? 1u : 0u;
			}
		}
		for (const Event& E : W.Log().All())
		{
			S.Vacancies += E.Is(SeatFellVacantEvent) ? 1u : 0u;
			S.Settled += E.Is(SuccessionSettledEvent) ? 1u : 0u;
			S.Contested += E.Is(SuccessionDisputedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Politics
