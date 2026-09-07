// VAELEN - VaelenEconomy
// Phase 06.07: the economy in the chronicle.
//
// STATUS: VALIDATED (Phase 06) - integration/text/deterministic tests in Tests/Economy

#include "Vaelen/Economy/EconomyHistory.h"

#include "Vaelen/Sim/HistoryText.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>

namespace Vaelen::Economy
{
	namespace
	{
		void Append(std::string& Out, const char* Text)
		{
			Out += Text;
		}

		void AppendNumber(std::string& Out, uint64 Value)
		{
			char Buffer[24];
			std::snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
			Out += Buffer;
		}

		void Capitalise(std::string& Out, const std::string& Text)
		{
			if (Text.empty())
			{
				return;
			}
			const char First = Text[0];
			Out += static_cast<char>(First >= 'a' && First <= 'z' ? First - ('a' - 'A') : First);
			Out.append(Text, 1, std::string::npos);
		}

		bool IsEconomyEvent(const Event& E)
		{
			return E.Is(RouteOpenedEvent) || E.Is(RouteClosedEvent) || E.Is(GoodsCarriedEvent) ||
				   E.Is(SettlementFoundedEvent) || E.Is(SettlementAbandonedEvent) || E.Is(PriceChangedEvent) ||
				   E.Is(ShortfallEvent) || E.Is(HarvestEvent) || E.Is(FortuneChangedEvent) || E.Is(HeirNamedEvent) ||
				   E.Is(StockInheritedEvent) || E.Is(StockReturnedEvent) || E.Is(StockSplitEvent) ||
				   E.Is(StockFoldedEvent) || E.Is(StockEndowedEvent) || E.Is(StockAddedEvent) || E.Is(StockTakenEvent);
		}

		EconomyChronicleState* FindState(World& W, const EconomyChronicleTypes& Types)
		{
			EconomyChronicleState* Found = nullptr;
			W.Components()
				.GetPool(Types.State)
				.ForEach(
					[&](EntityHandle, EconomyChronicleState& S)
					{
						if (Found == nullptr)
						{
							Found = &S;
						}
					});
			return Found;
		}

		/// The price at a market's floor and ceiling for a good, from the rules.
		void Bounds(const MarketRules& Rules, uint32 Good, uint32& Floor, uint32& Ceiling)
		{
			const uint32 Base = Good < GoodCount ? Rules.BasePrice[Good] : 0u;
			Floor = std::max<uint32>(1u, Base * Rules.FloorPerMille / 1000u);
			Ceiling = std::max(Floor, Base * Rules.CeilingPerMille / 1000u);
		}

		void AppendRegion(const World& W, const History::PreHistoryTypes& Types, uint32 Region, std::string& Out)
		{
			std::string Name;
			History::NameRegion(W, Types, Region, Name);
			Out += Name;
		}
	} // namespace

	EconomyChronicleTypes EconomyChronicleTypes::Declare(World& W)
	{
		EconomyChronicleTypes T;
		T.State = W.Types().Register<EconomyChronicleState>("EconomyChronicleState");
		W.Components().CreatePool(T.State);
		return T;
	}

	void EconomyChronicle::Attach()
	{
		EventBus& Bus = Owner->Events();
		Bus.Subscribe(RouteOpenedEvent.TypeHash, this);
		Bus.Subscribe(RouteClosedEvent.TypeHash, this);
		Bus.Subscribe(SettlementFoundedEvent.TypeHash, this);
		Bus.Subscribe(SettlementAbandonedEvent.TypeHash, this);
		Bus.Subscribe(PriceChangedEvent.TypeHash, this);
		Bus.Subscribe(ShortfallEvent.TypeHash, this);
		Bus.Subscribe(FortuneChangedEvent.TypeHash, this);
		Bus.Subscribe(StockInheritedEvent.TypeHash, this);
	}

	bool EconomyChronicle::Matters(const Event& E, uint32& Region) const
	{
		if (E.Is(RouteOpenedEvent) || E.Is(RouteClosedEvent))
		{
			// A road is history the first time it is built and the day it is
			// abandoned after having carried something; the churn of a road
			// reopened on a passing price gap is not.
			const TradePayload P = E.Get<TradePayload>();
			Region = P.From;
			if (Rules.RecordRoutes == 0)
			{
				return false;
			}
			const RouteInfo* R = RouteBetween(*Owner, Context.Trade, P.From, P.To);
			if (R == nullptr)
			{
				return false;
			}
			return E.Is(RouteOpenedEvent) ? R->Openings <= 1u : R->Carried >= Rules.RoadTraffic;
		}
		if (E.Is(SettlementFoundedEvent) || E.Is(SettlementAbandonedEvent))
		{
			Region = E.Get<TradePayload>().From;
			return Rules.RecordSettlements != 0;
		}
		if (E.Is(PriceChangedEvent))
		{
			// Only a price that reached its bound: a market that merely moves is
			// not history, and a century of a world would drown the chronicle.
			const StockPayload P = E.Get<StockPayload>();
			Region = P.Region;
			uint32 Floor = 0;
			uint32 Ceiling = 0;
			Bounds(Context.Prices, P.Good, Floor, Ceiling);
			return Rules.RecordExtremePrices != 0 && (P.Amount <= Floor || P.Amount >= Ceiling);
		}
		if (E.Is(ShortfallEvent))
		{
			const StockPayload P = E.Get<StockPayload>();
			Region = P.Region;
			return Rules.RecordShortfalls != 0 && P.Amount >= Rules.ShortfallFloor;
		}
		if (E.Is(FortuneChangedEvent))
		{
			const WealthPayload P = E.Get<WealthPayload>();
			Region = P.Region;
			const uint32 Gap = P.Value > P.Other ? P.Value - P.Other : P.Other - P.Value;
			return Rules.RecordFortunes != 0 && Gap >= Rules.FortuneSwing;
		}
		if (E.Is(StockInheritedEvent))
		{
			Region = E.Get<StockPayload>().Region;
			return Rules.RecordInheritances != 0;
		}
		return false;
	}

	void EconomyChronicle::OnEvent(const Event& E)
	{
		World& W = *Owner;
		EconomyChronicleState* S = FindState(W, State);
		if (S == nullptr)
		{
			const EntityHandle H = W.CreateEntity(IdKind::Entity);
			W.Components().GetPool(State.State).Add(H, EconomyChronicleState{});
			S = FindState(W, State);
			if (S == nullptr)
			{
				return;
			}
		}
		uint32 Region = 0;
		if (!Matters(E, Region))
		{
			return;
		}
		const uint32 Year = static_cast<uint32>(E.Tick / History::TicksPerYear);
		if (S->Year != Year || S->Region != Region)
		{
			S->Year = Year;
			S->Region = Region;
			S->InYear = 0;
		}
		if (Rules.MaxRecordsPerYear != 0 && S->InYear >= Rules.MaxRecordsPerYear)
		{
			++S->Dropped;
			return;
		}
		++S->InYear;
		++S->Records;
		History::RecordInfo R;
		R.Event = E.Id.Value;
		R.Tick = E.Tick;
		R.Type = E.TypeHash;
		R.Subject = E.Subject.Value;
		R.Era = History::EraAt(W, Types.History, E.Tick);
		R.Region = Region;
		const EntityHandle H = W.CreateEntity(IdKind::Document);
		W.Components().GetPool(Types.History.Record).Add(H, R);
		History::HistoryState* HS = nullptr;
		W.Components()
			.GetPool(Types.History.State)
			.ForEach(
				[&](EntityHandle, History::HistoryState& St)
				{
					if (HS == nullptr)
					{
						HS = &St;
					}
				});
		if (HS != nullptr)
		{
			++HS->RecordCount;
		}
	}

	void NameRoute(const World& W, const History::PreHistoryTypes& Types, const EconomyContext& Context, uint32 Route,
				   std::string& Out)
	{
		const RouteInfo* Found = nullptr;
		W.Components()
			.GetPool(Context.Trade.Route)
			.ForEach(
				[&](EntityHandle H, const RouteInfo& R)
				{
					if (R.Index == Route && Found == nullptr)
					{
						Found = W.Components().GetPool(Context.Trade.Route).TryGet(H);
					}
				});
		if (Found == nullptr)
		{
			Append(Out, "road ");
			AppendNumber(Out, Route);
			return;
		}
		Append(Out, "the road from ");
		AppendRegion(W, Types, Found->From, Out);
		Append(Out, " to ");
		AppendRegion(W, Types, Found->To, Out);
	}

	void NameSettlement(const World& W, const History::PreHistoryTypes& Types, const EconomyContext& Context,
						uint32 Settlement, std::string& Out)
	{
		const SettlementInfo* Found = nullptr;
		W.Components()
			.GetPool(Context.Trade.Settlement)
			.ForEach(
				[&](EntityHandle H, const SettlementInfo& S)
				{
					if (S.Index == Settlement && Found == nullptr)
					{
						Found = W.Components().GetPool(Context.Trade.Settlement).TryGet(H);
					}
				});
		if (Found == nullptr)
		{
			Append(Out, "town ");
			AppendNumber(Out, Settlement);
			return;
		}
		Append(Out, "the town of ");
		AppendRegion(W, Types, Found->Region, Out);
	}

	void DescribeEconomyEvent(const World& W, const History::PreHistoryTypes& Types, const EconomyContext& Context,
							  const Event& E, std::string& Out, const Population::PersonIndex* Index)
	{
		if (!IsEconomyEvent(E))
		{
			Population::DescribePersonEvent(W, Types, Context.Persons, Context.Families, E, Out, Index);
			return;
		}
		std::string Prefix;
		History::DescribeEvent(W, Types, E, Prefix);
		const usize Colon = Prefix.find(": ");
		Out.clear();
		Out += Colon != std::string::npos ? Prefix.substr(0, Colon + 2) : std::string();
		std::string Text;
		if (E.Is(RouteOpenedEvent) || E.Is(RouteClosedEvent))
		{
			const TradePayload P = E.Get<TradePayload>();
			Text.clear();
			NameRoute(W, Types, Context, P.Route, Text);
			Capitalise(Out, Text);
			Append(Out, E.Is(RouteOpenedEvent) ? " was opened." : " fell out of use.");
		}
		else if (E.Is(GoodsCarriedEvent))
		{
			const TradePayload P = E.Get<TradePayload>();
			Text.clear();
			NameRoute(W, Types, Context, P.Route, Text);
			Capitalise(Out, Text);
			Append(Out, " carried ");
			AppendNumber(Out, P.Amount);
			Append(Out, " of goods.");
		}
		else if (E.Is(SettlementFoundedEvent) || E.Is(SettlementAbandonedEvent))
		{
			const TradePayload P = E.Get<TradePayload>();
			Text.clear();
			NameSettlement(W, Types, Context, P.Route, Text);
			Capitalise(Out, Text);
			if (E.Is(SettlementFoundedEvent))
			{
				Append(Out, " rose on the traffic of its roads.");
			}
			else
			{
				Append(Out, " was abandoned.");
			}
		}
		else if (E.Is(PriceChangedEvent))
		{
			const StockPayload P = E.Get<StockPayload>();
			uint32 Floor = 0;
			uint32 Ceiling = 0;
			Bounds(Context.Prices, P.Good, Floor, Ceiling);
			Capitalise(Out, GoodName(static_cast<Good>(P.Good)));
			if (P.Amount >= Ceiling)
			{
				Append(Out, " could not be had in ");
			}
			else if (P.Amount <= Floor)
			{
				Append(Out, " was worth almost nothing in ");
			}
			else
			{
				Append(Out, " was worth ");
				AppendNumber(Out, P.Amount);
				Append(Out, " in ");
			}
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else if (E.Is(HarvestEvent))
		{
			const StockPayload P = E.Get<StockPayload>();
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " harvested ");
			AppendNumber(Out, P.Amount);
			Append(Out, " of grain.");
		}
		else if (E.Is(ShortfallEvent))
		{
			const StockPayload P = E.Get<StockPayload>();
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, " went ");
			AppendNumber(Out, P.Amount);
			Append(Out, " of grain short of its own.");
		}
		else if (E.Is(FortuneChangedEvent))
		{
			const WealthPayload P = E.Get<WealthPayload>();
			Text.clear();
			Population::NameFamily(W, Types, Context.Persons, Context.Families, P.Family, Text, Index);
			Capitalise(Out, Text);
			Append(Out, P.Value > P.Other ? " rose among the houses of " : " fell among the houses of ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else if (E.Is(HeirNamedEvent))
		{
			const WealthPayload P = E.Get<WealthPayload>();
			Text.clear();
			Population::NameFamily(W, Types, Context.Persons, Context.Families, P.Family, Text, Index);
			Capitalise(Out, Text);
			Append(Out, " named ");
			Population::NamePerson(W, Types, Context.Persons, P.Value, Out, Index);
			Append(Out, " its heir.");
		}
		else if (E.Is(StockInheritedEvent))
		{
			const StockPayload P = E.Get<StockPayload>();
			Text.clear();
			Population::NameFamily(W, Types, Context.Persons, Context.Families, P.House, Text, Index);
			Capitalise(Out, Text);
			Append(Out, " inherited ");
			AppendNumber(Out, P.Amount);
			Append(Out, " of goods from a house that died out.");
		}
		else if (E.Is(StockReturnedEvent))
		{
			const StockPayload P = E.Get<StockPayload>();
			Text.clear();
			Population::NameFamily(W, Types, Context.Persons, Context.Families, P.House, Text, Index);
			Capitalise(Out, Text);
			Append(Out, " died out and its goods returned to ");
			AppendRegion(W, Types, P.Region, Out);
			Append(Out, ".");
		}
		else
		{
			// The endowment, the split, the fold and the goods moved by hand are
			// bookkeeping: they get a plain line and are never recorded.
			const StockPayload P = E.Get<StockPayload>();
			AppendRegion(W, Types, P.Region, Out);
			if (E.Is(StockEndowedEvent))
			{
				Append(Out, " began with ");
			}
			else if (E.Is(StockSplitEvent))
			{
				Append(Out, " shared out ");
			}
			else if (E.Is(StockFoldedEvent))
			{
				Append(Out, " gathered back ");
			}
			else if (E.Is(StockAddedEvent))
			{
				Append(Out, " received ");
			}
			else
			{
				Append(Out, " gave up ");
			}
			AppendNumber(Out, P.Amount);
			Append(Out, " of goods.");
		}
	}

	uint32 ExportChronicleWithEconomy(const World& W, const History::PreHistoryTypes& Types,
									  const EconomyContext& Context, std::string& Out, uint32 MaxLines)
	{
		std::vector<History::RecordInfo> Records;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach([&](EntityHandle, const History::RecordInfo& R) { Records.push_back(R); });
		std::sort(Records.begin(), Records.end(), [](const History::RecordInfo& A, const History::RecordInfo& B)
				  { return A.Tick != B.Tick ? A.Tick < B.Tick : A.Event < B.Event; });
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		uint32 Lines = 0;
		std::string Line;
		for (const History::RecordInfo& R : Records)
		{
			if (MaxLines != 0 && Lines >= MaxLines)
			{
				break;
			}
			const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
			Line.clear();
			if (E != nullptr)
			{
				DescribeEconomyEvent(W, Types, Context, *E, Line, &Index);
			}
			else
			{
				History::DescribeRecord(W, Types, R, Line);
			}
			Out += Line;
			Out += '\n';
			++Lines;
		}
		return Lines;
	}

	uint32 ExportWhyWithEconomy(const World& W, const History::PreHistoryTypes& Types, const EconomyContext& Context,
								PersistentId Id, std::string& Out)
	{
		std::vector<History::WhyStep> Steps;
		History::Why(W, Types, Id, Steps);
		const Population::PersonIndex Index = Population::BuildPersonIndex(W, Context.Persons);
		uint32 Lines = 0;
		std::string Line;
		for (const History::WhyStep& S : Steps)
		{
			if (S.Cause == nullptr)
			{
				continue;
			}
			Line.clear();
			DescribeEconomyEvent(W, Types, Context, *S.Cause, Line, &Index);
			if (Lines > 0)
			{
				const usize Colon = Line.find(": ");
				Line = "  because " + (Colon != std::string::npos ? Line.substr(Colon + 2) : Line);
			}
			Out += Line;
			Out += '\n';
			++Lines;
		}
		return Lines;
	}

	EconomyChronicleStats CheckEconomyChronicle(const World& W, const History::PreHistoryTypes& Types,
												const EconomyContext& Context, const EconomyChronicleTypes& State)
	{
		(void)Context; // the counts need no naming, but the caller passes what it has
		EconomyChronicleStats S;
		W.Components()
			.GetPool(State.State)
			.ForEach(
				[&](EntityHandle, const EconomyChronicleState& St)
				{
					S.Records = St.Records;
					S.Dropped = St.Dropped;
				});
		std::string Line;
		W.Components()
			.GetPool(Types.History.Record)
			.ForEach(
				[&](EntityHandle, const History::RecordInfo& R)
				{
					const Event* E = History::FindEvent(W.Log(), PersistentId{R.Event});
					if (E == nullptr || !IsEconomyEvent(*E))
					{
						return;
					}
					++S.Described;
					S.WithRegion += R.Region != 0 ? 1u : 0u;
					S.EraConsistent += R.Era == History::EraAt(W, Types.History, R.Tick) ? 1u : 0u;
					if (E->Is(RouteOpenedEvent) || E->Is(RouteClosedEvent))
					{
						++S.ByType[0];
					}
					else if (E->Is(SettlementFoundedEvent) || E->Is(SettlementAbandonedEvent))
					{
						++S.ByType[1];
					}
					else if (E->Is(PriceChangedEvent))
					{
						++S.ByType[2];
					}
					else if (E->Is(ShortfallEvent))
					{
						++S.ByType[3];
					}
					else if (E->Is(FortuneChangedEvent))
					{
						++S.ByType[4];
					}
					else if (E->Is(StockInheritedEvent))
					{
						++S.ByType[5];
					}
				});
		return S;
	}
} // namespace Vaelen::Economy
