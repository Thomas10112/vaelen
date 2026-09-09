// VAELEN - VaelenGameplay
// Phase 12 task 12.04: maps.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/edge tests in Tests/Gameplay
#include "Vaelen/Gameplay/Maps.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

namespace Vaelen::Gameplay
{
	namespace
	{
		constexpr uint64 MapSalt = 0x6d61707065640000ull; // "mapped"

		EntityHandle HandleOf(const World& W, const Population::PersonTypes& Persons, uint32 Person)
		{
			EntityHandle Found;
			W.Components()
				.GetPool(Persons.Person)
				.ForEach(
					[&](EntityHandle H, const Population::PersonInfo& P)
					{
						if (P.Index == Person && Found.IsNull())
						{
							Found = H;
						}
					});
			return Found;
		}

		EntityHandle MapHandle(const World& W, const MapTypes& Maps, uint32 Map)
		{
			EntityHandle Found;
			W.Components().GetPool(Maps.Map).ForEach(
				[&](EntityHandle H, const MapInfo& M)
				{
					if (M.Index == Map && Found.IsNull())
					{
						Found = H;
					}
				});
			return Found;
		}

		/// Adds a region to what somebody can name. Walked ground displaces read
		/// ground when the handful is full, because standing somewhere is worth
		/// more than reading about it - and it is the one thing a person can be
		/// sure of.
		void Learn(PersonGround& G, uint32 Region, bool Walked)
		{
			const uint16 R = static_cast<uint16>(Region);
			for (usize i = 0; i < G.Count && i < MostGround; ++i)
			{
				if (G.Known[i] == R)
				{
					G.Walked |= Walked ? (1u << i) : 0u;
					return;
				}
			}
			if (G.Count < MostGround)
			{
				G.Known[G.Count] = R;
				G.Walked |= Walked ? (1u << G.Count) : 0u;
				++G.Count;
				return;
			}
			if (!Walked)
			{
				return; // a page does not push out ground somebody has stood on
			}
			for (usize i = 0; i < MostGround; ++i)
			{
				if ((G.Walked & (1u << i)) == 0u)
				{
					G.Known[i] = R;
					G.Walked |= 1u << i;
					return;
				}
			}
		}
	} // namespace

	MapTypes MapTypes::Declare(World& W)
	{
		MapTypes T;
		T.Ground = W.Types().Register<PersonGround>("PersonGround");
		T.Map = W.Types().Register<MapInfo>("MapInfo");
		W.Components().CreatePool(T.Ground);
		W.Components().CreatePool(T.Map);
		return T;
	}

	bool CanName(const PersonGround& G, uint32 Region) noexcept
	{
		for (usize i = 0; i < G.Count && i < MostGround; ++i)
		{
			if (G.Known[i] == static_cast<uint16>(Region))
			{
				return true;
			}
		}
		return false;
	}

	bool HasWalked(const PersonGround& G, uint32 Region) noexcept
	{
		for (usize i = 0; i < G.Count && i < MostGround; ++i)
		{
			if (G.Known[i] == static_cast<uint16>(Region))
			{
				return (G.Walked & (1u << i)) != 0u;
			}
		}
		return false;
	}

	bool NoteGround(World& W, const Population::PersonTypes& Persons, const MapTypes& Maps, uint32 Person, SimTick Now)
	{
		const EntityHandle H = HandleOf(W, Persons, Person);
		if (H.IsNull())
		{
			return false;
		}
		const Population::PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(H);
		if (P == nullptr || P->Region == 0 || P->State != static_cast<uint8>(Population::LifeState::Alive))
		{
			return false;
		}
		PersonGround* G = W.Components().GetPool(Maps.Ground).TryGet(H);
		if (G == nullptr)
		{
			PersonGround Fresh;
			Fresh.Since = Now;
			W.Components().GetPool(Maps.Ground).Add(H, Fresh);
			G = W.Components().GetPool(Maps.Ground).TryGet(H);
			if (G == nullptr)
			{
				return false;
			}
		}
		Learn(*G, P->Region, true);
		return true;
	}

	const PersonGround* GroundOf(const World& W, const Population::PersonTypes& Persons, const MapTypes& Maps,
								 uint32 Person)
	{
		const EntityHandle H = HandleOf(W, Persons, Person);
		return H.IsNull() ? nullptr : W.Components().GetPool(Maps.Ground).TryGet(H);
	}

	uint32 WriteMap(World& W, const Population::PersonTypes& Persons, const MapTypes& Maps, uint32 Writer, SimTick Now)
	{
		const PersonGround* G = GroundOf(W, Persons, Maps, Writer);
		if (G == nullptr || G->Count < 2)
		{
			return 0; // one region is not a map
		}
		MapInfo M;
		M.Writer = Writer;
		M.Holder = Writer;
		M.Written = Now;
		// Only CONSECUTIVE walked ground - the crossings they actually made.
		// Known is in order of first arrival, so a person who walked A then B
		// then C knows two roads and not three: walking A to C by way of B does
		// not put A next to C, and a first version of this claimed every pair,
		// which would have drawn a false map out of an honest walk.
		for (usize i = 0; i + 1 < G->Count && i + 1 < MostGround; ++i)
		{
			const bool BothWalked = (G->Walked & (1u << i)) != 0u && (G->Walked & (1u << (i + 1))) != 0u;
			if (!BothWalked || M.Claims >= MostClaims)
			{
				continue;
			}
			M.From[M.Claims] = G->Known[i];
			M.To[M.Claims] = G->Known[i + 1];
			++M.Claims;
		}
		if (M.Claims == 0)
		{
			return 0;
		}
		uint32 Next = 0;
		W.Components().GetPool(Maps.Map).ForEach([&](EntityHandle, const MapInfo& O)
												 { Next = O.Index > Next ? O.Index : Next; });
		M.Index = Next + 1u;
		M.Identity =
			Noise::LatticeHash(W.Config().Seed ^ MapSalt, static_cast<int32>(M.Index), static_cast<int32>(Writer));
		const EntityHandle H = W.CreateEntity(IdKind::Map);
		W.Components().GetPool(Maps.Map).Add(H, M);
		W.Events().Publish(Now, MapWrittenEvent, Player::ActPayload{Writer, M.Index, M.Claims, 0u},
						   W.Entities().GetId(H));
		return M.Index;
	}

	bool ForgeClaim(World& W, const MapTypes& Maps, uint32 Map, uint32 From, uint32 To)
	{
		const EntityHandle H = MapHandle(W, Maps, Map);
		if (H.IsNull() || From == 0 || To == 0 || From == To)
		{
			return false;
		}
		MapInfo* M = W.Components().GetPool(Maps.Map).TryGet(H);
		if (M == nullptr || M->Lost != 0 || M->Claims >= MostClaims)
		{
			return false;
		}
		M->From[M->Claims] = static_cast<uint16>(From);
		M->To[M->Claims] = static_cast<uint16>(To);
		++M->Claims;
		++M->Forged;
		return true;
	}

	bool ReadMap(World& W, const Population::PersonTypes& Persons, const MapTypes& Maps, uint32 Map, uint32 Reader,
				 SimTick Now)
	{
		const EntityHandle MH = MapHandle(W, Maps, Map);
		if (MH.IsNull() || Reader == 0)
		{
			return false;
		}
		const MapInfo* M = W.Components().GetPool(Maps.Map).TryGet(MH);
		if (M == nullptr || M->Lost != 0 || M->Claims == 0)
		{
			return false;
		}
		const EntityHandle RH = HandleOf(W, Persons, Reader);
		if (RH.IsNull())
		{
			return false;
		}
		PersonGround* G = W.Components().GetPool(Maps.Ground).TryGet(RH);
		if (G == nullptr)
		{
			PersonGround Fresh;
			Fresh.Since = Now;
			W.Components().GetPool(Maps.Ground).Add(RH, Fresh);
			G = W.Components().GetPool(Maps.Ground).TryGet(RH);
			if (G == nullptr)
			{
				return false;
			}
		}
		// Every region on it, walked or forged alike. The reader has no way to
		// tell the difference, which is the whole of what a map does to a person.
		for (usize i = 0; i < M->Claims && i < MostClaims; ++i)
		{
			Learn(*G, M->From[i], false);
			Learn(*G, M->To[i], false);
		}
		W.Events().Publish(Now, MapReadEvent, Player::ActPayload{Reader, M->Index, M->Claims, M->Forged},
						   W.Entities().GetId(MH));
		return true;
	}

	const MapInfo* MapOf(const World& W, const MapTypes& Maps, uint32 Map)
	{
		const EntityHandle H = MapHandle(W, Maps, Map);
		return H.IsNull() ? nullptr : W.Components().GetPool(Maps.Map).TryGet(H);
	}

	MapCheck CheckMap(const World& W, const WorldGen::RegionGraph& Graph, const MapTypes& Maps, uint32 Map)
	{
		MapCheck Out;
		const MapInfo* M = MapOf(W, Maps, Map);
		if (M == nullptr)
		{
			return Out;
		}
		for (usize i = 0; i < M->Claims && i < MostClaims; ++i)
		{
			++Out.Claims;
			// The world reading the map back. Nothing else in twelve phases can
			// be asked whether it is true.
			(Graph.AreAdjacent(M->From[i], M->To[i]) ? Out.True_ : Out.False_) += 1u;
		}
		return Out;
	}

	MapStats MeasureMaps(const World& W, const Population::PersonTypes& Persons, const MapTypes& Maps)
	{
		(void)Persons;
		MapStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		W.Components().GetPool(Maps.Map).ForEach(
			[&](EntityHandle, const MapInfo& M)
			{
				++Out.Maps;
				Out.Claims += M.Claims;
				Out.Forged += M.Forged;
				Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&M), sizeof(MapInfo)));
			});
		W.Components()
			.GetPool(Maps.Ground)
			.ForEach(
				[&](EntityHandle, const PersonGround& G)
				{
					if (G.Count == 0)
					{
						return;
					}
					++Out.KnowGround;
					for (usize i = 0; i < G.Count && i < MostGround; ++i)
					{
						((G.Walked & (1u << i)) != 0u ? Out.Walked : Out.Read) += 1u;
					}
				});
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Gameplay
