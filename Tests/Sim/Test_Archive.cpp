// VAELEN - VaelenSim tests
// Archive: symmetric write/read of scalars and vectors, bounds errors, zero-fill.
//
// STATUS: VALIDATED
#include "VaelenTest.h"

#include "Vaelen/Sim/Archive.h"

#include <vector>

using namespace Vaelen;

namespace
{
	enum class Colour : uint8
	{
		Red = 1,
		Blue = 2
	};
	struct Sample
	{
		uint32 A = 0;
		uint64 B = 0;
		double C = 0.0;
		Colour D = Colour::Red;
		std::vector<uint16> E;

		void Serialize(IArchive& Ar)
		{
			Ar << A << B << C << D;
			SerializeVector(Ar, E);
		}
	};
} // namespace

VAELEN_TEST(Archive, ScalarsAndVectorsRoundTrip)
{
	Sample In;
	In.A = 7;
	In.B = 0x0123456789abcdefull;
	In.C = -2.5;
	In.D = Colour::Blue;
	In.E = {1, 2, 3, 65535};

	std::vector<uint8> Bytes;
	MemoryWriter W(Bytes);
	VT_CHECK(W.IsSaving() && !W.IsLoading());
	In.Serialize(W);
	VT_CHECK_EQ(Bytes.size(), 4u + 8u + 8u + 1u + 8u + 4u * 2u);
	VT_CHECK_EQ(W.BytesWritten(), Bytes.size());

	Sample Out;
	MemoryReader R(Bytes.data(), Bytes.size());
	VT_CHECK(R.IsLoading());
	Out.Serialize(R);
	VT_CHECK(!R.HasError());
	VT_CHECK(R.AtEnd());
	VT_CHECK_EQ(Out.A, In.A);
	VT_CHECK_EQ(Out.B, In.B);
	VT_CHECK(Out.C == In.C);
	VT_CHECK(Out.D == In.D);
	VT_CHECK(Out.E == In.E);
}

VAELEN_TEST(Archive, ReadingPastTheEndSetsTheErrorAndZeroFills)
{
	std::vector<uint8> Bytes = {1, 2, 3};
	MemoryReader R(Bytes.data(), Bytes.size());
	uint16 First = 0;
	R << First;
	VT_CHECK(!R.HasError());
	VT_CHECK_EQ(R.Remaining(), 1u);

	uint32 Second = 0xffffffffu;
	R << Second;
	VT_CHECK(R.HasError());
	VT_CHECK_EQ(Second, 0u);	   // zero-filled, never partially read
	VT_CHECK_EQ(R.Position(), 2u); // the failed read consumed nothing

	// Every later read fails too, even one that would fit.
	uint8 Third = 9;
	VT_CHECK(!R.SerializeBytes(&Third, 1));
	VT_CHECK_EQ(Third, 0u);
	VT_CHECK(!R.AtEnd());
}

VAELEN_TEST(Archive, VectorCountAboveTheLimitIsRejected)
{
	std::vector<uint8> Bytes;
	{
		MemoryWriter W(Bytes);
		uint64 Count = 1000;
		W << Count;
	}
	std::vector<uint32> Values = {5};
	MemoryReader R(Bytes.data(), Bytes.size());
	VT_CHECK(!SerializeVector(R, Values, 999));
	VT_CHECK(Values.empty());

	// A truncated element area is an error as well.
	MemoryReader R2(Bytes.data(), Bytes.size());
	std::vector<uint32> Values2;
	VT_CHECK(!SerializeVector(R2, Values2));
	VT_CHECK(R2.HasError());
}

VAELEN_TEST(Archive, EmptyVectorAndZeroSizedWritesAreExact)
{
	std::vector<uint8> Bytes;
	MemoryWriter W(Bytes);
	std::vector<uint64> Empty;
	VT_CHECK(SerializeVector(W, Empty));
	VT_CHECK(W.SerializeBytes(nullptr, 0));
	VT_CHECK_EQ(Bytes.size(), 8u);

	MemoryReader R(Bytes.data(), Bytes.size());
	std::vector<uint64> Loaded = {1, 2};
	VT_CHECK(SerializeVector(R, Loaded));
	VT_CHECK(Loaded.empty());
	VT_CHECK(R.SerializeBytes(nullptr, 0));
	VT_CHECK(R.AtEnd());
}
