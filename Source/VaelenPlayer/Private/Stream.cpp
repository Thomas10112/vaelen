// VAELEN - VaelenPlayer
// Phase 14.01: the played input as a stream.
//
// STATUS: PROTOTYPE (Phase 14) - unit/edge/deterministic tests in Tests/Player/Test_Stream.cpp
#include "Vaelen/Player/Stream.h"

#include <cstdio>

namespace Vaelen::Player
{
	namespace
	{
		/// Appends what snprintf wrote, then a newline. The formats are literals
		/// at each call site so the compiler checks every argument against them,
		/// which is the reason there are three writers below and not one that
		/// takes a format.
		void Append(std::string& Out, const char* Buffer, int Wrote, usize Capacity)
		{
			if (Wrote > 0)
			{
				const usize Length = static_cast<usize>(Wrote);
				Out.append(Buffer, Length < Capacity ? Length : Capacity - 1);
				Out.push_back('\n');
			}
		}

		void WriteHeader(std::string& Out, const StreamHeader& H)
		{
			char Buffer[160];
			const int Wrote =
				std::snprintf(Buffer, sizeof(Buffer), "vaelen-stream %llu %llu %llu %llu %llu",
							  static_cast<unsigned long long>(H.Version), static_cast<unsigned long long>(H.Seed),
							  static_cast<unsigned long long>(H.Size), static_cast<unsigned long long>(H.PreHistory),
							  static_cast<unsigned long long>(H.Years));
			Append(Out, Buffer, Wrote, sizeof(Buffer));
		}

		void WriteCommand(std::string& Out, const Recorded& R)
		{
			char Buffer[160];
			const int Wrote = std::snprintf(
				Buffer, sizeof(Buffer), "c %llu %llu %llu %llu %llu %llu %llu", static_cast<unsigned long long>(R.Tick),
				static_cast<unsigned long long>(R.Command.Kind), static_cast<unsigned long long>(R.Command.Target),
				static_cast<unsigned long long>(R.Command.Amount), static_cast<unsigned long long>(R.Command.Hours),
				static_cast<unsigned long long>(R.Command.Issued),
				static_cast<unsigned long long>(static_cast<uint8>(R.Verdict)));
			Append(Out, Buffer, Wrote, sizeof(Buffer));
		}

		void WriteTaking(std::string& Out, const TakenUp& T)
		{
			char Buffer[80];
			const int Wrote =
				std::snprintf(Buffer, sizeof(Buffer), "t %llu %llu", static_cast<unsigned long long>(T.Tick),
							  static_cast<unsigned long long>(T.Person));
			Append(Out, Buffer, Wrote, sizeof(Buffer));
		}

		void WriteLook(std::string& Out, const Looked& L)
		{
			char Buffer[80];
			const int Wrote =
				std::snprintf(Buffer, sizeof(Buffer), "l %llu %llu %llu", static_cast<unsigned long long>(L.Tick),
							  static_cast<unsigned long long>(L.Region), static_cast<unsigned long long>(L.Reach));
			Append(Out, Buffer, Wrote, sizeof(Buffer));
		}

		void WriteDay(std::string& Out, const DayTurned& D)
		{
			char Buffer[48];
			const int Wrote = std::snprintf(Buffer, sizeof(Buffer), "d %llu", static_cast<unsigned long long>(D.Tick));
			Append(Out, Buffer, Wrote, sizeof(Buffer));
		}

		/// Reads one unsigned decimal field from a line, advancing past it and
		/// the single space after it. False on anything that is not digits.
		/// Hand-rolled rather than <charconv>, so the kernel's text form depends
		/// on no library's idea of a number.
		bool Field(std::string_view& Rest, uint64& Out)
		{
			usize i = 0;
			uint64 Value = 0;
			while (i < Rest.size() && Rest[i] >= '0' && Rest[i] <= '9')
			{
				const uint64 Digit = static_cast<uint64>(Rest[i] - '0');
				if (Value > (~uint64{0} - Digit) / 10u)
				{
					return false; // would overflow: not a number this stream writes
				}
				Value = Value * 10u + Digit;
				++i;
			}
			if (i == 0)
			{
				return false;
			}
			if (i < Rest.size() && Rest[i] != ' ')
			{
				return false;
			}
			Rest.remove_prefix(i < Rest.size() ? i + 1 : i);
			Out = Value;
			return true;
		}

		bool Fits32(uint64 V)
		{
			return V <= 0xFFFFFFFFull;
		}
		bool Fits8(uint64 V)
		{
			return V <= 0xFFull;
		}
	} // namespace

	bool SameWorld(const StreamHeader& A, const StreamHeader& B)
	{
		// The version is the text form's, not the world's: a stream of another
		// version is refused by the decoder as unreadable, never as "another
		// world", so that a refusal means what its name says.
		return A.Seed == B.Seed && A.Size == B.Size && A.PreHistory == B.PreHistory && A.Years == B.Years;
	}

	usize StreamRecords(const InputStream& S)
	{
		return S.Commands.size() + S.Takings.size() + S.Days.size() + S.Looks.size();
	}

	std::string EncodeStream(const InputStream& S)
	{
		std::string Out;
		WriteHeader(Out, S.Header);

		// A three-way merge on Tick, with the rule for equal ticks written as
		// the order of the three tests below: takings, then commands, then day
		// turns - the order a life is actually lived in. A person is taken up
		// at T and their first command is issued at that same T (the gates do
		// exactly this, Test_PlayerGate.cpp:1451 and :1504), and the day is
		// turned at T after the commands of T. Each vector is expected in time
		// order; if one is not, the output is still the same for the same
		// input, which is all a replay needs from an encoder.
		//
		// Exhaustion is tracked by the three booleans and NOT by a sentinel
		// tick: the first version used ~0 as "no more" and a genuine record at
		// tick 2^64-1 - which Field() admits, and rightly - read past the end
		// of an empty vector. Found by review before it shipped.
		//
		// 15.06 added a fourth arm for looks, FIRST at equal ticks: somebody
		// looks and then acts on what they saw. A stream with no looks leaves
		// Hl false on every pass, so the three arms below decide exactly what
		// they decided before and the encoder's output for every stream written
		// until now is byte for byte what it was.
		usize c = 0, t = 0, d = 0, l = 0;
		while (c < S.Commands.size() || t < S.Takings.size() || d < S.Days.size() || l < S.Looks.size())
		{
			const bool Hc = c < S.Commands.size();
			const bool Ht = t < S.Takings.size();
			const bool Hd = d < S.Days.size();
			const bool Hl = l < S.Looks.size();
			const uint64 Tc = Hc ? S.Commands[c].Tick : 0;
			const uint64 Tt = Ht ? S.Takings[t].Tick : 0;
			const uint64 Td = Hd ? S.Days[d].Tick : 0;
			const uint64 Tl = Hl ? S.Looks[l].Tick : 0;
			if (Hl && (!Ht || Tl <= Tt) && (!Hc || Tl <= Tc) && (!Hd || Tl <= Td))
			{
				WriteLook(Out, S.Looks[l++]);
			}
			else if (Ht && (!Hc || Tt <= Tc) && (!Hd || Tt <= Td))
			{
				WriteTaking(Out, S.Takings[t++]);
			}
			else if (Hc && (!Hd || Tc <= Td))
			{
				WriteCommand(Out, S.Commands[c++]);
			}
			else
			{
				WriteDay(Out, S.Days[d++]);
			}
		}
		return Out;
	}

	bool DecodeStream(std::string_view Text, InputStream& Out, StreamReport& Report, const StreamHeader* Expect)
	{
		Report = StreamReport{};
		InputStream Read;
		bool HaveHeader = false;

		// A line is what precedes a newline, or what follows the last newline
		// when the text does not end in one. Nothing follows a final newline,
		// so nothing is counted there: Lines is the count a person gets from
		// wc -l on the encoder's own output, and 0 for empty text.
		usize At = 0;
		while (At < Text.size())
		{
			const usize End = Text.find('\n', At);
			std::string_view L = Text.substr(At, End == std::string_view::npos ? std::string_view::npos : End - At);
			At = End == std::string_view::npos ? Text.size() : End + 1;
			if (!L.empty() && L.back() == '\r')
			{
				L.remove_suffix(1);
			}
			++Report.Lines;
			if (L.empty())
			{
				continue;
			}

			if (!HaveHeader)
			{
				constexpr std::string_view Magic = "vaelen-stream ";
				if (L.substr(0, Magic.size()) != Magic)
				{
					Report.HeaderBad = 1;
					return false;
				}
				std::string_view Rest = L.substr(Magic.size());
				uint64 V = 0, Seed = 0, Size = 0, Pre = 0, Years = 0;
				if (!Field(Rest, V) || !Field(Rest, Seed) || !Field(Rest, Size) || !Field(Rest, Pre) ||
					!Field(Rest, Years) || !Rest.empty() || !Fits32(V) || !Fits32(Size) || !Fits32(Pre) ||
					!Fits32(Years))
				{
					Report.HeaderBad = 1;
					return false;
				}
				if (V != 1)
				{
					// A header, and a well-formed one, in a form this build does
					// not read. Named as such: reading a version-2 stream under
					// version-1 rules would drop every unknown field as a bad
					// line and call the result a success.
					Report.VersionBad = 1;
					return false;
				}
				Read.Header.Version = static_cast<uint32>(V);
				Read.Header.Seed = Seed;
				Read.Header.Size = static_cast<uint32>(Size);
				Read.Header.PreHistory = static_cast<uint32>(Pre);
				Read.Header.Years = static_cast<uint32>(Years);
				if (Expect != nullptr && !SameWorld(Read.Header, *Expect))
				{
					Report.Refused = 1;
					return false;
				}
				HaveHeader = true;
				continue;
			}

			// A record: one letter, one space, then fields.
			if (L.size() < 3 || L[1] != ' ')
			{
				++Report.BadLines;
				continue;
			}
			std::string_view Rest = L.substr(2);
			bool Ok = false;
			if (L[0] == 'c')
			{
				uint64 Tick = 0, Kind = 0, Target = 0, Amount = 0, Hours = 0, Issued = 0, Verdict = 0;
				Ok = Field(Rest, Tick) && Field(Rest, Kind) && Field(Rest, Target) && Field(Rest, Amount) &&
					 Field(Rest, Hours) && Field(Rest, Issued) && Field(Rest, Verdict) && Rest.empty() && Fits8(Kind) &&
					 Fits32(Target) && Fits32(Amount) && Fits32(Hours) && Fits8(Verdict);
				if (Ok)
				{
					Recorded R;
					R.Tick = Tick;
					R.Command.Kind = static_cast<uint8>(Kind);
					R.Command.Target = static_cast<uint32>(Target);
					R.Command.Amount = static_cast<uint32>(Amount);
					R.Command.Hours = static_cast<uint32>(Hours);
					R.Command.Issued = Issued;
					R.Verdict = static_cast<Refusal>(Verdict);
					Read.Commands.push_back(R);
				}
			}
			else if (L[0] == 't')
			{
				uint64 Tick = 0, Person = 0;
				Ok = Field(Rest, Tick) && Field(Rest, Person) && Rest.empty() && Fits32(Person);
				if (Ok)
				{
					Read.Takings.push_back(TakenUp{Tick, static_cast<uint32>(Person), 0});
				}
			}
			else if (L[0] == 'd')
			{
				uint64 Tick = 0;
				Ok = Field(Rest, Tick) && Rest.empty();
				if (Ok)
				{
					Read.Days.push_back(DayTurned{Tick});
				}
			}
			else if (L[0] == 'l')
			{
				uint64 Tick = 0, Region = 0, Reach = 0;
				Ok = Field(Rest, Tick) && Field(Rest, Region) && Field(Rest, Reach) && Rest.empty() && Fits32(Region) &&
					 Fits32(Reach);
				if (Ok)
				{
					Read.Looks.push_back(Looked{Tick, static_cast<uint32>(Region), static_cast<uint32>(Reach)});
				}
			}
			if (Ok)
			{
				++Report.Records;
			}
			else
			{
				++Report.BadLines;
			}
		}
		if (!HaveHeader)
		{
			Report.HeaderBad = 1;
			return false;
		}
		Out = Read;
		return true;
	}
} // namespace Vaelen::Player
