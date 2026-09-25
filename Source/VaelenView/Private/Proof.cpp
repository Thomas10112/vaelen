// VAELEN - VaelenView
// Phase 19 task 19.04: the lines a sitting is checked by. See Proof.h.
//
// STATUS: VALIDATED headless (Phase 19 task 19.04)
#include "Vaelen/View/Proof.h"

namespace Vaelen::View
{
	namespace
	{
		/// A bounded writer that remembers whether anything did not fit.
		struct Writer
		{
			char* Out;
			uint32 Bytes;
			uint32 At = 0;
			bool Full = false;

			void Char(char C)
			{
				if (At + 1u >= Bytes)
				{
					Full = true;
					return;
				}
				Out[At++] = C;
			}
			void Put(const char* S)
			{
				while (*S != '\0')
				{
					Char(*S++);
				}
			}
			void Unsigned(uint64 N)
			{
				char Digits[20];
				uint32 Count = 0;
				do
				{
					Digits[Count++] = static_cast<char>('0' + N % 10u);
					N /= 10u;
				} while (N != 0u);
				while (Count > 0u)
				{
					Char(Digits[--Count]);
				}
			}
			/// Whole degrees can be below zero; printf's %d is what the line
			/// always said, so a minus and the magnitude.
			void Signed(int64 N)
			{
				if (N < 0)
				{
					Char('-');
					Unsigned(static_cast<uint64>(0) - static_cast<uint64>(N));
					return;
				}
				Unsigned(static_cast<uint64>(N));
			}
			/// Zero-padded lower-case hex of exactly Width digits, as %0Nllx.
			void Hex(uint64 N, uint32 Width)
			{
				static const char Nibbles[] = "0123456789abcdef";
				for (uint32 Shift = Width; Shift > 0u; --Shift)
				{
					Char(Nibbles[(N >> ((Shift - 1u) * 4u)) & 0xFu]);
				}
			}
		};

		const char* SeasonName(uint32 Season)
		{
			static const char* const Names[] = {"none", "spring", "summer", "autumn", "winter"};
			return Names[Season < 5u ? Season : 0u];
		}
	} // namespace

	uint32 ClimateLine(const ClimateLineFacts& F, char* Out, uint32 Bytes)
	{
		if (Out == nullptr || Bytes == 0u)
		{
			return 0u;
		}
		Writer W{Out, Bytes};
		W.Put("LogVaelenClimate: AELVOR ");
		W.Unsigned(F.Size);
		W.Put(" seed ");
		W.Hex(F.Seed, 12u);
		W.Put(": day ");
		W.Unsigned(static_cast<uint32>(F.Day + 1u)); // uint32 arithmetic, as the format it replaced
		W.Put(" of year ");
		W.Unsigned(F.Year);
		W.Put(", ");
		W.Put(SeasonName(F.Season));
		W.Put("; coldest ");
		W.Signed(F.Stats.Coldest);
		W.Put(" warmest ");
		W.Signed(F.Stats.Warmest);
		W.Put("; frost ");
		W.Unsigned(F.Stats.Frost);
		W.Put(" of ");
		W.Unsigned(F.Stats.Tiles);
		W.Put(" tiles, ");
		W.Unsigned(F.Stats.Growing);
		W.Put(" growing; hard winters ");
		W.Unsigned(F.HardWinters);
		W.Put(", cold deaths ");
		W.Unsigned(F.ColdDeaths);
		W.Put("; climate ");
		W.Hex(F.Stats.Digest, 16u);
		if (W.Full)
		{
			Out[0] = '\0';
			return 0u;
		}
		Out[W.At] = '\0';
		return W.At;
	}
} // namespace Vaelen::View
