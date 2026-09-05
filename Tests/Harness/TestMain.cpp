// VAELEN - test runner.
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Core/Version.h"

#include <cstdio>
#include <cstring>

namespace VaelenTest
{
	TestCase*& Registry()
	{
		static TestCase* Head = nullptr;
		return Head;
	}
} // namespace VaelenTest

namespace
{
	bool Contains(const char* Haystack, const char* Needle)
	{
		return Needle == nullptr || Needle[0] == '\0' || std::strstr(Haystack, Needle) != nullptr;
	}

	void PrintUsage()
	{
		std::printf("Usage: VaelenCoreTests [--suite Name] [--filter Substring] [--list] [--verbose] [--quiet-log]\n");
	}
} // namespace

int main(int Argc, char** Argv)
{
	const char* SuiteFilter = nullptr;
	const char* NameFilter = nullptr;
	bool ListOnly = false;
	bool Verbose = false;
	bool QuietLog = true;

	for (int i = 1; i < Argc; ++i)
	{
		if (std::strcmp(Argv[i], "--suite") == 0 && i + 1 < Argc)
		{
			SuiteFilter = Argv[++i];
		}
		else if (std::strcmp(Argv[i], "--filter") == 0 && i + 1 < Argc)
		{
			NameFilter = Argv[++i];
		}
		else if (std::strcmp(Argv[i], "--list") == 0)
		{
			ListOnly = true;
		}
		else if (std::strcmp(Argv[i], "--verbose") == 0)
		{
			Verbose = true;
			QuietLog = false;
		}
		else if (std::strcmp(Argv[i], "--quiet-log") == 0)
		{
			QuietLog = true;
		}
		else
		{
			PrintUsage();
			return 2;
		}
	}

	// Kernel log output is noise for test runs unless --verbose is given.
	Vaelen::StdioLogSink StdioSink;
	if (!QuietLog)
	{
		Vaelen::Log::AddSink(&StdioSink);
	}

	int Run = 0;
	int Failed = 0;
	int TotalChecks = 0;

	for (VaelenTest::TestCase* Case = VaelenTest::Registry(); Case != nullptr; Case = Case->Next)
	{
		if (SuiteFilter != nullptr && std::strcmp(Case->Suite, SuiteFilter) != 0)
		{
			continue;
		}
		if (!Contains(Case->Name, NameFilter))
		{
			continue;
		}
		if (ListOnly)
		{
			std::printf("%s.%s\n", Case->Suite, Case->Name);
			continue;
		}

		VaelenTest::Context Ctx;
		Ctx.Verbose = Verbose;
		if (Verbose)
		{
			std::printf("[ RUN  ] %s.%s\n", Case->Suite, Case->Name);
		}
		Case->Function(Ctx);
		++Run;
		TotalChecks += Ctx.Checks;
		if (Ctx.Failures > 0)
		{
			++Failed;
			std::printf("[ FAIL ] %s.%s (%d failure%s)\n", Case->Suite, Case->Name, Ctx.Failures,
						Ctx.Failures == 1 ? "" : "s");
		}
		else if (Verbose)
		{
			std::printf("[  OK  ] %s.%s (%d checks)\n", Case->Suite, Case->Name, Ctx.Checks);
		}
	}

	if (ListOnly)
	{
		return 0;
	}

	if (!QuietLog)
	{
		Vaelen::Log::RemoveSink(&StdioSink);
	}

	if (Run == 0)
	{
		std::printf("VAELEN %s tests: no test matched (suite=%s filter=%s)\n", Vaelen::GetProjectVersionString(),
					SuiteFilter ? SuiteFilter : "*", NameFilter ? NameFilter : "*");
		return 3;
	}

	std::printf("VAELEN %s tests: %d run, %d passed, %d failed, %d checks\n", Vaelen::GetProjectVersionString(), Run,
				Run - Failed, Failed, TotalChecks);
	return Failed == 0 ? 0 : 1;
}
