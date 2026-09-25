// VAELEN - Phase 19 task 19.04: the lines a sitting is checked by, composed once.
//
// LogVaelenClimate was a printf in the Atlas. From 19.04 it is composed by the
// View leaf (Vaelen/View/Proof.h) that the Atlas calls today and the engine's
// Vaelen.Play calls from 19.06, so the line a sitting brings back is compared
// with the headless one byte for byte. These cases hold the composer to the
// format string it replaced - two instruments on one line - to its own
// literal, and to its all-or-nothing rule. That the Atlas's real line did not
// move is Atlas.ClimateFrozen128's to say (its digest is the line's last word).
#include "Vaelen/View/Proof.h"
#include "Vaelen/Core/Random.h"
#include "VaelenTest.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace Vaelen;
using namespace Vaelen::View;

namespace
{
	/// The format string the Atlas printed until 19.04, verbatim - the other
	/// instrument. If the composer and this ever disagree, one of them moved.
	std::string AsTheAtlasPrintedIt(const ClimateLineFacts& F)
	{
		static const char* const Seasons[] = {"none", "spring", "summer", "autumn", "winter"};
		char Buffer[512];
		std::snprintf(Buffer, sizeof(Buffer),
					  "LogVaelenClimate: AELVOR %u seed %012llx: day %u of year %u, %s; coldest %d warmest %d; "
					  "frost %u of %u tiles, %u growing; hard winters %u, cold deaths %u; climate %016llx",
					  F.Size, static_cast<unsigned long long>(F.Seed), F.Day + 1u, F.Year,
					  Seasons[F.Season < 5u ? F.Season : 0u], F.Stats.Coldest, F.Stats.Warmest, F.Stats.Frost,
					  F.Stats.Tiles, F.Stats.Growing, F.HardWinters, F.ColdDeaths,
					  static_cast<unsigned long long>(F.Stats.Digest));
		return Buffer;
	}

	std::string Composed(const ClimateLineFacts& F)
	{
		char Line[ClimateLineBytes];
		const uint32 Wrote = ClimateLine(F, Line, ClimateLineBytes);
		return std::string(Line, Wrote);
	}

	ClimateLineFacts Winter()
	{
		ClimateLineFacts F;
		F.Size = 128;
		F.Seed = 0x41454c564f52ull;
		F.Day = 314;
		F.Year = 420;
		F.Season = 4;
		F.Stats.Tiles = 16384;
		F.Stats.Frost = 5210;
		F.Stats.Growing = 3977;
		F.Stats.Coldest = -26;
		F.Stats.Warmest = 31;
		F.Stats.Digest = 0x59615fa1f5d24bd1ull;
		F.HardWinters = 12835;
		F.ColdDeaths = 1581;
		return F;
	}
} // namespace

VAELEN_TEST(Proof, TheLineIsTheLiteral)
{
	// Hand-built facts, so a composer that read a field from the wrong place
	// cannot hide behind a real world's coincidences. The minus sign is the
	// point of Coldest = -26: an unsigned writer prints 18446744073709551590.
	VT_CHECK_EQ(Composed(Winter()),
				std::string("LogVaelenClimate: AELVOR 128 seed 41454c564f52: day 315 of year 420, winter; coldest -26 "
							"warmest 31; frost 5210 of 16384 tiles, 3977 growing; hard winters 12835, cold deaths "
							"1581; climate 59615fa1f5d24bd1"));
}

VAELEN_TEST(Proof, TheComposerIsTheFormatItReplaced)
{
	// Two instruments: the leaf's own writer and the C library's printf over
	// the Atlas's old format string, on a thousand seeded fact sets - the
	// extremes of every field among them.
	RandomStream Draw(0x19040000ull);
	uint32 Agreed = 0;
	for (uint32 i = 0; i < 1000u; ++i)
	{
		ClimateLineFacts F;
		F.Size = Draw.NextU32();
		F.Seed = Draw.NextU64() & 0xFFFFFFFFFFFFull;
		F.Day = Draw.NextU32() % 360u;
		F.Year = Draw.NextU32();
		F.Season = Draw.NextU32() % 7u; // 5 and 6 are no season: printed "none"
		F.Stats.Tiles = Draw.NextU32();
		F.Stats.Frost = Draw.NextU32();
		F.Stats.Growing = Draw.NextU32();
		F.Stats.Coldest = static_cast<int32>(Draw.NextU32());
		F.Stats.Warmest = static_cast<int32>(Draw.NextU32());
		F.Stats.Digest = Draw.NextU64();
		F.HardWinters = Draw.NextU32();
		F.ColdDeaths = Draw.NextU32();
		if (i == 0u)
		{
			F.Stats.Coldest = INT32_MIN;
			F.Stats.Warmest = INT32_MAX;
			F.Day = 359u;
			F.Size = 0xFFFFFFFFu;
		}
		const std::string Mine = Composed(F);
		const std::string Theirs = AsTheAtlasPrintedIt(F);
		VT_CHECK_MSG(Mine == Theirs, "fact set %u: composed\n  %s\nprintf\n  %s", i, Mine.c_str(), Theirs.c_str());
		Agreed += Mine == Theirs ? 1u : 0u;
	}
	VT_CHECK_EQ(Agreed, 1000u);
}

VAELEN_TEST(Proof, TheWidestLineFits)
{
	// Every number at its widest: the buffer size the header promises is enough.
	ClimateLineFacts F;
	F.Size = F.Day = F.Year = F.HardWinters = F.ColdDeaths = 0xFFFFFFFEu;
	F.Seed = 0xFFFFFFFFFFFFull;
	F.Season = 3; // "autumn", the longest name
	F.Stats.Tiles = F.Stats.Frost = F.Stats.Growing = 0xFFFFFFFFu;
	F.Stats.Coldest = F.Stats.Warmest = INT32_MIN;
	F.Stats.Digest = ~0ull;
	const std::string Line = Composed(F);
	VT_CHECK_EQ(Line, AsTheAtlasPrintedIt(F));
	VT_CHECK(Line.size() + 1u <= ClimateLineBytes);
}

VAELEN_TEST(Proof, AllOfTheLineOrNone)
{
	const std::string Whole = Composed(Winter());
	VT_CHECK(Whole.size() > 150u);
	char Small[ClimateLineBytes];
	// One byte short of the line and its terminator: nothing, not a line cut
	// short that still starts like the right one.
	std::memset(Small, 'x', sizeof(Small));
	VT_CHECK_EQ(ClimateLine(Winter(), Small, static_cast<uint32>(Whole.size())), 0u);
	VT_CHECK_EQ(Small[0], '\0');
	// Exactly enough: the whole line.
	VT_CHECK_EQ(ClimateLine(Winter(), Small, static_cast<uint32>(Whole.size()) + 1u),
				static_cast<uint32>(Whole.size()));
	VT_CHECK_EQ(std::string(Small), Whole);
	// CONTROL: no buffer is refused, not written through.
	VT_CHECK_EQ(ClimateLine(Winter(), nullptr, 0u), 0u);
}
