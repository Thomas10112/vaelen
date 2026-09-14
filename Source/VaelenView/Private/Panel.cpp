// VAELEN - VaelenView
// Phase 14 task 14.06: the first screen, composed kernel-side.
//
// STATUS: PROTOTYPE (Phase 14) - unit/integration/deterministic tests in Tests/View/Test_Panel.cpp
#include "Vaelen/View/Panel.h"

#include "Vaelen/Player/Commands.h"

namespace Vaelen::View
{
	namespace
	{
		/// The page is written through this and nowhere else: every byte goes
		/// through a bound that leaves room for the row's terminator, and a row
		/// that does not fit is cut and counted rather than lost or overrun.
		struct Page
		{
			PanelView& V;
			uint32 Stop;	///< no byte may be written at or past this
			uint32 MaxRows; ///< nor a row opened at or past this
			bool Open = false;
			bool Cut = false;
			uint32 Begun = 0; ///< where the open row starts

			bool Begin(RowKind Kind, uint32 Verb = 0)
			{
				if (Open || V.RowCount >= MaxRows || V.Used + 1 >= Stop)
				{
					return false;
				}
				Open = true;
				Cut = false;
				Begun = V.Used;
				RowView& R = V.Rows[V.RowCount];
				R = RowView{};
				R.Kind = static_cast<uint32>(Kind);
				R.Verb = Verb;
				R.Begin = V.Used;
				return true;
			}

			void Put(const char* S)
			{
				for (; S != nullptr && *S != '\0'; ++S)
				{
					if (V.Used + 1 >= Stop)
					{
						Cut = true;
						return;
					}
					V.Text[V.Used++] = *S;
				}
			}

			void Char(char C)
			{
				if (V.Used + 1 >= Stop)
				{
					Cut = true;
					return;
				}
				V.Text[V.Used++] = C;
			}

			void Number(uint64 N)
			{
				char Digits[20] = {};
				uint32 n = 0;
				do
				{
					Digits[n++] = static_cast<char>('0' + static_cast<char>(N % 10));
					N /= 10;
				} while (N != 0 && n < 20);
				while (n > 0)
				{
					if (V.Used + 1 >= Stop)
					{
						Cut = true;
						return;
					}
					V.Text[V.Used++] = Digits[--n];
				}
			}

			void Hex(uint64 N)
			{
				static const char Figures[] = "0123456789abcdef";
				for (uint32 i = 0; i < PanelDigestBytes; ++i)
				{
					const uint32 Shift = (PanelDigestBytes - 1 - i) * 4;
					if (V.Used + 1 >= Stop)
					{
						Cut = true;
						return;
					}
					V.Text[V.Used++] = Figures[(N >> Shift) & 0xfu];
				}
			}

			/// Pads with spaces to a column of the open row, so the eight verbs
			/// line up in a screenshot without anybody counting letters.
			void Column(uint32 To)
			{
				while (V.Used - Begun < To)
				{
					if (V.Used + 1 >= Stop)
					{
						Cut = true;
						return;
					}
					V.Text[V.Used++] = ' ';
				}
			}

			void End()
			{
				if (!Open)
				{
					return;
				}
				RowView& R = V.Rows[V.RowCount];
				R.Length = V.Used - R.Begin;
				V.Text[V.Used++] = '\0';
				++V.RowCount;
				V.Truncated += Cut ? 1u : 0u;
				Open = false;
			}
		};

		const char* Named(Player::Intent Verb)
		{
			return Player::IntentName(Verb);
		}

		/// What the page can foresee, from the three views and nothing else.
		/// What it cannot - no grain to eat, somebody who dies before the hour
		/// comes - is the world's answer, and arrives as a chronicle line.
		Player::Refusal Foresee(const LifeView& Life, Player::Intent Verb, uint32 Cost)
		{
			if (Life.Person == 0)
			{
				return Player::Refusal::NoPlayer;
			}
			if (Life.Alive == 0)
			{
				return Player::Refusal::Dead;
			}
			if (Life.Held >= MostWaiting)
			{
				return Player::Refusal::Full;
			}
			if (Cost > Life.Awake)
			{
				return Player::Refusal::Costly; // nothing can pay for it, ever
			}
			if (Verb == Player::Intent::Move && Life.NearCount == 0)
			{
				return Player::Refusal::TooFar;
			}
			if ((Verb == Player::Intent::Speak || Verb == Player::Intent::Give || Verb == Player::Intent::Take) &&
				Life.CompanyThere == 0)
			{
				return Player::Refusal::NoOne;
			}
			return Player::Refusal::None;
		}

		void FillVerbs(const LifeView& Life, PanelView& Out)
		{
			static const char Keys[PanelVerbs] = {'1', '2', '3', '4', '5', '6', '7', '8'};
			Out.Offered = 0;
			for (uint32 i = 0; i < PanelVerbs; ++i)
			{
				const Player::Intent Verb = static_cast<Player::Intent>(i + 1); // Wait .. Take
				VerbView& Slot = Out.Verbs[i];
				Slot = VerbView{};
				Slot.Verb = static_cast<uint32>(Verb);
				Slot.Cost = static_cast<uint32>(Verb) < IntentSlots ? Life.Cost[static_cast<uint32>(Verb)] : 0u;
				Slot.Key = static_cast<uint8>(Keys[i]);
				const Player::Refusal Why = Foresee(Life, Verb, Slot.Cost);
				Slot.Foreseen = static_cast<uint32>(Why);
				// The hours left are the one obstacle that is not a refusal:
				// what the day cannot pay for waits for tomorrow (Commands.cpp).
				Slot.Offered = Why == Player::Refusal::None && Slot.Cost <= Life.Left ? 1u : 0u;
				Out.Offered += Slot.Offered;
			}
		}
	} // namespace

	void TakePanel(const WorldView& World_, const LifeView& Life, const ChronicleView& Told, PanelView& Out)
	{
		Out = PanelView{};
		Out.Tick = Life.Tick;
		Out.Year = Life.Year;
		Out.Day = Life.Day;
		Out.Person = Life.Person;
		Out.Alive = Life.Alive;
		Out.Left = Life.Left;
		Out.Awake = Life.Awake;
		Out.Held = Life.Held;
		FillVerbs(Life, Out);

		// Room is kept for the digest row, which is the one row the page must
		// always be able to write: it is what a screenshot is checked by.
		Page P{Out, PanelTextBytes - (PanelDigestBytes + 10), PanelRows - 1};

		if (P.Begin(RowKind::Date))
		{
			P.Put("AELVOR  year ");
			P.Number(Out.Year);
			P.Put("  day ");
			P.Number(uint64{Out.Day} + 1);
			P.Put("  ");
			P.Number(World_.People);
			P.Put(" alive in ");
			P.Number(static_cast<uint64>(World_.Regions.size()));
			P.Put(" regions");
			P.End();
		}

		if (P.Begin(RowKind::Self))
		{
			if (Out.Person == 0)
			{
				P.Put("nobody is played");
			}
			else
			{
				P.Put(Life.Name);
				P.Put(" of ");
				P.Put(Life.RegionName);
				P.Put(", ");
				P.Number(Life.Years);
				P.Put(Life.Alive != 0 ? "" : ", dead");
			}
			P.End();
		}
		if (Out.Person != 0 && Life.Bond != 0 && P.Begin(RowKind::Self))
		{
			// Who held them at the first moment (10.02). The holder's CURRENT
			// bond is Society's and no view carries it yet: this is the start's.
			P.Put("bound to ");
			P.Put(Life.Holder != 0 ? Life.HolderName : "the place");
			P.Put(" since year ");
			P.Number(Life.StartYear);
			P.End();
		}

		if (P.Begin(RowKind::Body))
		{
			P.Put("food ");
			P.Number(Life.Food);
			P.Put("  health ");
			P.Number(Life.Health);
			P.Put("  rest ");
			P.Number(Life.Rest);
			P.End();
		}

		if (P.Begin(RowKind::Hours))
		{
			P.Put("hours left ");
			P.Number(Life.Left);
			P.Put(" of ");
			P.Number(Life.Awake);
			P.End();
		}
		if (P.Begin(RowKind::Hours))
		{
			P.Put("queue ");
			P.Number(Life.Held);
			P.Put(" held, ");
			P.Number(Life.Taken);
			P.Put(" taken, ");
			P.Number(Life.Refused);
			P.Put(" refused");
			P.End();
		}
		if (Life.LastRefusal != 0 && P.Begin(RowKind::Hours))
		{
			P.Put("last: ");
			P.Put(Player::RefusalName(static_cast<Player::Refusal>(Life.LastRefusal)));
			P.End();
		}

		for (uint32 i = 0; i < PanelVerbs; ++i)
		{
			const VerbView& Slot = Out.Verbs[i];
			if (!P.Begin(RowKind::Verb, Slot.Verb))
			{
				break;
			}
			P.Char('[');
			P.Char(static_cast<char>(Slot.Key));
			P.Put("] ");
			P.Put(Named(static_cast<Player::Intent>(Slot.Verb)));
			P.Column(14);
			P.Number(Slot.Cost);
			P.Put("h  ");
			if (Slot.Offered != 0)
			{
				P.Put("ok");
			}
			else if (Slot.Foreseen != 0)
			{
				P.Put("-  ");
				P.Put(Player::RefusalName(static_cast<Player::Refusal>(Slot.Foreseen)));
			}
			else
			{
				P.Put("-  not today");
			}
			P.End();
		}

		if (P.Begin(RowKind::Near))
		{
			P.Put("near:");
			if (Life.NearCount == 0)
			{
				P.Put(" nowhere a walk reaches");
			}
			for (uint32 i = 0; i < Life.NearCount && i < MostNear; ++i)
			{
				const RegionView* R = RegionIn(World_, Life.Near[i]);
				P.Put(" ");
				P.Number(Life.Near[i]);
				P.Put("(");
				P.Number(R != nullptr ? R->People : 0u);
				P.Put(")");
			}
			P.End();
		}

		if (P.Begin(RowKind::Company))
		{
			P.Put("here:");
			if (Life.CompanyThere == 0)
			{
				P.Put(" nobody");
			}
			for (uint32 i = 0; i < Life.CompanyCount && i < PanelNamed; ++i)
			{
				P.Put(i == 0 ? " " : ", ");
				P.Put(Life.Company[i].Name);
			}
			if (Life.CompanyThere > PanelNamed)
			{
				P.Put(" and ");
				P.Number(uint64{Life.CompanyThere} - PanelNamed);
				P.Put(" more");
			}
			P.End();
		}

		const uint32 First = Told.LineCount > PanelChronicle ? Told.LineCount - PanelChronicle : 0u;
		for (uint32 i = First; i < Told.LineCount && i < ChronicleLines; ++i)
		{
			if (!P.Begin(RowKind::Chronicle, Told.Lines[i].Verb))
			{
				break;
			}
			P.Put("  ");
			P.Put(Told.Text + Told.Lines[i].Begin);
			P.End();
		}

		// The page's own digest, of every byte written above, as its last row.
		Out.Digest = HashBytes(Out.Text, Out.Used);
		P.Stop = PanelTextBytes;
		P.MaxRows = PanelRows;
		if (P.Begin(RowKind::Digest))
		{
			P.Put("digest ");
			P.Hex(Out.Digest);
			P.End();
		}
	}

	Player::Refusal Press(const PanelView& V, Player::Intent Verb, uint32 Target, uint32 Amount,
						  Player::PlayerCommand& Out)
	{
		for (const VerbView& Slot : V.Verbs)
		{
			if (Slot.Verb != static_cast<uint32>(Verb))
			{
				continue;
			}
			if (Slot.Offered == 0)
			{
				// What the page foresaw, or Costly when the only want is the
				// hours this day has left. The kernel's Costly is narrower -
				// more than a WHOLE day, which nothing can ever pay for
				// (Commands.cpp) - and the page's is "more hours than are
				// left", which tomorrow answers; the row says the page
				// refuses both, and this is the nearest true word for it.
				// Neither costs the world a tick.
				return Slot.Foreseen != 0 ? static_cast<Player::Refusal>(Slot.Foreseen) : Player::Refusal::Costly;
			}
			Out = Player::PlayerCommand{};
			Out.Kind = static_cast<uint8>(Verb);
			Out.Target = Target;
			Out.Amount = Amount;
			Out.Issued = 0; // the door stamps this; a page cannot know the tick
			return Player::Refusal::None;
		}
		return Player::Refusal::Unknown;
	}

	uint32 Lines(const PanelView& V, char* Out, uint32 Bytes)
	{
		if (Out == nullptr || Bytes == 0)
		{
			return 0;
		}
		uint32 At = 0;
		for (uint32 i = 0; i < V.RowCount && i < PanelRows; ++i)
		{
			const RowView& R = V.Rows[i];
			if (At + R.Length + 1 >= Bytes)
			{
				Out[At] = '\0';
				return At;
			}
			for (uint32 n = 0; n < R.Length; ++n)
			{
				Out[At++] = V.Text[R.Begin + n];
			}
			Out[At++] = '\n';
		}
		Out[At] = '\0';
		return At;
	}

	PanelStats MeasurePanel(const PanelView& V)
	{
		PanelStats Out;
		Out.Rows = V.RowCount;
		Out.Bytes = static_cast<uint32>(sizeof(PanelView));
		Out.TextBytes = V.Used;
		Out.Truncated = V.Truncated;
		Out.Offered = V.Offered;
		uint32 Before = V.Used;
		for (uint32 i = 0; i < V.RowCount && i < PanelRows; ++i)
		{
			const RowView& R = V.Rows[i];
			if (R.Kind == static_cast<uint32>(RowKind::Digest))
			{
				Before = R.Begin;
			}
			for (uint32 n = 0; n < R.Length; ++n)
			{
				const unsigned char c = static_cast<unsigned char>(V.Text[R.Begin + n]);
				Out.NonAscii += c < 0x20 || c > 0x7e ? 1u : 0u;
			}
		}
		// Recomputed from the text, not read back from the field: the last row
		// prints this, and a page whose digest row lies is a page that failed.
		Out.Digest = HashBytes(V.Text, Before);
		return Out;
	}
} // namespace Vaelen::View
