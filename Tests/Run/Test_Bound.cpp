// VAELEN - Tests/Run
// Section 27 step 2, the colony question measured: why Play+Colony offers
// nobody bound at 128, and whether the PRODUCT's world does.
//
// The premise of the game is a bound life (StartRules{} asks for one), and
// the 40-second Atlas cell that carries the colony (Play+Stream+Lively+
// Colony, the `full-*` goldens) offers nobody bound at 128. This suite
// builds the two worlds the question is about, from one seed, and reads the
// clause that refuses:
//
//   Aelvor::Begin details ONE region for the pre-play years. Without the
//   colony it is the busiest (Busiest); with it, the busiest that has ORE
//   under it (BusiestWithOre), because a colony is people put on rock. On
//   AELVOR 128 the busiest is region 26 and the busiest with ore is region
//   75, and the two regions belong to different cultures. A culture's
//   customs (NormSet::BondageAllowed, 05.04) say which institutions it
//   allows, each drawn at 500 per mille from the culture's identity; region
//   26's culture allows all three (bits 7) and region 75's allows NONE
//   (bits 0). Every bond entry Bondage.cpp has asks the culture first -
//   debt (`Allows(Culture, Debt)`), birth (`Allows(Culture, Birth)`),
//   capture - and the promotion path binds only what an earlier detail
//   left in the region's strata, which a first detail has none of. So the
//   colony's region binds nobody in 120 years, and a start that WANTS a
//   bound life is offered nobody: not the size, not the age window, not
//   the colony's hands - the ground the colony rule chose, and the customs
//   of the people on it.
//
// AND THE PRODUCT'S WORLD IS NOT THAT CELL: VaelenWorldSubsystem::Begin asks
// for Play and Stream and never the colony, so the game's world details
// region 26, where 119 are bound after 120 years and 45 of them are inside
// the default window. The premise is in the product's world as it is; only
// the host's `WantBound = 0` line hides it, and that line is the owner's.
//
// Two worlds at 128 is the cost Run.Climate already pays; the numbers are
// PINNED (the region, the culture, its bits, the counts, the person) so that
// the day a fix moves the colony or the customs, this suite says so by name.
//
// STATUS: PROTOTYPE (section 27 step 2)
#include "VaelenTest.h"

#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/Sim/Population.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Society/BondState.h"
#include "Vaelen/Society/Bondage.h"
#include "Vaelen/Society/Norms.h"

#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;

namespace
{
	/// What one detailed region holds at the end of the pre-play years, read
	/// the way Start.cpp's TakeUp and Bondage.cpp's entries read it.
	struct Held
	{
		uint32 Majority = 0;	  ///< the region's majority culture (RegionPopulation)
		uint32 BondageBits = 0;	  ///< that culture's NormSet::BondageAllowed
		uint32 Alive = 0;		  ///< living persons of the region
		uint32 InWindow = 0;	  ///< of them, aged StartRules{}.FromAge..ToAge
		uint32 Bound = 0;		  ///< of the living, with a BondState that is not Free
		uint32 BoundInWindow = 0; ///< of the bound, inside the window
		uint32 DebtAllowed = 0;	  ///< living persons whose own culture allows debt bondage
	};

	Held Read(const Aelvor& A, uint32 Region)
	{
		const World& W = A.Instance();
		const Wired& H = A.Handles();
		const uint64 Now = W.Now();
		Held Out;
		std::vector<uint32> Allowed;
		W.Components()
			.GetPool(A.Ages().Population.Culture)
			.ForEach(
				[&](EntityHandle E, const History::CultureInfo& C)
				{
					const Society::NormSet* N = W.Components().GetPool(H.Norms.Norms).TryGet(E);
					if (C.Index >= Allowed.size())
					{
						Allowed.resize(usize{C.Index} + 1u, 0u);
					}
					Allowed[C.Index] = N != nullptr ? N->BondageAllowed : 0u;
				});
		W.Components()
			.GetPool(A.Ages().World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle E, const WorldGen::RegionInfo& R)
				{
					const History::RegionPopulation* P =
						W.Components().GetPool(A.Ages().Population.Population).TryGet(E);
					if (R.Index == Region && P != nullptr)
					{
						Out.Majority = P->Majority;
						Out.BondageBits = P->Majority < Allowed.size() ? Allowed[P->Majority] : 0u;
					}
				});
		const Player::StartRules Window;
		W.Components()
			.GetPool(H.Persons.Person)
			.ForEach(
				[&](EntityHandle E, const Population::PersonInfo& P)
				{
					if (P.Region != Region || P.State != static_cast<uint8>(Population::LifeState::Alive) ||
						P.Born > Now)
					{
						return;
					}
					++Out.Alive;
					const uint32 Age = static_cast<uint32>((Now - P.Born) / History::TicksPerYear);
					const bool In = Age >= Window.FromAge && Age <= Window.ToAge;
					Out.InWindow += In ? 1u : 0u;
					Out.DebtAllowed += P.Culture < Allowed.size() &&
											   (Allowed[P.Culture] & static_cast<uint32>(Society::Bondage::Debt)) != 0
										   ? 1u
										   : 0u;
					const Society::BondState* B = W.Components().GetPool(H.Bondage.Bond).TryGet(E);
					if (B != nullptr && B->Kind != static_cast<uint8>(Society::BondKind::Free))
					{
						++Out.Bound;
						Out.BoundInWindow += In ? 1u : 0u;
					}
				});
		return Out;
	}

	Options ProductOptions()
	{
		Options O;
		O.Size = 128;
		O.Years = 120;
		O.Play = true;
		O.Stream = true;
		return O; // PreHistory 300 and the climate on: the subsystem's Begin, as Run.Soak has it
	}
} // namespace

// THE CELL: Play+Stream+Colony (Lively changes nothing here and is left off,
// so the two worlds of this suite differ by the colony alone - ADR-0149).
VAELEN_TEST(Bound, TheColonysGroundHasACultureThatBindsNobody)
{
	Options O = ProductOptions();
	O.Colony = true;
	Aelvor A(O);
	VT_REQUIRE(A.Begin());
	// The colony rule chose region 75 - the busiest with ore - over region
	// 26, and founded the colony there.
	VT_CHECK_EQ(A.Detail(), 75u);
	VT_CHECK_EQ(A.Founded(), 75u);
	const Held R = Read(A, A.Detail());
	VT_CHECK_EQ(R.Majority, 3u);
	VT_CHECK_EQ(R.BondageBits, 0u); // no debt, no capture, no birth: nothing Bondage.cpp can enter by
	VT_CHECK_EQ(R.DebtAllowed, 0u); // and not one person of another custom lives there
	VT_CHECK_EQ(R.Alive, 1359u);
	VT_CHECK_EQ(R.InWindow, 503u); // the window is not empty: 503 lives the start could take
	VT_CHECK_EQ(R.Bound, 0u);
	VT_CHECK_EQ(R.BoundInWindow, 0u);
	// So a bound start is offered nobody ...
	Player::StartRules WantBound;
	VT_CHECK_EQ(WantBound.WantBound, 1u);
	VT_CHECK_EQ(A.TakeUp(WantBound), 0u);
	VT_CHECK_EQ(A.Played(), 0u);
	// ... and a free one is offered the lowest person index in the window
	// (the CONTROL: the world is not empty, the filter is what refuses).
	Player::StartRules Free;
	Free.WantBound = 0;
	const uint32 Taken = A.TakeUp(Free);
	VT_CHECK_NE(Taken, 0u);
	const Player::PlayerStart* S = Player::StartOf(A.Instance(), A.Handles().Start);
	VT_REQUIRE(S != nullptr);
	VT_CHECK_EQ(S->Region, 75u);
	VT_CHECK_EQ(S->Bond, static_cast<uint32>(Society::BondKind::Free));
}

// THE PRODUCT: Play+Stream, as VaelenWorldSubsystem::Begin asks (no colony).
VAELEN_TEST(Bound, TheProductsWorldOffersABoundLife)
{
	Aelvor A(ProductOptions());
	VT_REQUIRE(A.Begin());
	VT_CHECK_EQ(A.Detail(), 26u);
	VT_CHECK_EQ(A.Founded(), 0u);
	const Held R = Read(A, A.Detail());
	VT_CHECK_EQ(R.Majority, 1u);
	VT_CHECK_EQ(R.BondageBits, 7u); // debt, capture and birth all allowed
	VT_CHECK_EQ(R.Alive, 1428u);
	VT_CHECK_EQ(R.DebtAllowed, 1428u);
	VT_CHECK_EQ(R.InWindow, 522u);
	VT_CHECK_EQ(R.Bound, 119u);
	VT_CHECK_EQ(R.BoundInWindow, 45u);
	// The default rules - a bound life aged 16 to 40 - are offered person
	// 3580, and the record says bound.
	const Player::StartRules WantBound;
	VT_CHECK_EQ(A.TakeUp(WantBound), 3580u);
	const Player::PlayerStart* S = Player::StartOf(A.Instance(), A.Handles().Start);
	VT_REQUIRE(S != nullptr);
	VT_CHECK_EQ(S->Person, 3580u);
	VT_CHECK_EQ(S->Region, 26u);
	VT_CHECK_NE(S->Bond, static_cast<uint32>(Society::BondKind::Free));
	VT_CHECK(S->Age >= WantBound.FromAge && S->Age <= WantBound.ToAge);
}
