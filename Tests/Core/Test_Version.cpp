// VAELEN - VaelenCore tests
// Project version (fields versus the VAELEN_VERSION_* macros, the
// "MAJOR.MINOR.PATCH" string, static storage, equality) and the save format
// version (macro agreement, >= 1, constexpr).
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Core/Version.h"

#include <cstdio>
#include <cstring>
#include <type_traits>

using Vaelen::GetProjectVersion;
using Vaelen::GetProjectVersionString;
using Vaelen::GetSaveFormatVersion;
using Vaelen::ProjectVersion;
using Vaelen::uint16;
using Vaelen::uint32;
using Vaelen::usize;

namespace
{
	// Parses "A.B.C" (decimal digits only, exactly three components); returns
	// false for any other shape. Hand-written so no scanf leniency slips in.
	bool ParseVersionString(const char* Text, uint32 (&Out)[3]) noexcept
	{
		usize Part = 0;
		uint32 Value = 0;
		bool HaveDigit = false;
		for (const char* P = Text;; ++P)
		{
			const char C = *P;
			if (C >= '0' && C <= '9')
			{
				Value = Value * 10u + static_cast<uint32>(C - '0');
				HaveDigit = true;
			}
			else if (C == '.' || C == '\0')
			{
				if (!HaveDigit || Part >= 3)
				{
					return false;
				}
				Out[Part++] = Value;
				Value = 0;
				HaveDigit = false;
				if (C == '\0')
				{
					break;
				}
			}
			else
			{
				return false;
			}
		}
		return Part == 3;
	}
} // namespace

// ── Compile-time facts ──────────────────────────────────────────────────────

static_assert(VAELEN_VERSION_MAJOR >= 0 && VAELEN_VERSION_MAJOR <= 65535, "Major must fit ProjectVersion::Major");
static_assert(VAELEN_VERSION_MINOR >= 0 && VAELEN_VERSION_MINOR <= 65535, "Minor must fit ProjectVersion::Minor");
static_assert(VAELEN_VERSION_PATCH >= 0 && VAELEN_VERSION_PATCH <= 65535, "Patch must fit ProjectVersion::Patch");

static_assert(std::is_same_v<decltype(GetSaveFormatVersion()), uint32>);
static_assert(GetSaveFormatVersion() == VAELEN_SAVE_FORMAT_VERSION, "GetSaveFormatVersion must return the macro");
static_assert(GetSaveFormatVersion() >= 1, "Save format versions start at 1; 0 is reserved for 'no save'");
static_assert(VAELEN_SAVE_FORMAT_VERSION >= 1);

static_assert(std::is_same_v<decltype(ProjectVersion::Major), uint16>);
static_assert(std::is_same_v<decltype(ProjectVersion::Minor), uint16>);
static_assert(std::is_same_v<decltype(ProjectVersion::Patch), uint16>);
static_assert(std::is_trivially_copyable_v<ProjectVersion>);
static_assert(std::is_standard_layout_v<ProjectVersion>);
static_assert(ProjectVersion{} == ProjectVersion{}, "operator== must be constexpr");
static_assert(ProjectVersion{}.Major == VAELEN_VERSION_MAJOR);
static_assert(ProjectVersion{}.Minor == VAELEN_VERSION_MINOR);
static_assert(ProjectVersion{}.Patch == VAELEN_VERSION_PATCH);

// ── Project version ─────────────────────────────────────────────────────────

VAELEN_TEST(Version, FieldsMatchMacros)
{
	const ProjectVersion V = GetProjectVersion();
	VT_CHECK_EQ(V.Major, static_cast<uint16>(VAELEN_VERSION_MAJOR));
	VT_CHECK_EQ(V.Minor, static_cast<uint16>(VAELEN_VERSION_MINOR));
	VT_CHECK_EQ(V.Patch, static_cast<uint16>(VAELEN_VERSION_PATCH));
}

VAELEN_TEST(Version, DefaultConstructedEqualsCompiledIn)
{
	const ProjectVersion Compiled = GetProjectVersion();
	const ProjectVersion Default{};
	VT_CHECK(Compiled == Default);
	VT_CHECK(!(Compiled != Default));

	// Every field participates in equality.
	ProjectVersion Major = Compiled;
	Major.Major = static_cast<uint16>(Major.Major + 1u);
	ProjectVersion Minor = Compiled;
	Minor.Minor = static_cast<uint16>(Minor.Minor + 1u);
	ProjectVersion Patch = Compiled;
	Patch.Patch = static_cast<uint16>(Patch.Patch + 1u);
	VT_CHECK(Major != Compiled);
	VT_CHECK(Minor != Compiled);
	VT_CHECK(Patch != Compiled);
	VT_CHECK(Major != Minor);

	// Repeated calls agree.
	VT_CHECK(GetProjectVersion() == Compiled);
}

VAELEN_TEST(Version, StringMatchesMacros)
{
	char Expected[64];
	const int Written = std::snprintf(Expected, sizeof(Expected), "%d.%d.%d", VAELEN_VERSION_MAJOR,
									  VAELEN_VERSION_MINOR, VAELEN_VERSION_PATCH);
	VT_REQUIRE(Written > 0 && static_cast<usize>(Written) < sizeof(Expected));

	const char* Actual = GetProjectVersionString();
	VT_REQUIRE(Actual != nullptr);
	VT_CHECK_STREQ(Actual, Expected);
	VT_CHECK_EQ(std::strlen(Actual), static_cast<usize>(Written));
}

VAELEN_TEST(Version, StringMatchesFields)
{
	const char* Text = GetProjectVersionString();
	VT_REQUIRE(Text != nullptr);

	uint32 Parts[3] = {};
	VT_REQUIRE(ParseVersionString(Text, Parts));

	const ProjectVersion V = GetProjectVersion();
	VT_CHECK_EQ(Parts[0], static_cast<uint32>(V.Major));
	VT_CHECK_EQ(Parts[1], static_cast<uint32>(V.Minor));
	VT_CHECK_EQ(Parts[2], static_cast<uint32>(V.Patch));

	// Exactly two dots, nothing but digits and dots, no surrounding whitespace.
	usize Dots = 0;
	for (const char* P = Text; *P != '\0'; ++P)
	{
		const bool Digit = *P >= '0' && *P <= '9';
		VT_CHECK_MSG(Digit || *P == '.', "unexpected character 0x%02x in \"%s\"", static_cast<unsigned>(*P) & 0xffu,
					 Text);
		if (*P == '.')
		{
			++Dots;
		}
	}
	VT_CHECK_EQ(Dots, usize{2});
	VT_CHECK(Text[0] != '.');
	VT_CHECK(Text[std::strlen(Text) - 1] != '.');
}

VAELEN_TEST(Version, StringHasStaticStorage)
{
	// Documented as static storage: the pointer is stable and the content does
	// not change between calls (callers may keep it for the process lifetime).
	const char* First = GetProjectVersionString();
	const char* Second = GetProjectVersionString();
	VT_REQUIRE(First != nullptr);
	VT_CHECK(First == Second);
	VT_CHECK(First[0] != '\0');

	char Copy[64];
	std::snprintf(Copy, sizeof(Copy), "%s", First);
	VT_CHECK_STREQ(GetProjectVersionString(), Copy);
}

VAELEN_TEST(Version, ParserRejectsMalformedStrings)
{
	// Guards the helper used above so a lenient parser cannot mask a bad string.
	uint32 Parts[3] = {};
	VT_CHECK(ParseVersionString("0.0.1", Parts));
	VT_CHECK_EQ(Parts[0], 0u);
	VT_CHECK_EQ(Parts[1], 0u);
	VT_CHECK_EQ(Parts[2], 1u);
	VT_CHECK(ParseVersionString("12.345.6789", Parts));
	VT_CHECK_EQ(Parts[0], 12u);
	VT_CHECK_EQ(Parts[1], 345u);
	VT_CHECK_EQ(Parts[2], 6789u);
	VT_CHECK(!ParseVersionString("", Parts));
	VT_CHECK(!ParseVersionString("1", Parts));
	VT_CHECK(!ParseVersionString("1.2", Parts));
	VT_CHECK(!ParseVersionString("1.2.3.4", Parts));
	VT_CHECK(!ParseVersionString("1..3", Parts));
	VT_CHECK(!ParseVersionString(".1.2", Parts));
	VT_CHECK(!ParseVersionString("1.2.", Parts));
	VT_CHECK(!ParseVersionString("1.2.3 ", Parts));
	VT_CHECK(!ParseVersionString("(1).2.3", Parts));
	VT_CHECK(!ParseVersionString("1.2.3-rc1", Parts));
}

// ── Save format version ─────────────────────────────────────────────────────

VAELEN_TEST(Version, SaveFormatVersionMatchesMacro)
{
	constexpr uint32 Constexpr = GetSaveFormatVersion(); // must be usable in a constant expression
	VT_CHECK_EQ(Constexpr, static_cast<uint32>(VAELEN_SAVE_FORMAT_VERSION));
	VT_CHECK_EQ(GetSaveFormatVersion(), static_cast<uint32>(VAELEN_SAVE_FORMAT_VERSION));
	VT_CHECK(GetSaveFormatVersion() >= 1u);
	VT_CHECK_EQ(GetSaveFormatVersion(), GetSaveFormatVersion());
}
