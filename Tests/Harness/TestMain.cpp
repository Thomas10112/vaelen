// VAELEN - test runner.
//
// STATUS: VALIDATED (Phase 00)
//
// Exit codes: 0 all passed, 1 failures, 2 bad arguments or registry error
// (a VAELEN_TEST suite name that does not match its Test_<Suite>.cpp file),
// 3 no test matched the filters.
#include "VaelenTest.h"

#include "Vaelen/Core/Log.h"
#include "Vaelen/Core/Version.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

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
		std::printf("Usage: VaelenCoreTests [--suite Name] [--filter Substring] [--list] [--verbose] [--quiet-log]\n"
					"                       [--reverse] [--shuffle Seed] [--check-registry]\n");
	}

	/// File stem of a test source: "/x/Tests/Core/Test_Random.cpp" -> "Random".
	void SuiteFromFile(const char* File, char* Out, std::size_t OutSize)
	{
		const char* Base = File;
		for (const char* P = File; *P != '\0'; ++P)
		{
			if (*P == '/' || *P == '\\')
			{
				Base = P + 1;
			}
		}
		if (std::strncmp(Base, "Test_", 5) == 0)
		{
			Base += 5;
		}
		std::size_t Length = std::strlen(Base);
		const char* Dot = std::strrchr(Base, '.');
		if (Dot != nullptr)
		{
			Length = static_cast<std::size_t>(Dot - Base);
		}
		if (Length >= OutSize)
		{
			Length = OutSize - 1;
		}
		std::memcpy(Out, Base, Length);
		Out[Length] = '\0';
	}

	/// Every VAELEN_TEST(Suite, ...) must live in Tests/<Module>/Test_<Suite>.cpp,
	/// otherwise no CTest entry ever selects it. Returns the number of mismatches.
	int CheckRegistry()
	{
		int Mismatches = 0;
		for (VaelenTest::TestCase* Case = VaelenTest::Registry(); Case != nullptr; Case = Case->Next)
		{
			char Expected[128];
			SuiteFromFile(Case->File, Expected, sizeof(Expected));
			if (std::strcmp(Expected, Case->Suite) != 0)
			{
				++Mismatches;
				std::fprintf(stderr, "suite/file mismatch: %s.%s is registered in %s (expected suite %s)\n",
							 Case->Suite, Case->Name, Case->File, Expected);
			}
		}
		return Mismatches;
	}

	/// Deterministic permutation (64-bit LCG) so an order-dependent test can be
	/// reproduced from the printed seed.
	void Shuffle(std::vector<VaelenTest::TestCase*>& Cases, unsigned long long Seed)
	{
		unsigned long long State = Seed * 6364136223846793005ull + 1442695040888963407ull;
		for (std::size_t i = Cases.size(); i > 1; --i)
		{
			State = State * 6364136223846793005ull + 1442695040888963407ull;
			const std::size_t j = static_cast<std::size_t>((State >> 33) % i);
			VaelenTest::TestCase* Tmp = Cases[i - 1];
			Cases[i - 1] = Cases[j];
			Cases[j] = Tmp;
		}
	}
} // namespace

int main(int Argc, char** Argv)
{
	const char* SuiteFilter = nullptr;
	const char* NameFilter = nullptr;
	bool ListOnly = false;
	bool Verbose = false;
	bool QuietLog = true;
	bool Reverse = false;
	bool DoShuffle = false;
	bool CheckRegistryOnly = false;
	unsigned long long ShuffleSeed = 0;

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
		else if (std::strcmp(Argv[i], "--shuffle") == 0 && i + 1 < Argc)
		{
			DoShuffle = true;
			ShuffleSeed = std::strtoull(Argv[++i], nullptr, 10);
		}
		else if (std::strcmp(Argv[i], "--reverse") == 0)
		{
			Reverse = true;
		}
		else if (std::strcmp(Argv[i], "--list") == 0)
		{
			ListOnly = true;
		}
		else if (std::strcmp(Argv[i], "--check-registry") == 0)
		{
			CheckRegistryOnly = true;
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

	// The registry is validated on every run: a mistyped suite name would
	// otherwise silently drop a test from CI.
	const int Mismatches = CheckRegistry();
	if (Mismatches != 0)
	{
		std::fprintf(stderr, "%d test(s) registered under a suite that does not match their file\n", Mismatches);
		return 2;
	}
	if (CheckRegistryOnly)
	{
		int Total = 0;
		for (VaelenTest::TestCase* Case = VaelenTest::Registry(); Case != nullptr; Case = Case->Next)
		{
			++Total;
		}
		std::printf("VAELEN %s tests: registry ok, %d test(s), every suite matches its file\n",
					Vaelen::GetProjectVersionString(), Total);
		return 0;
	}

	std::vector<VaelenTest::TestCase*> Selected;
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
		Selected.push_back(Case);
	}
	if (Reverse)
	{
		for (std::size_t i = 0, j = Selected.size(); i + 1 < j; ++i, --j)
		{
			VaelenTest::TestCase* Tmp = Selected[i];
			Selected[i] = Selected[j - 1];
			Selected[j - 1] = Tmp;
		}
	}
	if (DoShuffle)
	{
		Shuffle(Selected, ShuffleSeed);
		std::printf("VAELEN tests: order shuffled with seed %llu\n", ShuffleSeed);
	}

	if (Selected.empty())
	{
		std::printf("VAELEN %s tests: no test matched (suite=%s filter=%s)\n", Vaelen::GetProjectVersionString(),
					SuiteFilter ? SuiteFilter : "*", NameFilter ? NameFilter : "*");
		return 3;
	}

	if (ListOnly)
	{
		for (const VaelenTest::TestCase* Case : Selected)
		{
			std::printf("%s.%s\n", Case->Suite, Case->Name);
		}
		return 0;
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

	for (VaelenTest::TestCase* Case : Selected)
	{
		VaelenTest::Context Ctx;
		Ctx.Verbose = Verbose;
		if (Verbose)
		{
			std::printf("[ RUN  ] %s.%s\n", Case->Suite, Case->Name);
		}
		Case->Function(Ctx);
		++Run;
		TotalChecks += Ctx.Checks;
		if (Ctx.Checks == 0)
		{
			std::printf("[ WARN ] %s.%s recorded no checks\n", Case->Suite, Case->Name);
		}
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

	if (!QuietLog)
	{
		Vaelen::Log::RemoveSink(&StdioSink);
	}

	std::printf("VAELEN %s tests: %d run, %d passed, %d failed, %d checks\n", Vaelen::GetProjectVersionString(), Run,
				Run - Failed, Failed, TotalChecks);
	return Failed == 0 ? 0 : 1;
}
