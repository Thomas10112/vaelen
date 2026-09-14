// VAELEN - VaelenView
// Phase 14 task 14.05: the last lines of one played life, as the screen that
// shows them needs them.
//
// STATUS: PROTOTYPE (Phase 14) - unit/integration/deterministic tests in Tests/View/Test_Chronicle.cpp
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Take.h"

#include "Vaelen/Economy/EconomyHistory.h"
#include "Vaelen/Player/Commands.h"
#include "Vaelen/Player/Player.h"
#include "Vaelen/Player/PlayerHistory.h"
#include "Vaelen/Population/Lives.h"
#include "Vaelen/Population/Lod.h"
#include "Vaelen/Population/PersonHistory.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/PreHistory.h"
#include "Vaelen/Sim/World.h"
#include "Vaelen/Society/SocietyHistory.h"

#include <cstring>
#include <string>
#include <vector>

namespace Vaelen::View
{
	namespace
	{
		/// What kind of line an event makes, and for whom. The five kinds
		/// Player::LifeTimeline keeps, so that the lines here are the lines of
		/// ExportLife; Kind None for anything else.
		LineKind KindOf(const Event& E, uint32& Person, uint32& Verb, uint32& Refused)
		{
			Person = 0;
			Verb = 0;
			Refused = 0;
			if (E.Is(Player::PlayerActedEvent) || E.Is(Player::PlayerRefusedEvent))
			{
				const Player::ActPayload A = E.Get<Player::ActPayload>();
				Person = A.Person;
				Verb = A.Kind;
				if (E.Is(Player::PlayerRefusedEvent))
				{
					Refused = A.Amount;
					return LineKind::Refused;
				}
				return LineKind::Acted;
			}
			if (E.Is(Population::PersonMovedEvent) || E.Is(Population::PersonBornEvent) ||
				E.Is(Population::PersonDiedEvent))
			{
				Person = E.Get<Population::PersonPayload>().Person;
				return E.Is(Population::PersonMovedEvent)  ? LineKind::Walked
					   : E.Is(Population::PersonBornEvent) ? LineKind::Born
														   : LineKind::Died;
			}
			return LineKind::None;
		}

		bool IsAbout(const Event& E, uint32 Played)
		{
			uint32 Person = 0;
			uint32 Verb = 0;
			uint32 Refused = 0;
			return KindOf(E, Person, Verb, Refused) != LineKind::None && Person == Played;
		}

		void Describe(const Event& E, LineView& Out)
		{
			uint32 Person = 0;
			Out.Tick = static_cast<uint64>(E.Tick);
			Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
			Out.Kind = static_cast<uint32>(KindOf(E, Person, Out.Verb, Out.Refused));
		}

		/// Drops the oldest N lines: the rest slide to the front, the tail is
		/// zeroed, so the bytes are the same as if the dropped lines had never
		/// been there.
		void DropFront(ChronicleView& V, uint32 N)
		{
			if (N == 0)
			{
				return;
			}
			if (N >= V.LineCount)
			{
				V.Dropped += V.LineCount;
				V.LineCount = 0;
				V.Used = 0;
				for (LineView& L : V.Lines)
				{
					L = LineView{};
				}
				std::memset(V.Text, 0, sizeof(V.Text));
				return;
			}
			const uint32 Cut = V.Lines[N].Begin;
			std::memmove(V.Text, V.Text + Cut, V.Used - Cut);
			std::memset(V.Text + (V.Used - Cut), 0, Cut);
			V.Used -= Cut;
			for (uint32 i = N; i < V.LineCount; ++i)
			{
				V.Lines[i - N] = V.Lines[i];
				V.Lines[i - N].Begin -= Cut;
			}
			for (uint32 i = V.LineCount - N; i < V.LineCount; ++i)
			{
				V.Lines[i] = LineView{};
			}
			V.LineCount -= N;
			V.Dropped += N;
		}

		/// Appends one line, terminator included, dropping the oldest lines
		/// until it fits. A line longer than the whole buffer is cut and
		/// counted: nothing the kernel writes is, and the count says if that
		/// ever changes.
		void Append(ChronicleView& V, const LineView& Line, const std::string& Text)
		{
			uint32 Length = static_cast<uint32>(Text.size());
			if (Length + 1 > ChronicleTextBytes)
			{
				Length = ChronicleTextBytes - 1;
				++V.Truncated;
			}
			if (V.LineCount == ChronicleLines)
			{
				DropFront(V, 1);
			}
			while (V.LineCount > 0 && V.Used + Length + 1 > ChronicleTextBytes)
			{
				DropFront(V, 1);
			}
			LineView& L = V.Lines[V.LineCount];
			L = Line;
			L.Begin = V.Used;
			L.Length = Length;
			std::memcpy(V.Text + V.Used, Text.data(), Length);
			V.Text[V.Used + Length] = '\0';
			V.Used += Length + 1;
			++V.LineCount;
		}

		/// The why of one event: ExportWhyWithLife's lines, the first WhyLines
		/// of them, each cut at WhyTextBytes when it must be.
		void TakeWhy(const World& W, const ViewSources& From, const Player::LifeContext& Life, ChronicleView& V)
		{
			V.WhyCount = 0;
			V.WhyUsed = 0;
			V.WhyTruncated = 0;
			for (LineView& L : V.Why)
			{
				L = LineView{};
			}
			std::memset(V.WhyText, 0, sizeof(V.WhyText));
			if (V.WhyOf == 0)
			{
				return;
			}
			std::vector<History::WhyStep> Steps;
			History::Why(W, From.Types, PersistentId{V.WhyOf}, Steps, WhyLines);
			std::string All;
			Player::ExportWhyWithLife(W, From.Types, Life, PersistentId{V.WhyOf}, All);
			usize At = 0;
			usize Step = 0;
			while (At < All.size() && V.WhyCount < WhyLines && V.WhyUsed + 1 < WhyTextBytes)
			{
				usize End = All.find('\n', At);
				if (End == std::string::npos)
				{
					End = All.size();
				}
				uint32 Length = static_cast<uint32>(End - At);
				const uint32 Room = WhyTextBytes - 1 - V.WhyUsed; // > 0 by the loop's condition
				if (Length > Room)
				{
					Length = Room;
					++V.WhyTruncated;
				}
				LineView& L = V.Why[V.WhyCount];
				for (; Step < Steps.size() && Steps[Step].Cause == nullptr; ++Step)
				{
				}
				if (Step < Steps.size())
				{
					Describe(*Steps[Step].Cause, L);
					++Step;
				}
				L.Begin = V.WhyUsed;
				L.Length = Length;
				std::memcpy(V.WhyText + V.WhyUsed, All.data() + At, Length);
				V.WhyText[V.WhyUsed + Length] = '\0';
				V.WhyUsed += Length + 1;
				++V.WhyCount;
				At = End + 1;
			}
		}
	} // namespace

	uint32 TakeChronicleView(const World& W, const ViewSources& From, ChronicleView& Out)
	{
		const uint32 Played = From.HasPlayer && From.HasLife ? Player::PlayerPerson(W, From.Played) : 0u;
		if (Played != Out.Person || Played == 0)
		{
			Out = ChronicleView{};
			Out.Person = Played;
		}
		Out.Tick = static_cast<uint64>(W.Now());
		Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		if (Played == 0)
		{
			return 0;
		}

		// The same context Run::Aelvor::Life() describes with, so the lines
		// are ExportLife's to the byte: the goods speak for the layers under
		// them when the sources carry them, else the person layer does.
		Society::SocietyContext Society{From.Persons, From.Families, From.Organizations};
		Economy::EconomyContext Goods{From.Persons, From.Families,			From.Trade,
									  From.Markets, Economy::MarketRules{}, &Society};
		Player::LifeContext Life;
		Life.Persons = From.Persons;
		Life.Families = From.Families;
		Life.Player = From.Played;
		Life.Regard = From.Regard;
		Life.Goods = From.HasGoods && From.HasTrade ? &Goods : nullptr;

		const EventLog& Log = W.Log();
		const uint64 Count = Log.Count();
		if (Out.Since > Count || (Out.Since > 0 && Log.At(Out.Since - 1).Hash() != Out.LastHash))
		{
			// Another world: a log shorter than what was read, or one whose
			// last event read is not the one this view read. Start over.
			Out = ChronicleView{};
			Out.Person = Played;
			Out.Tick = static_cast<uint64>(W.Now());
			Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		}
		const uint64 Was = Out.Since;
		const uint64 WhyWas = Out.WhyOf;
		bool Indexed = false;
		Population::PersonIndex Index;
		std::string Text;
		for (uint64 i = Out.Since; i < Count; ++i)
		{
			const Event& E = Log.At(i);
			if (IsAbout(E, Played))
			{
				if (!Indexed)
				{
					Index = Population::BuildPersonIndex(W, From.Persons);
					Indexed = true;
				}
				LineView Line;
				Describe(E, Line);
				Text.clear();
				Player::DescribeLifeEvent(W, From.Types, Life, E, Text, &Index);
				Append(Out, Line, Text);
			}
			if (E.Cause.IsValid())
			{
				const Event* Cause = History::FindEvent(Log, E.Cause);
				if (Cause != nullptr && IsAbout(*Cause, Played))
				{
					Out.WhyOf = E.Id.Value; // the newest thing that happened because of them
				}
			}
		}
		Out.Since = Count;
		if (Count > Was)
		{
			Out.LastHash = Log.At(Count - 1).Hash();
		}
		if (Out.WhyOf != WhyWas)
		{
			TakeWhy(W, From, Life, Out);
		}
		return static_cast<uint32>(Count - Was);
	}

	ChronicleViewStats MeasureChronicleView(const ChronicleView& V)
	{
		ChronicleViewStats Out;
		Out.Lines = V.LineCount;
		Out.Bytes = static_cast<uint32>(sizeof(ChronicleView));
		Out.EventsRead = static_cast<uint32>(V.Since);
		Out.Truncated = V.Truncated + V.WhyTruncated;
		Out.WhyLines_ = V.WhyCount;
		const auto Count = [&](const char* Text, uint32 Used)
		{
			for (uint32 i = 0; i < Used; ++i)
			{
				const unsigned char c = static_cast<unsigned char>(Text[i]);
				if (c != 0 && (c < 0x20 || c > 0x7e))
				{
					++Out.NonAscii;
				}
			}
		};
		Count(V.Text, V.Used);
		Count(V.WhyText, V.WhyUsed);
		Out.Digest = HashBytes(reinterpret_cast<const char*>(&V), sizeof(ChronicleView));
		return Out;
	}
} // namespace Vaelen::View
