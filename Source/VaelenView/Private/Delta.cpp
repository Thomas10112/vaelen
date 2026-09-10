// VAELEN - VaelenView
// Phase 13 task 13.02: what changed since the last frame.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/edge tests in Tests/View/Test_Delta.cpp
#include "Vaelen/View/Delta.h"

#include <algorithm>
#include <cstring>

namespace Vaelen::View
{
	namespace
	{
		bool Same(const RegionView& A, const RegionView& B) noexcept
		{
			return std::memcmp(&A, &B, sizeof(RegionView)) == 0;
		}
	} // namespace

	void Diff(const WorldView& Was, const WorldView& Now, ViewDelta& Out)
	{
		Out.Changed.clear();
		Out.Gone.clear();
		Out.Tick = Now.Tick;
		Out.Year = Now.Year;
		Out.Width = Now.Width;
		Out.Height = Now.Height;
		Out.People = Now.People;
		Out.Played = Now.Played;
		Out.Reserved = 0;
		// A view of different ground is not a difference, it is a replacement.
		// Saying so is cheaper than pretending two maps can be reconciled.
		Out.Whole = (Was.Width != Now.Width || Was.Height != Now.Height || Was.Regions.empty()) ? 1u : 0u;
		if (Out.Whole != 0)
		{
			Out.Changed = Now.Regions;
			return;
		}

		// Both sides are in index order (13.01 guarantees it), so this is one
		// walk of each and not a lookup per region.
		usize i = 0;
		usize j = 0;
		while (i < Was.Regions.size() && j < Now.Regions.size())
		{
			const RegionView& A = Was.Regions[i];
			const RegionView& B = Now.Regions[j];
			if (A.Index == B.Index)
			{
				if (!Same(A, B))
				{
					Out.Changed.push_back(B);
				}
				++i;
				++j;
				continue;
			}
			if (A.Index < B.Index)
			{
				Out.Gone.push_back(A.Index);
				++i;
				continue;
			}
			Out.Changed.push_back(B); // new ground the older view never had
			++j;
		}
		for (; i < Was.Regions.size(); ++i)
		{
			Out.Gone.push_back(Was.Regions[i].Index);
		}
		for (; j < Now.Regions.size(); ++j)
		{
			Out.Changed.push_back(Now.Regions[j]);
		}
	}

	void Apply(WorldView& Onto, const ViewDelta& D)
	{
		Onto.Tick = D.Tick;
		Onto.Year = D.Year;
		Onto.Width = D.Width;
		Onto.Height = D.Height;
		Onto.People = D.People;
		Onto.Played = D.Played;
		if (D.Whole != 0)
		{
			Onto.Regions = D.Changed;
			return;
		}
		if (!D.Gone.empty())
		{
			Onto.Regions.erase(std::remove_if(Onto.Regions.begin(), Onto.Regions.end(), [&](const RegionView& R)
											  { return std::binary_search(D.Gone.begin(), D.Gone.end(), R.Index); }),
							   Onto.Regions.end());
		}
		for (const RegionView& R : D.Changed)
		{
			const auto At = std::lower_bound(Onto.Regions.begin(), Onto.Regions.end(), R.Index,
											 [](const RegionView& A, uint32 B) { return A.Index < B; });
			if (At != Onto.Regions.end() && At->Index == R.Index)
			{
				*At = R;
			}
			else
			{
				Onto.Regions.insert(At, R); // ground the older view never had, kept in order
			}
		}
	}

	DeltaStats MeasureDelta(const ViewDelta& D, const WorldView& Of)
	{
		DeltaStats Out;
		Out.Changed = static_cast<uint32>(D.Changed.size());
		Out.Gone = static_cast<uint32>(D.Gone.size());
		Out.Bytes = static_cast<uint32>(sizeof(ViewDelta) + D.Changed.size() * sizeof(RegionView) +
										D.Gone.size() * sizeof(uint32));
		Out.Whole = static_cast<uint32>(sizeof(WorldView) + Of.Regions.size() * sizeof(RegionView));
		Out.PerMille = Out.Whole == 0 ? 0u : static_cast<uint32>(uint64{Out.Bytes} * 1000u / Out.Whole);
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const RegionView& R : D.Changed)
		{
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&R), sizeof(RegionView)));
		}
		for (const uint32 G : D.Gone)
		{
			Digest = HashCombine(Digest, HashUInt64(G));
		}
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::View
