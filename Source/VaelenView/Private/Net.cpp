// VAELEN - VaelenView
// Phase 13 task 13.08a: what the world has BUILT, as a renderer needs it.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/deterministic tests in Tests/View/Test_Net.cpp
#include "Vaelen/View/Net.h"

#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::View
{
	void TakeNetView(const World& W, const ViewSources& From, NetView& Out)
	{
		Out.Routes.clear();
		Out.Colonies.clear();
		Out.Tick = static_cast<uint64>(W.Now());
		Out.Year = static_cast<uint32>(Out.Tick / History::TicksPerYear);
		Out.Open = 0;

		if (From.HasTrade)
		{
			W.Components()
				.GetPool(From.Trade.Route)
				.ForEach(
					[&](EntityHandle, const Economy::RouteInfo& R)
					{
						if (R.Index == 0)
						{
							return;
						}
						RouteView V;
						V.Index = R.Index;
						V.From = R.From;
						V.To = R.To;
						V.Idle = R.Idle;
						V.Openings = R.Openings;
						V.Open = R.Closed == 0 ? 1u : 0u;
						V.Carried = R.Carried;
						Out.Routes.push_back(V);
					});
			// In index order, not pool order: a renderer keeps a line per route
			// and cannot do that if the order rides on which entity was made
			// first. Same reason RegionView is sorted in 13.01.
			std::sort(Out.Routes.begin(), Out.Routes.end(),
					  [](const RouteView& A, const RouteView& B) { return A.Index < B.Index; });
			for (const RouteView& R : Out.Routes)
			{
				Out.Open += R.Open != 0 ? 1u : 0u;
			}
		}

		if (From.HasColony)
		{
			W.Components()
				.GetPool(From.Colony_.Colony)
				.ForEach(
					[&](EntityHandle, const Colony::ColonyInfo& C)
					{
						if (C.Region == 0)
						{
							return;
						}
						ColonyView V;
						V.Region = C.Region;
						V.Hands = C.Hands;
						V.Lifted = C.Lifted;
						Out.Colonies.push_back(V);
					});
			std::sort(Out.Colonies.begin(), Out.Colonies.end(),
					  [](const ColonyView& A, const ColonyView& B) { return A.Region < B.Region; });
		}
	}

	const RouteView* RouteBetween(const NetView& V, uint32 A, uint32 B)
	{
		// The kernel keeps From below To. Asking for (7, 3) and (3, 7) must give
		// the same answer, so the pair is ordered here rather than at every call
		// site - a caller that got it wrong would silently find nothing.
		const uint32 Low = A < B ? A : B;
		const uint32 High = A < B ? B : A;
		// And a pair can carry more than one route: 06.04 builds a second entity
		// when it reopens a road it closed earlier in the same tick (ADR-0120).
		// The open one is the road that is there; a closed twin is a record of a
		// road that was. Returning the first match would return whichever the
		// index order happened to put first, which is the kind of answer that is
		// right until the day it is not.
		const RouteView* Found = nullptr;
		for (const RouteView& R : V.Routes)
		{
			if (R.From != Low || R.To != High)
			{
				continue;
			}
			if (R.Open != 0)
			{
				return &R;
			}
			Found = Found == nullptr ? &R : Found;
		}
		return Found;
	}

	const RouteView* RouteOf(const NetView& V, uint32 Index)
	{
		const auto At = std::lower_bound(V.Routes.begin(), V.Routes.end(), Index,
										 [](const RouteView& A, uint32 B) { return A.Index < B; });
		return At != V.Routes.end() && At->Index == Index ? &*At : nullptr;
	}

	const ColonyView* ColonyIn(const NetView& V, uint32 Region)
	{
		const auto At = std::lower_bound(V.Colonies.begin(), V.Colonies.end(), Region,
										 [](const ColonyView& A, uint32 B) { return A.Region < B; });
		return At != V.Colonies.end() && At->Region == Region ? &*At : nullptr;
	}

	NetStats MeasureNetView(const NetView& V)
	{
		NetStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const RouteView& R : V.Routes)
		{
			++Out.Routes;
			Out.Open += R.Open != 0 ? 1u : 0u;
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&R), sizeof(RouteView)));
		}
		for (const ColonyView& C : V.Colonies)
		{
			++Out.Colonies;
			Out.Hands += C.Hands;
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&C), sizeof(ColonyView)));
		}
		Out.Bytes = static_cast<uint32>(sizeof(NetView) + V.Routes.size() * sizeof(RouteView) +
										V.Colonies.size() * sizeof(ColonyView));
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::View
