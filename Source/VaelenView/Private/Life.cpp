// VAELEN - VaelenView
// Phase 14 task 14.04: one played life, as the screen that shows it needs it.
//
// STATUS: PROTOTYPE (Phase 14) - unit/integration/deterministic tests in Tests/View/Test_Life.cpp
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Take.h"

#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Hours.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/Regard.h"
#include "Vaelen/Player/Start.h"
#include "Vaelen/Population/Families.h"
#include "Vaelen/Population/Needs.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Population/Persons.h"
#include "Vaelen/Sim/Climate.h"
#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Vaelen::View
{
	// The one place allowed to see both the view's constants and the kernel's.
	static_assert(MostWaiting == Player::MostOrders, "View::MostWaiting must match Player::MostOrders");
	static_assert(MostKnownOf == Player::MostKnown, "View::MostKnownOf must match Player::MostKnown");
	static_assert(sizeof(Player::OrderRules::HoursOf) == IntentSlots * sizeof(uint32),
				  "View::IntentSlots must match OrderRules::HoursOf");

	namespace
	{
		/// Twenty-four ticks a day (CalendarRules' default), 360 days a year.
		constexpr uint64 TicksPerDay = 24;
		static_assert(History::TicksPerYear == 360 * TicksPerDay, "a year is 360 days of 24 ticks");

		/// Copies a name into its fixed bytes, always terminated, never past
		/// the end. What does not fit is cut; nothing here is longer than 24.
		void Put(char (&To)[LifeNameBytes], const std::string& From)
		{
			usize n = 0;
			for (; n < From.size() && n + 1 < LifeNameBytes; ++n)
			{
				To[n] = From[n];
			}
			To[n] = '\0';
		}

		uint32 AgeAt(const Population::PersonInfo& P, uint64 Tick)
		{
			const uint64 Until = P.Died != 0 && P.Died < Tick ? P.Died : Tick;
			return Until > P.Born ? static_cast<uint32>((Until - P.Born) / History::TicksPerYear) : 0u;
		}
	} // namespace

	void TakeLifeView(const World& W, const ViewSources& From, WorldGen::RegionGraphCache& Ways, LifeView& Out)
	{
		Out = LifeView{};
		Out.Tick = static_cast<uint64>(W.Now());
		Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		Out.Day = static_cast<uint32>((Out.Tick % History::TicksPerYear) / TicksPerDay);
		// The verbs cost what the rules say, played or not: the panel lists
		// them either way. The Run plays under the default rules (Aelvor.cpp),
		// and rules are the host's configuration, not world state.
		const Player::OrderRules Rules;
		for (usize k = 0; k < IntentSlots; ++k)
		{
			Out.Cost[k] = Rules.HoursOf[k];
		}
		// 18.04: the season is the world's, played or not; the degrees are
		// where the played person stands, below.
		if (From.HasClimate)
		{
			Out.Season = 1u + W.Clock().Date().Season;
		}
		if (!From.HasPlayer || !From.HasLife)
		{
			return;
		}
		const uint32 Person = Player::PlayerPerson(W, From.Played);
		if (Person == 0)
		{
			return;
		}
		Out.Person = Person;

		// One walk of the person pool for every name below, not one per name.
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, From.Persons);
		const EntityHandle Self = Person < Index.Handles.size() ? Index.Handles[Person] : EntityHandle{};
		const Population::PersonInfo* P =
			Self.IsNull() ? nullptr : W.Components().GetPool(From.Persons.Person).TryGet(Self);
		std::string Text;
		if (P != nullptr)
		{
			Out.Region = P->Region;
			Out.Alive = P->State == static_cast<uint8>(Population::LifeState::Alive) ? 1u : 0u;
			Out.Years = AgeAt(*P, Out.Tick);
			Population::NamePerson(W, From.Types, From.Persons, Person, Text, &Index);
			Put(Out.Name, Text);
			History::NameRegion(W, From.Types, P->Region, Text);
			Put(Out.RegionName, Text);
			const Population::PersonNeeds* N = W.Components().GetPool(From.Needs.Needs).TryGet(Self);
			if (N != nullptr)
			{
				Out.Food = N->Food;
				Out.Health = N->Health;
				Out.Rest = N->Rest;
				Out.Hungry = N->Hungry;
			}
			// 18.05: the chill the person carries, where the world declared one.
			if (From.HasWarmth)
			{
				if (const Population::PersonWarmth* C = W.Components().GetPool(From.Warmth.Warmth).TryGet(Self))
				{
					Out.Chill = C->Chill;
				}
			}
			// 18.04: today's temperature at the region's centroid, in tenths,
			// and this year's winter there.
			if (From.HasClimate && P->Region != 0)
			{
				const uint32 Centroid = WorldGen::RegionCentroidTile(W, From.Types.World, P->Region);
				const Fix64 Now = WorldGen::TileTemperatureOn(W.Map(), From.Types.World.Layers, Centroid, Out.Day);
				Out.Degrees = (Now * 10).FloorToInt();
				Out.Winter = WorldGen::WinterSeverity(
					WorldGen::RegionYear(W, From.Types.World, P->Region, Out.Year, From.Climate), From.Climate);
			}
		}

		if (const Player::PlayerStart* S = Player::StartOf(W, From.Start))
		{
			Out.StartRegion = S->Region;
			Out.Holder = S->Holder;
			Out.StartYear = static_cast<uint32>(S->Began / History::TicksPerYear);
			Out.Bond = S->Bond;
			if (S->Holder != 0)
			{
				Text.clear();
				Population::NamePerson(W, From.Types, From.Persons, S->Holder, Text, &Index);
				Put(Out.HolderName, Text);
			}
		}

		if (const Player::PlayerHours* H = Player::HoursOf(W, From.Hour))
		{
			Out.Awake = H->Awake;
			Out.Spent = H->Spent;
			Out.DaysLived = H->Days;
			Out.Missed = H->Missed;
		}
		Out.Left = Player::HoursLeft(W, From.Hour);

		if (const Player::PlayerOrders* Q = Player::OrdersOf(W, From.Order))
		{
			Out.Held = Q->Held;
			Out.Taken = Q->Taken;
			Out.Refused = Q->Refused;
			Out.Dropped = Q->Dropped;
			Out.LastRefusal = Q->Last;
			const uint32 Waiting = Q->Held < MostWaiting ? Q->Held : MostWaiting;
			for (uint32 i = 0; i < Waiting; ++i)
			{
				Out.Waiting[i] = Q->Ring[(Q->First + i) % Player::MostOrders];
			}
		}

		if (const Player::PlayerRegard* R = Player::RegardOf(W, From.Regard))
		{
			Out.Repute = Player::ReputeOf(W, From.Regard);
			Out.KnownCount = R->Known;
			Out.Kindnesses = R->Kindnesses;
			Out.Wrongs = R->Wrongs;
			const uint32 Listed = R->Known < MostKnownOf ? R->Known : MostKnownOf;
			for (uint32 i = 0; i < Listed; ++i)
			{
				Out.Known[i].Person = R->Who[i].Person;
				Out.Known[i].Regard = R->Who[i].Regard;
				Out.Known[i].Met = R->Who[i].Met;
				Text.clear();
				Population::NamePerson(W, From.Types, From.Persons, R->Who[i].Person, Text, &Index);
				Put(Out.Known[i].Name, Text);
			}
		}

		if (P == nullptr || P->Region == 0)
		{
			return;
		}

		// Where a Move will not be refused TooFar (Doings.cpp, Intent::Move):
		// a neighbour of this region that the world simulates person by
		// person. The graph is the caller's cache, as for TakeViewFor.
		const WorldGen::RegionGraph& Graph = Ways.Of(W.Map(), From.Types.World.Regions);
		if (P->Region < Graph.Neighbours.size())
		{
			for (const uint16 N : Graph.Neighbours[P->Region])
			{
				if (Out.NearCount < MostNear && Population::IsDetailed(W, From.Types, From.Persons, N))
				{
					Out.Near[Out.NearCount++] = N;
				}
			}
		}

		// Who a Speak, a Give or a Take can be aimed at: alive, here, and not
		// the played person. Index order, the lowest first, so the list is a
		// function of the world and not of pool order.
		std::vector<uint32> Here;
		W.Components()
			.GetPool(From.Persons.Person)
			.ForEach(
				[&](EntityHandle, const Population::PersonInfo& Other)
				{
					if (Other.Region == P->Region && Other.Index != Person &&
						Other.State == static_cast<uint8>(Population::LifeState::Alive))
					{
						Here.push_back(Other.Index);
					}
				});
		std::sort(Here.begin(), Here.end());
		Out.CompanyThere = static_cast<uint32>(Here.size());
		Out.CompanyCount = Here.size() < MostCompany ? static_cast<uint32>(Here.size()) : MostCompany;
		for (uint32 i = 0; i < Out.CompanyCount; ++i)
		{
			const uint32 Who = Here[i];
			const EntityHandle H = Who < Index.Handles.size() ? Index.Handles[Who] : EntityHandle{};
			const Population::PersonInfo* O =
				H.IsNull() ? nullptr : W.Components().GetPool(From.Persons.Person).TryGet(H);
			Out.Company[i].Person = Who;
			if (O != nullptr)
			{
				Out.Company[i].Years = AgeAt(*O, Out.Tick);
				Out.Company[i].Sex = O->Sex;
			}
			Text.clear();
			Population::NamePerson(W, From.Types, From.Persons, Who, Text, &Index);
			Put(Out.Company[i].Name, Text);
		}
	}

	LifeStats MeasureLifeView(const LifeView& V)
	{
		LifeStats Out;
		Out.Bytes = static_cast<uint32>(sizeof(LifeView));
		Out.Named += V.Name[0] != '\0' ? 1u : 0u;
		Out.Named += V.RegionName[0] != '\0' ? 1u : 0u;
		Out.Named += V.HolderName[0] != '\0' ? 1u : 0u;
		for (const KnownView& K : V.Known)
		{
			Out.Named += K.Name[0] != '\0' ? 1u : 0u;
		}
		for (const CompanyView& C : V.Company)
		{
			Out.Named += C.Name[0] != '\0' ? 1u : 0u;
		}
		Out.Listed = V.NearCount + (V.KnownCount < MostKnownOf ? V.KnownCount : MostKnownOf) + V.CompanyCount;
		Out.Digest = HashBytes(reinterpret_cast<const char*>(&V), sizeof(LifeView));
		return Out;
	}
} // namespace Vaelen::View
