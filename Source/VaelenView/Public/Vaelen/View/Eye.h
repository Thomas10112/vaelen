// VAELEN - VaelenView
// Phase 13 task 13.03: level of detail for the eye.
//
// 01.03's SimLod says how finely the world THINKS: Full, Detailed, Aggregate,
// Statistic, World, on periods of 1, 4, 24, 720 and 8640 ticks. It answers "how
// much computation is this corner of the world worth".
//
// This is a different question with a different answer, and the whole point of
// the task is that they must not be confused. **How finely a place is SHOWN
// depends on where somebody is looking, and the world has no idea where anybody
// is looking.** A region the simulation runs once a year can be directly under
// the eye and must be drawn; a region simulated person by person can be four
// valleys off-screen and must not be drawn at all.
//
// So the eye is an INPUT to the view, not a property of the world. Nothing here
// writes anything, nothing here changes what the simulation does, and a world
// looked at from two places at once gives two views and stays one world.
//
// STATUS: PROTOTYPE (Phase 13) - unit/integration/edge tests in Tests/View/Test_Eye.cpp
#pragma once

#include "Vaelen/Core/CoreTypes.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/ViewApi.h"

namespace Vaelen::View
{
	/// Where somebody is looking, and how far.
	struct Eye
	{
		/// The region at the middle of the screen. 0 means nobody is looking
		/// anywhere in particular, and the view is the whole world - which is
		/// what 13.01 did and stays the default.
		uint32 Region = 0;
		/// How many borders out the eye reaches. 0 with a Region set means one
		/// region only.
		uint32 Reach = 0;
		/// The most regions to put in a frame, 0 for no limit. A budget, and the
		/// nearest ground is what survives it.
		uint32 Most = 0;
	};

	/// How finely one region is drawn, which is what the renderer switches on.
	enum class Grain : uint32
	{
		Near = 0,	///< under the eye: everything the view knows
		Far = 1,	///< in sight but not the subject: the ground and the heads, nothing finer
		Unseen = 2, ///< not in the frame at all
	};
	VAELEN_VIEW_API const char* GrainName(Grain G) noexcept;

	/// Takes a frame for somebody looking from `At`. The regions come out in
	/// index order as always, and each carries its own `Grain_` so a renderer
	/// can switch on it.
	///
	/// The cache belongs to the caller, the way 10.05's Doings holds its own:
	/// building the region graph is a walk of the whole map and a frame is taken
	/// sixty times a second, so the choice is made where somebody can see it
	/// rather than hidden in a static.
	VAELEN_VIEW_API void TakeViewFor(const World& W, const ViewSources& From, const Eye& At,
									 WorldGen::RegionGraphCache& Ways, WorldView& Out);

	/// How far one region is from another across borders, or `Unreached` when no
	/// chain of borders joins them. Public because "how far is that" is a
	/// question a renderer asks about things other than drawing.
	inline constexpr uint32 Unreached = 0xFFFFFFFFu;
	VAELEN_VIEW_API uint32 BordersBetween(const World& W, const History::PreHistoryTypes& Types,
										  WorldGen::RegionGraphCache& Ways, uint32 From, uint32 To);

	struct EyeStats
	{
		uint32 Seen = 0;   ///< regions in the frame
		uint32 Near_ = 0;  ///< of them, drawn finely
		uint32 Far_ = 0;   ///< of them, drawn coarsely
		uint32 Unseen = 0; ///< regions of the world left out
		uint32 Bytes = 0;
		Hash64 Digest = 0;
	};
	VAELEN_VIEW_API EyeStats MeasureEye(const WorldView& V, uint32 OfRegions);
} // namespace Vaelen::View
