// VAELEN - VaelenScene
// Phase 23 task 23.05: roofs and walls. See Town.h.
//
// STATUS: VALIDATED headless (Phase 23 task 23.05)
#include "Vaelen/Scene/Town.h"

#include "Vaelen/Scene/LineWriter.h"

#include <set>

namespace Vaelen::Scene
{
	namespace
	{
		constexpr Hash64 RidgeSalt = 0x5269646765ull; // "Ridge"

		/// Per road tile, the direction to the path's next tile (its last
		/// tile keeps the direction it came by), as 0 along X or 1 along Y.
		/// A tile on two roads keeps the first path's (the lower route).
		void RoadAxes(const Ground& G, const SceneLayout& L, std::vector<uint8>& Axis, std::vector<uint8>& IsRoad)
		{
			Axis.assign(static_cast<usize>(G.Width) * G.Height, 0u);
			IsRoad.assign(Axis.size(), 0u);
			for (const RoadPath& R : L.Roads)
			{
				for (usize I = 0; I < R.Tiles.size(); ++I)
				{
					const uint32 T = R.Tiles[I];
					if (T >= IsRoad.size() || IsRoad[T] != 0u)
					{
						continue;
					}
					const usize J = I + 1u < R.Tiles.size() ? I + 1u : (I > 0u ? I - 1u : I);
					const uint32 Next = R.Tiles[J];
					const int64 DX = int64{Next % G.Width} - int64{T % G.Width};
					const int64 DY = int64{Next / G.Width} - int64{T / G.Width};
					IsRoad[T] = 1u;
					Axis[T] = (DY < 0 ? -DY : DY) > (DX < 0 ? -DX : DX) ? 1u : 0u;
				}
			}
		}

		uint32 RidgeWith(const Ground& G, const std::vector<uint8>& Axis, const std::vector<uint8>& IsRoad,
						 const Placed& House, bool& bByRoad)
		{
			bByRoad = false;
			uint32 Tile = 0;
			if (G.Width != 0u && TileOfPoint(G, House.X, House.Y, Tile))
			{
				const int64 HX = Tile % G.Width, HY = Tile / G.Width;
				int64 Best = -1;
				uint32 BestTile = 0;
				for (int64 Y = HY - RoofReachTiles; Y <= HY + int64{RoofReachTiles}; ++Y)
				{
					for (int64 X = HX - RoofReachTiles; X <= HX + int64{RoofReachTiles}; ++X)
					{
						if (X < 0 || Y < 0 || X >= G.Width || Y >= G.Height)
						{
							continue;
						}
						const uint32 T = static_cast<uint32>(Y) * G.Width + static_cast<uint32>(X);
						if (IsRoad[T] == 0u)
						{
							continue;
						}
						const int64 D = (X - HX) * (X - HX) + (Y - HY) * (Y - HY);
						if (Best < 0 || D < Best || (D == Best && T < BestTile))
						{
							Best = D;
							BestTile = T;
						}
					}
				}
				if (Best >= 0)
				{
					bByRoad = true;
					return Axis[BestTile];
				}
			}
			return static_cast<uint32>(Mix64(HashCombine(RidgeSalt, HashUInt64(House.Key))) & 1u);
		}
	} // namespace

	uint32 RidgeOf(const Ground& G, const SceneLayout& L, const Placed& House, bool& bByRoad)
	{
		std::vector<uint8> Axis, IsRoad;
		RoadAxes(G, L, Axis, IsRoad);
		return RidgeWith(G, Axis, IsRoad, House, bByRoad);
	}

	uint32 CultureOfHouse(const View::PeopleView& People, const Placed& House)
	{
		if (House.Flags != 0u || House.Key == 0u)
		{
			return 0u;
		}
		for (const View::PersonView& P : People.People)
		{
			if (P.Family == House.Key && View::IsAlive(P))
			{
				return P.Culture;
			}
		}
		return 0u;
	}

	void LookOfTown(const Ground& G, const SceneLayout& L, const View::PeopleView& People, TownLook& Out)
	{
		Out.Day = L.Day;
		Out.Houses.clear();
		Out.Houses.reserve(L.Houses.size());
		std::vector<uint8> Axis, IsRoad;
		RoadAxes(G, L, Axis, IsRoad);
		// The families' cultures once: the lowest-indexed living member's,
		// which is the first met, the people being in index order.
		std::vector<uint32> FamilyCulture;
		for (const View::PersonView& P : People.People)
		{
			if (P.Family == 0u || !View::IsAlive(P))
			{
				continue;
			}
			if (P.Family >= FamilyCulture.size())
			{
				FamilyCulture.resize(static_cast<usize>(P.Family) + 1u, 0u);
			}
			if (FamilyCulture[P.Family] == 0u)
			{
				FamilyCulture[P.Family] = P.Culture;
			}
		}
		for (const Placed& H : L.Houses)
		{
			HouseLook Look;
			bool bByRoad = false;
			Look.RidgeAlongY = static_cast<uint8>(RidgeWith(G, Axis, IsRoad, H, bByRoad));
			Look.ByRoad = bByRoad ? 1u : 0u;
			const uint32 Culture = H.Flags != 0u || H.Key >= FamilyCulture.size() ? 0u : FamilyCulture[H.Key];
			Look.Culture = static_cast<uint16>(Culture > 0xFFFFu ? 0xFFFFu : Culture);
			Out.Houses.push_back(Look);
		}
	}

	TownStats MeasureTown(const TownLook& T)
	{
		TownStats S;
		Hash64 H = HashUInt64((uint64{T.Day} << 32) | T.Houses.size());
		std::set<uint32> Cultures;
		for (const HouseLook& L : T.Houses)
		{
			H = HashBytes(reinterpret_cast<const char*>(&L), sizeof(L), H);
			S.AlongY += L.RidgeAlongY != 0u ? 1u : 0u;
			S.ByRoad += L.ByRoad != 0u ? 1u : 0u;
			if (L.Culture != 0u)
			{
				++S.Cultured;
				Cultures.insert(L.Culture);
			}
		}
		S.Houses = static_cast<uint32>(T.Houses.size());
		S.Cultures = static_cast<uint32>(Cultures.size());
		S.Digest = H;
		return S;
	}

	uint32 TownLine(uint32 Size, uint64 Seed, uint32 Day, const TownStats& S, char* Out, uint32 Bytes)
	{
		if (Out == nullptr || Bytes == 0u)
		{
			return 0u;
		}
		Detail::LineWriter W{Out, Bytes};
		W.Put("LogVaelenScene: AELVOR ");
		W.Unsigned(Size);
		W.Put(" seed ");
		W.Hex(Seed, 12u);
		W.Put(" town day ");
		W.Unsigned(Day);
		W.Put(": houses ");
		W.Unsigned(S.Houses);
		W.Put(", ridges along y ");
		W.Unsigned(S.AlongY);
		W.Put(" (by a road ");
		W.Unsigned(S.ByRoad);
		W.Put("), of a culture ");
		W.Unsigned(S.Cultured);
		W.Put(" (");
		W.Unsigned(S.Cultures);
		W.Put(" cultures); town ");
		W.Hex(S.Digest, 16u);
		return W.Finish();
	}
} // namespace Vaelen::Scene
