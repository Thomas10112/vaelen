// VAELEN - VaelenScene
// The bounded writer the scene's lines are composed with (Terrain, Layout): all
// or nothing, numbers as printf's %u, %d and %0Nllx would print them.
//
// STATUS: VALIDATED headless (Phase 19 task 19.08)
#pragma once

#include "Vaelen/Core/CoreTypes.h"

namespace Vaelen::Scene::Detail
{
	struct LineWriter
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
			char D[20];
			uint32 C = 0;
			do
			{
				D[C++] = static_cast<char>('0' + N % 10u);
				N /= 10u;
			} while (N != 0u);
			while (C > 0u)
			{
				Char(D[--C]);
			}
		}
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
		void Hex(uint64 N, uint32 Width)
		{
			static const char Nibbles[] = "0123456789abcdef";
			for (uint32 Shift = Width; Shift > 0u; --Shift)
			{
				Char(Nibbles[(N >> ((Shift - 1u) * 4u)) & 0xFu]);
			}
		}
		/// The line's length, or 0 with an empty string when it did not fit.
		uint32 Finish()
		{
			if (Full)
			{
				Out[0] = '\0';
				return 0u;
			}
			Out[At] = '\0';
			return At;
		}
	};
} // namespace Vaelen::Scene::Detail
