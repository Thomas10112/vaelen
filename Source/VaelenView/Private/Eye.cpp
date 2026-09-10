// VAELEN - VaelenView
// Phase 13 task 13.03: level of detail for the eye.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/edge tests in Tests/View/Test_Eye.cpp
#include "Vaelen/View/Eye.h"

#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <vector>

namespace Vaelen::View
{
	namespace
	{
		/// Borders out from one region, breadth first. Returns a distance per
		/// region index, `Unreached` where no chain of borders joins them - an
		/// island is not a long way away, it is not reachable, and a renderer
		/// wants to know the difference.
		void Spread(const WorldGen::RegionGraph& Graph, uint32 From, uint32 Limit, std::vector<uint32>& Out)
		{
			Out.assign(Graph.Neighbours.size(), Unreached);
			if (From == 0 || From >= Graph.Neighbours.size())
			{
				return;
			}
			Out[From] = 0;
			std::vector<uint32> Wave{From};
			std::vector<uint32> Next;
			uint32 Step = 0;
			while (!Wave.empty() && Step < Limit)
			{
				++Step;
				Next.clear();
				for (const uint32 R : Wave)
				{
					for (const uint16 N : Graph.Neighbours[R])
					{
						if (N != 0 && N < Out.size() && Out[N] == Unreached)
						{
							Out[N] = Step;
							Next.push_back(N);
						}
					}
				}
				// Sorted, so the walk is the same on every machine even though
				// the answer would be the same either way. Cheap, and it keeps
				// the habit.
				std::sort(Next.begin(), Next.end());
				Wave.swap(Next);
			}
		}
	} // namespace

	const char* GrainName(Grain G) noexcept
	{
		switch (G)
		{
		case Grain::Near:
			return "near";
		case Grain::Far:
			return "far";
		case Grain::Unseen:
			return "unseen";
		default:
			return "unknown";
		}
	}

	uint32 BordersBetween(const World& W, const History::PreHistoryTypes& Types, WorldGen::RegionGraphCache& Ways,
						  uint32 From, uint32 To)
	{
		if (From == 0 || To == 0)
		{
			return Unreached;
		}
		if (From == To)
		{
			return 0;
		}
		const WorldGen::RegionGraph& Graph = Ways.Of(W.Map(), Types.World.Regions);
		std::vector<uint32> Far;
		Spread(Graph, From, Unreached, Far);
		return To < Far.size() ? Far[To] : Unreached;
	}

	void TakeViewFor(const World& W, const ViewSources& From, const Eye& At, WorldGen::RegionGraphCache& Ways,
					 WorldView& Out)
	{
		TakeView(W, From, Out);
		if (At.Region == 0)
		{
			return; // nobody is looking anywhere in particular: the whole world, as 13.01 gives it
		}

		const WorldGen::RegionGraph& Graph = Ways.Of(W.Map(), From.Types.World.Regions);
		std::vector<uint32> Far;
		Spread(Graph, At.Region, At.Reach, Far);

		// The grain first, then the budget. A budget that cut before the grain
		// was worked out would drop near ground to keep far ground it happened
		// to reach first.
		for (RegionView& R : Out.Regions)
		{
			const uint32 Steps = R.Index < Far.size() ? Far[R.Index] : Unreached;
			if (Steps == Unreached)
			{
				R.Grain_ = static_cast<uint32>(Grain::Unseen);
			}
			else if (Steps == 0)
			{
				R.Grain_ = static_cast<uint32>(Grain::Near);
			}
			else
			{
				R.Grain_ = static_cast<uint32>(Grain::Far);
			}
		}
		Out.Regions.erase(std::remove_if(Out.Regions.begin(), Out.Regions.end(), [](const RegionView& R)
										 { return R.Grain_ == static_cast<uint32>(Grain::Unseen); }),
						  Out.Regions.end());

		if (At.Most != 0 && Out.Regions.size() > At.Most)
		{
			// Nearest first, and by index within a ring, so a budget that cuts
			// mid-ring cuts the same way every frame and the screen does not
			// flicker between two regions of equal distance.
			std::stable_sort(Out.Regions.begin(), Out.Regions.end(),
							 [&](const RegionView& A, const RegionView& B)
							 {
								 const uint32 DA = A.Index < Far.size() ? Far[A.Index] : Unreached;
								 const uint32 DB = B.Index < Far.size() ? Far[B.Index] : Unreached;
								 return DA != DB ? DA < DB : A.Index < B.Index;
							 });
			Out.Regions.resize(At.Most);
			std::sort(Out.Regions.begin(), Out.Regions.end(),
					  [](const RegionView& A, const RegionView& B) { return A.Index < B.Index; });
		}

		// The header counts what is in the FRAME, not what is in the world: a
		// renderer showing 40000 people while drawing four regions would be
		// lying about both.
		Out.People = 0;
		for (const RegionView& R : Out.Regions)
		{
			Out.People += R.People;
		}
	}

	EyeStats MeasureEye(const WorldView& V, uint32 OfRegions)
	{
		EyeStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		for (const RegionView& R : V.Regions)
		{
			++Out.Seen;
			Out.Near_ += R.Grain_ == static_cast<uint32>(Grain::Near) ? 1u : 0u;
			Out.Far_ += R.Grain_ == static_cast<uint32>(Grain::Far) ? 1u : 0u;
			Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&R), sizeof(RegionView)));
		}
		Out.Unseen = OfRegions > Out.Seen ? OfRegions - Out.Seen : 0u;
		Out.Bytes = static_cast<uint32>(sizeof(WorldView) + V.Regions.size() * sizeof(RegionView));
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::View
