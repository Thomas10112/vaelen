// VAELEN - Phase 19 task 19.07: the body is not an input (ADR-0155).
//
// An engine-free host that walks the way the engine's walker will: a body
// that wanders inside the fence of the played region, M pressed at the fence
// facing a region the life lists as Near, the page's Press before the door's
// Mean (VaelenPlayerController.cpp's order), the look taken from the region
// under the body and the day turned (VaelenWorldSubsystem.cpp's order), and the
// body put back after every day. What reaches the door is integers only, so:
//   - two walks with the same crossings and different wanderings record the
//     SAME stream, byte for byte;
//   - a crossing the fence refuses (a region not Near) records nothing - and
//     a host that sent it anyway would record it and have the world refuse
//     it TooFar at the turn, so the fence is what keeps it out of the stream;
//   - every look recorded is that day's region of the life;
//   - the walk replays with no position to the world the host recorded.
#include "Vaelen/Scene/Fence.h"
#include "Vaelen/Scene/Terrain.h"
#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Attention.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Panel.h"
#include "Vaelen/View/Take.h"
#include "Vaelen/Player/Stream.h"
#include "Vaelen/Sim/Regions.h"
#include "Vaelen/Core/Log.h"
#include "Vaelen/Core/Random.h"
#include "VaelenTest.h"

#include <string>
#include <vector>

using namespace Vaelen;
using namespace Vaelen::Run;
using namespace Vaelen::Scene;

namespace
{
	VAELEN_DEFINE_LOG_CATEGORY(LogRunWalk);

	struct Walked
	{
		std::string Tape;		   ///< the stream, encoded
		Hash64 State = 0;		   ///< the world the host reached
		ReplayReport Back;		   ///< the stream replayed into a fresh world
		uint32 Crossings = 0;	   ///< days whose turn changed the life's region by a Move
		uint32 Asked = 0;		   ///< Moves sent through the door
		uint32 PutBack = 0;		   ///< times the body was put back after a day
		uint32 LooksOnLife = 0;	   ///< looks equal to that day's region of the life
		uint32 Looks = 0;		   ///< looks taken
		uint32 NotNearRefused = 0; ///< non-Near borders the fence refused
		uint32 NotNearSeen = 0;	   ///< borders with a non-Near region faced
		uint32 WorldTooFar = 0;	   ///< days whose turn ended with the life's last refusal TooFar
	};

	Options WalkOptions()
	{
		Options O;
		O.Size = 128;
		O.Years = 100;
		O.Play = true;
		O.Stream = true;
		return O; // the climate world, the default since 18.10
	}

	/// A border tile of the life's region with a walkable neighbour in another
	/// region: the body stands on it, facing the neighbour. WantNear picks a
	/// neighbour listed in Near, or one that is not.
	bool FindBorder(const Ground& G, const View::LifeView& Life, bool WantNear, int64& BX, int64& BY, int64& AX,
					int64& AY)
	{
		for (uint32 T = 0; T < G.Width * G.Height; ++T)
		{
			if (!IsWalkable(G, T, Life.Region))
			{
				continue;
			}
			const uint32 X = T % G.Width;
			const uint32 Y = T / G.Width;
			const uint32 Around[4] = {X + 1u < G.Width ? T + 1u : T, X > 0u ? T - 1u : T,
									  Y + 1u < G.Height ? T + G.Width : T, Y > 0u ? T - G.Width : T};
			for (const uint32 N : Around)
			{
				const uint32 There = G.Region[N];
				if (N == T || There == 0u || There == Life.Region || !IsWalkable(G, N, There))
				{
					continue;
				}
				bool Listed = false;
				for (uint32 i = 0; i < Life.NearCount && i < View::MostNear; ++i)
				{
					Listed = Listed || Life.Near[i] == There;
				}
				if (Listed == WantNear)
				{
					PointOfTile(G, T, BX, BY);
					PointOfTile(G, N, AX, AY);
					return true;
				}
			}
		}
		return false;
	}

	enum class NotNear
	{
		Never, ///< never faces a region the life does not list
		Face,  ///< faces one every crossing day, and the fence refuses it
		Send,  ///< faces one and sends the Move anyway, as a host with no fence would
	};

	/// The host's loop. Wander seeds only the body's steps inside the fence.
	Walked Walk(uint64 Wander, NotNear Mode, uint32 Days)
	{
		Walked Out;
		const Options O = WalkOptions();
		Player::StartRules Rules;
		Rules.WantBound = 0;
		Aelvor A(O);
		if (!A.Begin())
		{
			return Out;
		}
		View::MapView Map;
		View::TakeMapView(A.Instance(), A.Sources(), Map);
		Ground G;
		BuildGround(Map, SceneScale{}, G);
		Door D(A, Rules);
		if (D.TakeUp() == 0u)
		{
			return Out;
		}
		WorldGen::RegionGraphCache Ways;
		RandomStream Steps(Wander);
		const int64 Step = G.Scale.CmPerTile / 3;

		View::LifeView Life;
		View::TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
		int64 BX = 0, BY = 0;
		PlaceAfterDay(G, Life.Region, BX, BY);
		for (uint32 Day = 0; Day < Days; ++Day)
		{
			// The walker wanders: steps that would leave the fence are not taken.
			for (uint32 s = 0; s < 4u; ++s)
			{
				const int64 NX = BX + static_cast<int64>(Steps.NextU32() % static_cast<uint32>(2 * Step + 1)) - Step;
				const int64 NY = BY + static_cast<int64>(Steps.NextU32() % static_cast<uint32>(2 * Step + 1)) - Step;
				if (Inside(G, Life.Region, NX, NY))
				{
					BX = NX;
					BY = NY;
				}
			}
			// Every seventh day: to the fence, facing a Near region, and M.
			if (Day % 7u == 3u && Life.Alive != 0u)
			{
				View::WorldView Frame;
				View::ChronicleView Told;
				View::PanelView Page;
				View::TakeView(A.Instance(), A.Sources(), Frame);
				View::TakeChronicleView(A.Instance(), A.Sources(), Told);
				View::TakePanel(Frame, Life, Told, Page);
				int64 AX = 0, AY = 0;
				if (Mode != NotNear::Never && FindBorder(G, Life, false, BX, BY, AX, AY))
				{
					++Out.NotNearSeen;
					const Crossing C = CrossingOf(G, Life, AX, AY);
					Out.NotNearRefused += C.Why == CrossingWhy::NotNear ? 1u : 0u;
					// The page does not know which regions are Near - Press checks
					// the verb and the hours, not the target (Panel.cpp). So a host
					// without the fence gets None here and the world's TooFar later.
					Player::PlayerCommand Cmd;
					if (Mode == NotNear::Send &&
						View::Press(Page, Player::Intent::Move, RegionAt(G, AX, AY), 1, Cmd) == Player::Refusal::None)
					{
						D.Mean(Cmd);
					}
				}
				if (FindBorder(G, Life, true, BX, BY, AX, AY))
				{
					const Crossing C = CrossingOf(G, Life, AX, AY);
					Player::PlayerCommand Cmd;
					if (C.Why == CrossingWhy::Crossing &&
						View::Press(Page, Player::Intent::Move, C.Region, 1, Cmd) == Player::Refusal::None)
					{
						D.Mean(Cmd);
						++Out.Asked;
					}
				}
			}
			// The look from the body, then the day - the subsystem's order,
			// skipping a tick that already carries a taking (15.10's defect).
			const Player::InputStream& Tape = D.Stream();
			const bool TookHere = !Tape.Takings.empty() && Tape.Takings.back().Tick == A.Now();
			if (!TookHere)
			{
				const uint32 Looked = RegionAt(G, BX, BY);
				Out.LooksOnLife += Looked == Life.Region ? 1u : 0u;
				++Out.Looks;
				D.Look(Attention{Looked, 1u, 4u});
			}
			const uint32 Before = Life.Region;
			const uint32 Who = Life.Person;
			D.Day();
			const uint32 RefusedBefore = Life.Refused;
			View::TakeLifeView(A.Instance(), A.Sources(), Ways, Life);
			Out.Crossings += Life.Person == Who && Life.Region != Before ? 1u : 0u;
			Out.WorldTooFar += Life.Person == Who && Life.Refused > RefusedBefore &&
									   Life.LastRefusal == static_cast<uint32>(Player::Refusal::TooFar)
								   ? 1u
								   : 0u;
			Out.PutBack += PlaceAfterDay(G, Life.Region, BX, BY) ? 1u : 0u;
		}
		Out.Tape = Player::EncodeStream(D.Stream());
		Out.State = A.StateDigest();
		Aelvor Fresh(O);
		if (Fresh.Begin())
		{
			Out.Back = Replay(Fresh, D.Stream(), Rules);
		}
		return Out;
	}
} // namespace

VAELEN_TEST(Walk, TheBodyIsNotAnInput)
{
	const uint32 Days = 90;
	const Walked One = Walk(0x19070001ull, NotNear::Never, Days);
	VT_REQUIRE(!One.Tape.empty());

	// The walk crossed, and every crossing was the world's doing at the day turn.
	VT_CHECK(One.Asked >= 3u);
	VT_CHECK(One.Crossings >= 2u);
	VT_CHECK(One.Crossings <= One.Asked);
	// Every look was the life's region: a level camera's "nowhere" never happens.
	VT_CHECK_EQ(One.LooksOnLife, One.Looks);
	VT_CHECK(One.Looks >= Days - 4u);

	// It replays, with no position anywhere, to the world the host recorded.
	VT_CHECK_EQ(One.Back.Refused, 0u);
	VT_CHECK_EQ(One.Back.Wrong, 0u);
	VT_CHECK_EQ(One.Back.Days, Days);
	VT_CHECK_DIGEST_EQ(One.Back.State, One.State);

	// CONTROL (a): another wandering, the same crossings - the same stream, byte for byte.
	const Walked Two = Walk(0x19070002ull, NotNear::Never, Days);
	VT_CHECK_EQ(Two.Asked, One.Asked);
	VT_CHECK(Two.Tape == One.Tape);
	VT_CHECK_DIGEST_EQ(Two.State, One.State);

	// CONTROL (b): facing a region the life does not list, the fence refuses and
	// nothing reaches the door - the stream is the walk that never tried.
	const Walked Tried = Walk(0x19070001ull, NotNear::Face, Days);
	VT_CHECK(Tried.NotNearSeen > 0u);
	VT_CHECK_EQ(Tried.NotNearRefused, Tried.NotNearSeen);
	VT_CHECK(Tried.Tape == One.Tape);
	VT_CHECK_EQ(Tried.WorldTooFar, 0u);

	// ...and the fence is what does it. The same walk by a host that sends the
	// Move anyway records it, and the world refuses it TooFar at the turn.
	const Walked Sent = Walk(0x19070001ull, NotNear::Send, Days);
	VT_CHECK(Sent.Tape != One.Tape);
	VT_CHECK(Sent.WorldTooFar > 0u);

	VAELEN_LOG_INFO(
		LogRunWalk,
		"%u days on foot: %u Moves sent, %u crossings, %u put back, %u looks all on the life's region; "
		"%u non-Near borders refused by the fence (%u TooFar from the world when sent anyway); state %016llx",
		Days, One.Asked, One.Crossings, One.PutBack, One.Looks, Tried.NotNearRefused, Sent.WorldTooFar,
		static_cast<unsigned long long>(One.State));
}
