// VAELEN - VaelenScene
// Phase 19 task 19.08: what the scene invents, from what the views hold. See
// Layout.h for what is invented and why each rule is the one it is.
//
// STATUS: VALIDATED headless (Phase 19 task 19.08)
#include "Vaelen/Scene/Layout.h"

#include "Vaelen/Scene/LineWriter.h"

#include <algorithm>
#include <map>
#include <queue>
#include <utility>

namespace Vaelen::Scene
{
	namespace
	{
		// Salts, so a family's house and a coarse region's k-th house never
		// draw from the same hash.
		constexpr Hash64 HouseSalt = 0x486f75736573ull;	 // "Houses"
		constexpr Hash64 CoarseSalt = 0x436f61727365ull; // "Coarse"

		/// The land tiles of every region, ascending: where anything may stand.
		/// Not the river tiles - nothing is put in water.
		std::vector<std::vector<uint32>> LandOf(const Ground& G)
		{
			uint32 Most = 0;
			for (const uint16 R : G.Region)
			{
				Most = R > Most ? R : Most;
			}
			std::vector<std::vector<uint32>> Out(static_cast<usize>(Most) + 1u);
			for (uint32 T = 0; T < G.Width * G.Height; ++T)
			{
				if (G.Region[T] != 0u && G.Kind[T] == GroundKind::Land)
				{
					Out[G.Region[T]].push_back(T);
				}
			}
			return Out;
		}

		/// A point inside the tile, Margin from its edges, from the hash's bits.
		void PointIn(const Ground& G, uint32 Tile, Hash64 H, int32 Margin, int64& X, int64& Y)
		{
			PointOfTile(G, Tile, X, Y);
			const int64 Span = G.Scale.CmPerTile - 2 * int64{Margin};
			if (Span <= 0)
			{
				return;
			}
			X += static_cast<int64>((H >> 20) % static_cast<uint64>(Span)) - Span / 2;
			Y += static_cast<int64>((H >> 42) % static_cast<uint64>(Span)) - Span / 2;
		}

		bool Level(const Ground& G, int64 X, int64 Y)
		{
			const int64 DX = int64{HeightAt(G, X + HouseHalfCm, Y)} - HeightAt(G, X - HouseHalfCm, Y);
			const int64 DY = int64{HeightAt(G, X, Y + HouseHalfCm)} - HeightAt(G, X, Y - HouseHalfCm);
			return (DX < 0 ? -DX : DX) <= HouseSlopeCm && (DY < 0 ? -DY : DY) <= HouseSlopeCm;
		}

		bool Overlaps(const std::vector<Placed>& Houses, usize From, int64 X, int64 Y)
		{
			for (usize i = From; i < Houses.size(); ++i)
			{
				const int64 DX = X - Houses[i].X;
				const int64 DY = Y - Houses[i].Y;
				if ((DX < 0 ? -DX : DX) < 2 * HouseHalfCm && (DY < 0 ? -DY : DY) < 2 * HouseHalfCm)
				{
					return true;
				}
			}
			return false;
		}

		/// One house for Key in Region, or false after HouseTries plots. Only the
		/// houses from index From on (this region's, placed before it) are in its way.
		bool PlaceHouse(const Ground& G, const std::vector<uint32>& Land, uint32 Region, uint32 Key, Hash64 Salt,
						uint32 Flags, usize From, std::vector<Placed>& Houses)
		{
			if (Land.empty())
			{
				return false;
			}
			for (uint32 Try = 0; Try < HouseTries; ++Try)
			{
				const Hash64 H = Mix64(HashCombine(HashCombine(Salt, HashUInt64(Key)), HashUInt64(Try)));
				const uint32 Tile = Land[static_cast<usize>(H % Land.size())];
				int64 X = 0, Y = 0;
				PointIn(G, Tile, H, HouseHalfCm, X, Y);
				if (!Level(G, X, Y) || Overlaps(Houses, From, X, Y))
				{
					continue;
				}
				Placed P;
				P.X = static_cast<int32>(X);
				P.Y = static_cast<int32>(Y);
				P.Z = HeightAt(G, X, Y);
				P.Region = Region;
				P.Key = Key;
				P.Flags = Flags;
				Houses.push_back(P);
				return true;
			}
			return false;
		}

		/// The land tile of a region nearest its centroid tile's centre.
		bool Middle(const Ground& G, const View::RegionView& R, uint32& Tile)
		{
			int64 X = 0, Y = 0;
			PointOfTile(G, R.CentroidTile, X, Y);
			bool Found = false;
			uint64 Best = 0;
			for (uint32 T = 0; T < G.Width * G.Height; ++T)
			{
				if (G.Region[T] != R.Index || G.Kind[T] != GroundKind::Land)
				{
					continue;
				}
				int64 CX = 0, CY = 0;
				PointOfTile(G, T, CX, CY);
				const uint64 D = static_cast<uint64>((CX - X) * (CX - X) + (CY - Y) * (CY - Y));
				if (!Found || D < Best)
				{
					Found = true;
					Best = D;
					Tile = T;
				}
			}
			return Found;
		}

		Placed At(const Ground& G, uint32 Tile, uint32 Region, uint32 Key)
		{
			int64 X = 0, Y = 0;
			PointOfTile(G, Tile, X, Y);
			Placed P;
			P.X = static_cast<int32>(X);
			P.Y = static_cast<int32>(Y);
			P.Z = HeightAt(G, X, Y);
			P.Region = Region;
			P.Key = Key;
			return P;
		}

		/// Integer A* over land and river tiles, four-connected, cost one a step,
		/// ties on the lower tile index. Empty when no land path joins them.
		std::vector<uint32> Path(const Ground& G, uint32 From, uint32 To)
		{
			const uint32 N = G.Width * G.Height;
			const auto Passable = [&](uint32 T)
			{ return G.Kind[T] == GroundKind::Land || G.Kind[T] == GroundKind::River; };
			const auto Guess = [&](uint32 T)
			{
				const int64 DX = static_cast<int64>(T % G.Width) - static_cast<int64>(To % G.Width);
				const int64 DY = static_cast<int64>(T / G.Width) - static_cast<int64>(To / G.Width);
				return static_cast<uint32>((DX < 0 ? -DX : DX) + (DY < 0 ? -DY : DY));
			};
			std::vector<uint32> Cost(N, 0xFFFFFFFFu);
			std::vector<uint32> Came(N, 0xFFFFFFFFu);
			using Entry = std::pair<uint64, uint32>; // (f << 32 | tile) for ordering, tile
			std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Open;
			Cost[From] = 0;
			Open.push({(uint64{Guess(From)} << 32) | From, From});
			while (!Open.empty())
			{
				const uint32 T = Open.top().second;
				const uint32 F = static_cast<uint32>(Open.top().first >> 32);
				Open.pop();
				if (F != Cost[T] + Guess(T))
				{
					continue; // a stale entry
				}
				if (T == To)
				{
					break;
				}
				const uint32 X = T % G.Width;
				const uint32 Y = T / G.Width;
				const uint32 Around[4] = {X + 1u < G.Width ? T + 1u : T, X > 0u ? T - 1u : T,
										  Y + 1u < G.Height ? T + G.Width : T, Y > 0u ? T - G.Width : T};
				for (const uint32 Next : Around)
				{
					if (Next == T || !Passable(Next) || Cost[T] + 1u >= Cost[Next])
					{
						continue;
					}
					Cost[Next] = Cost[T] + 1u;
					Came[Next] = T;
					Open.push({(uint64{Cost[Next] + Guess(Next)} << 32) | Next, Next});
				}
			}
			std::vector<uint32> Out;
			if (Cost[To] == 0xFFFFFFFFu)
			{
				return Out;
			}
			for (uint32 T = To; T != From; T = Came[T])
			{
				Out.push_back(T);
			}
			Out.push_back(From);
			std::reverse(Out.begin(), Out.end());
			return Out;
		}
	} // namespace

	void BuildLayout(const Ground& G, const View::WorldView& World_, const View::NetView& Net,
					 const View::PeopleView& People, const View::LifeView& Life, uint32 Day, SceneLayout& Out)
	{
		Out = SceneLayout{};
		Out.Day = Day;
		if (G.Width == 0u)
		{
			return;
		}
		const std::vector<std::vector<uint32>> Land = LandOf(G);
		const auto LandIn = [&](uint32 R) -> const std::vector<uint32>&
		{
			static const std::vector<uint32> None;
			return R < Land.size() ? Land[R] : None;
		};

		// Who lives where: the living families of each region, and its living people.
		std::map<uint32, std::vector<uint32>> Families;
		std::map<uint32, std::vector<const View::PersonView*>> Living;
		for (const View::PersonView& P : People.People)
		{
			if (!View::IsAlive(P) || P.Region == 0u)
			{
				continue;
			}
			Living[P.Region].push_back(&P);
			if (P.Family != 0u)
			{
				Families[P.Region].push_back(P.Family);
			}
		}
		for (auto& [Region, List] : Families)
		{
			std::sort(List.begin(), List.end());
			List.erase(std::unique(List.begin(), List.end()), List.end());
		}

		for (const View::RegionView& R : World_.Regions)
		{
			uint32 Tile = 0;
			if (R.Settlement != 0u && Middle(G, R, Tile))
			{
				Out.Squares.push_back(At(G, Tile, R.Index, R.Settlement));
			}
			if (View::ColonyIn(Net, R.Index) != nullptr && Middle(G, R, Tile))
			{
				Out.Pits.push_back(At(G, Tile, R.Index, R.Index));
			}
			// Houses: a family's own, in ascending family order, each yielding only
			// to the ones before it; a coarse region's, ceil(people / 5) of them.
			const usize First = Out.Houses.size();
			if (R.Detailed != 0u)
			{
				const auto Found = Families.find(R.Index);
				if (Found != Families.end())
				{
					for (const uint32 Family : Found->second)
					{
						Out.Unplaced +=
							PlaceHouse(G, LandIn(R.Index), R.Index, Family, HouseSalt, 0u, First, Out.Houses) ? 0u : 1u;
					}
				}
			}
			else
			{
				const uint32 Count = (R.People + 4u) / 5u;
				for (uint32 k = 0; k < Count; ++k)
				{
					Out.Unplaced += PlaceHouse(G, LandIn(R.Index), R.Index, k, HashCombine(CoarseSalt, R.Index), 1u,
											   First, Out.Houses)
										? 0u
										: 1u;
				}
			}
			// Figures: the living of a detailed region, the played person excepted,
			// at a slot of their own for this day.
			if (R.Detailed != 0u)
			{
				const auto Found = Living.find(R.Index);
				const std::vector<uint32>& Ground_ = LandIn(R.Index);
				if (Found != Living.end() && !Ground_.empty())
				{
					for (const View::PersonView* P : Found->second)
					{
						if (P->Index == World_.Played || P->Index == Life.Person)
						{
							continue;
						}
						const Hash64 H = Mix64(HashCombine(P->Identity, HashUInt64(Day)));
						int64 X = 0, Y = 0;
						PointIn(G, Ground_[static_cast<usize>(H % Ground_.size())], H, 50, X, Y);
						Placed F;
						F.X = static_cast<int32>(X);
						F.Y = static_cast<int32>(Y);
						F.Z = HeightAt(G, X, Y);
						F.Region = R.Index;
						F.Key = P->Index;
						for (uint32 c = 0; c < Life.CompanyCount && c < View::MostCompany; ++c)
						{
							F.Flags |= Life.Company[c].Person == P->Index ? FigureFlag::Company : 0u;
						}
						Out.Figures.push_back(F);
					}
				}
			}
		}

		for (const View::RouteView& Route : Net.Routes)
		{
			if (Route.Open == 0u)
			{
				continue;
			}
			const View::RegionView* A = View::RegionIn(World_, Route.From);
			const View::RegionView* B = View::RegionIn(World_, Route.To);
			uint32 TA = 0, TB = 0;
			if (A == nullptr || B == nullptr || !Middle(G, *A, TA) || !Middle(G, *B, TB))
			{
				++Out.Unrouted;
				continue;
			}
			RoadPath Road;
			Road.Route = Route.Index;
			Road.Tiles = Path(G, TA, TB);
			if (Road.Tiles.empty())
			{
				++Out.Unrouted;
				continue;
			}
			Out.Roads.push_back(std::move(Road));
		}
	}

	uint32 AimAt(const SceneLayout& L, int64 Xcm, int64 Ycm, int64 DirX, int64 DirY)
	{
		const int64 Dir2 = DirX * DirX + DirY * DirY;
		if (Dir2 == 0)
		{
			return 0u;
		}
		uint32 Best = 0;
		int64 BestD = 0;
		for (const Placed& F : L.Figures)
		{
			if ((F.Flags & FigureFlag::Company) == 0u)
			{
				continue;
			}
			const int64 DX = F.X - Xcm;
			const int64 DY = F.Y - Ycm;
			const int64 D2 = DX * DX + DY * DY;
			const int64 Dot = DX * DirX + DY * DirY;
			// Within reach, in front, and within 30 deg: cos^2 30 = 3/4.
			if (D2 > int64{AimReachCm} * AimReachCm || Dot <= 0 || 4 * Dot * Dot < 3 * D2 * Dir2)
			{
				continue;
			}
			if (Best == 0u || D2 < BestD || (D2 == BestD && F.Key < Best))
			{
				Best = F.Key;
				BestD = D2;
			}
		}
		return Best;
	}

	LayoutStats MeasureLayout(const SceneLayout& L)
	{
		LayoutStats S;
		Hash64 H = HashUInt64(L.Day);
		const auto Fold = [&](const std::vector<Placed>& List)
		{
			H = HashCombine(H, HashUInt64(List.size()));
			for (const Placed& P : List)
			{
				H = HashBytes(reinterpret_cast<const char*>(&P), sizeof(P), H);
			}
		};
		Fold(L.Squares);
		Fold(L.Houses);
		Fold(L.Figures);
		Fold(L.Pits);
		H = HashCombine(H, HashUInt64(L.Roads.size()));
		for (const RoadPath& R : L.Roads)
		{
			H = HashCombine(H, HashUInt64((uint64{R.Route} << 32) | R.Tiles.size()));
			for (const uint32 T : R.Tiles)
			{
				H = HashCombine(H, HashUInt64(T));
			}
			S.RoadTiles += static_cast<uint32>(R.Tiles.size());
		}
		H = HashCombine(H, HashUInt64((uint64{L.Unplaced} << 32) | L.Unrouted));
		S.Squares = static_cast<uint32>(L.Squares.size());
		S.Houses = static_cast<uint32>(L.Houses.size());
		S.Unplaced = L.Unplaced;
		S.Figures = static_cast<uint32>(L.Figures.size());
		for (const Placed& F : L.Figures)
		{
			S.Company += (F.Flags & FigureFlag::Company) != 0u ? 1u : 0u;
		}
		S.Roads = static_cast<uint32>(L.Roads.size());
		S.Unrouted = L.Unrouted;
		S.Pits = static_cast<uint32>(L.Pits.size());
		S.Digest = H;
		return S;
	}

	uint32 LayoutLine(uint32 Size, uint64 Seed, uint32 Day, const LayoutStats& S, char* Out, uint32 Bytes)
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
		W.Put(" layout day ");
		W.Unsigned(Day);
		W.Put(": squares ");
		W.Unsigned(S.Squares);
		W.Put(", houses ");
		W.Unsigned(S.Houses);
		W.Put(" (unplaced ");
		W.Unsigned(S.Unplaced);
		W.Put("), figures ");
		W.Unsigned(S.Figures);
		W.Put(" (company ");
		W.Unsigned(S.Company);
		W.Put("), roads ");
		W.Unsigned(S.Roads);
		W.Put(" over ");
		W.Unsigned(S.RoadTiles);
		W.Put(" tiles (unrouted ");
		W.Unsigned(S.Unrouted);
		W.Put("), pits ");
		W.Unsigned(S.Pits);
		W.Put("; layout ");
		W.Hex(S.Digest, 16u);
		return W.Finish();
	}
} // namespace Vaelen::Scene
