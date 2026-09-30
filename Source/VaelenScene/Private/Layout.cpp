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

		/// At most 20 degrees across the house's 8 m along either axis AND
		/// along either diagonal (the review of 2026-09-27: along the axes
		/// alone, a house stood on a 27 degree diagonal while the roadmap
		/// promised 20). A diagonal is 8 * sqrt 2 m long, so its bound is
		/// HouseSlopeCm * sqrt 2, held exactly as squares.
		bool Level(const Ground& G, int64 X, int64 Y)
		{
			const int64 DX = int64{HeightAt(G, X + HouseHalfCm, Y)} - HeightAt(G, X - HouseHalfCm, Y);
			const int64 DY = int64{HeightAt(G, X, Y + HouseHalfCm)} - HeightAt(G, X, Y - HouseHalfCm);
			const int64 D1 =
				int64{HeightAt(G, X + HouseHalfCm, Y + HouseHalfCm)} - HeightAt(G, X - HouseHalfCm, Y - HouseHalfCm);
			const int64 D2 =
				int64{HeightAt(G, X + HouseHalfCm, Y - HouseHalfCm)} - HeightAt(G, X - HouseHalfCm, Y + HouseHalfCm);
			constexpr int64 Diagonal2 = 2 * int64{HouseSlopeCm} * HouseSlopeCm;
			return (DX < 0 ? -DX : DX) <= HouseSlopeCm && (DY < 0 ? -DY : DY) <= HouseSlopeCm && D1 * D1 <= Diagonal2 &&
				   D2 * D2 <= Diagonal2;
		}

		/// One region's houses so far, by the land tile each stands in (the
		/// review of 2026-09-27): Overlaps used to ask every house of the
		/// region about every plot, H^2/2 comparisons a region, and the count
		/// of houses follows the population, which grows with the years - on
		/// every day turn. A house's centre is HouseHalfCm inside its tile, so
		/// two houses in different tiles are at least 2 * HouseHalfCm apart on
		/// the axis that parts their tiles and never overlap: only the plot's
		/// own tile is asked, by the slot PlaceHouse drew it from - no search.
		/// (A tile narrower than a house cannot keep that margin; then the
		/// tiles within Reach are asked too, found in the region's ascending
		/// land by binary search.) The same predicate: every house that could
		/// overlap is among those asked, and the ones not asked could not.
		struct Plots
		{
			const std::vector<uint32>* Land = nullptr; ///< the region's land, ascending: the buckets' keys
			std::vector<std::vector<uint32>> ByTile;   ///< per Land slot, indices into Houses
			int64 Reach = 0;

			void Begin(const Ground& G, const std::vector<uint32>& RegionLand)
			{
				Land = &RegionLand;
				ByTile.assign(RegionLand.size(), std::vector<uint32>());
				Reach = G.Scale.CmPerTile > 2 * int64{HouseHalfCm}
							? 0
							: (2 * int64{HouseHalfCm} + G.Scale.CmPerTile - 1) / G.Scale.CmPerTile;
			}

			static bool Near(const Placed& H, int64 X, int64 Y)
			{
				const int64 HX = X - H.X;
				const int64 HY = Y - H.Y;
				return (HX < 0 ? -HX : HX) < 2 * HouseHalfCm && (HY < 0 ? -HY : HY) < 2 * HouseHalfCm;
			}

			bool Overlaps(const Ground& G, const std::vector<Placed>& Houses, usize Slot, int64 X, int64 Y) const
			{
				for (const uint32 i : ByTile[Slot])
				{
					if (Near(Houses[i], X, Y))
					{
						return true;
					}
				}
				if (Reach == 0)
				{
					return false;
				}
				const uint32 Tile = (*Land)[Slot];
				const int64 TX = Tile % G.Width;
				const int64 TY = Tile / G.Width;
				for (int64 DY = -Reach; DY <= Reach; ++DY)
				{
					for (int64 DX = -Reach; DX <= Reach; ++DX)
					{
						const int64 NX = TX + DX;
						const int64 NY = TY + DY;
						if ((DX == 0 && DY == 0) || NX < 0 || NY < 0 || NX >= G.Width || NY >= G.Height)
						{
							continue;
						}
						const uint32 Other = static_cast<uint32>(NY * G.Width + NX);
						const auto At = std::lower_bound(Land->begin(), Land->end(), Other);
						if (At == Land->end() || *At != Other)
						{
							continue;
						}
						for (const uint32 i : ByTile[static_cast<usize>(At - Land->begin())])
						{
							if (Near(Houses[i], X, Y))
							{
								return true;
							}
						}
					}
				}
				return false;
			}

			void Add(usize Slot, uint32 HouseIndex) { ByTile[Slot].push_back(HouseIndex); }
		};

		/// One house for Key in Region, or false after HouseTries plots. Only the
		/// houses in Taken (this region's, placed before it) are in its way.
		bool PlaceHouse(const Ground& G, const std::vector<uint32>& Land, uint32 Region, uint32 Key, Hash64 Salt,
						uint32 Flags, Plots& Taken, std::vector<Placed>& Houses)
		{
			if (Land.empty())
			{
				return false;
			}
			for (uint32 Try = 0; Try < HouseTries; ++Try)
			{
				const Hash64 H = Mix64(HashCombine(HashCombine(Salt, HashUInt64(Key)), HashUInt64(Try)));
				const usize Slot = static_cast<usize>(H % Land.size());
				const uint32 Tile = Land[Slot];
				int64 X = 0, Y = 0;
				PointIn(G, Tile, H, HouseHalfCm, X, Y);
				if (!Level(G, X, Y) || Taken.Overlaps(G, Houses, Slot, X, Y))
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
				Taken.Add(Slot, static_cast<uint32>(Houses.size()));
				Houses.push_back(P);
				return true;
			}
			return false;
		}

		/// The land tile of a region nearest its centroid tile's centre, over
		/// the region's own land - ascending, so the first least distance wins
		/// as it did when this scanned the whole map (once per settlement, per
		/// colony and per route end; the review of 2026-09-27).
		bool Middle(const Ground& G, const std::vector<uint32>& Land, const View::RegionView& R, uint32& Tile)
		{
			int64 X = 0, Y = 0;
			PointOfTile(G, R.CentroidTile, X, Y);
			bool Found = false;
			uint64 Best = 0;
			for (const uint32 T : Land)
			{
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

		/// The A*'s two per-tile arrays, kept from road to road and reset on
		/// the tiles a search touched rather than on all N (the review of
		/// 2026-09-27: two N-word fills per road were the second cost of the
		/// layout at 512). Every search still starts from Unreached everywhere.
		struct PathScratch
		{
			std::vector<uint32> Cost;
			std::vector<uint32> Came;
			std::vector<uint32> Touched;
		};
		constexpr uint32 Unreached = 0xFFFFFFFFu;

		/// Integer A* over land and river tiles, four-connected, cost one a step,
		/// ties on the lower tile index. Empty when no land path joins them.
		std::vector<uint32> Path(const Ground& G, uint32 From, uint32 To, PathScratch& Scratch)
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
			if (Scratch.Cost.size() != N)
			{
				Scratch.Cost.assign(N, Unreached);
				Scratch.Came.assign(N, Unreached);
				Scratch.Touched.clear();
			}
			std::vector<uint32>& Cost = Scratch.Cost;
			std::vector<uint32>& Came = Scratch.Came;
			const auto Reach = [&](uint32 T, uint32 At, uint32 Via)
			{
				if (Cost[T] == Unreached)
				{
					Scratch.Touched.push_back(T);
				}
				Cost[T] = At;
				Came[T] = Via;
			};
			using Entry = std::pair<uint64, uint32>; // (f << 32 | tile) for ordering, tile
			std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Open;
			Reach(From, 0u, Unreached);
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
					Reach(Next, Cost[T] + 1u, T);
					Open.push({(uint64{Cost[Next] + Guess(Next)} << 32) | Next, Next});
				}
			}
			std::vector<uint32> Out;
			if (Cost[To] != Unreached)
			{
				for (uint32 T = To; T != From; T = Came[T])
				{
					Out.push_back(T);
				}
				Out.push_back(From);
				std::reverse(Out.begin(), Out.end());
			}
			for (const uint32 T : Scratch.Touched)
			{
				Cost[T] = Unreached;
				Came[T] = Unreached;
			}
			Scratch.Touched.clear();
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

		// Each region's middle once (the first region of an index, as RegionIn
		// answers): the squares, the pits and both ends of every route ask for it.
		std::map<uint32, uint32> Middles;
		for (const View::RegionView& R : World_.Regions)
		{
			uint32 Tile = 0;
			if (Middle(G, LandIn(R.Index), R, Tile))
			{
				Middles.emplace(R.Index, Tile);
			}
		}
		const auto MiddleOf = [&](uint32 Region, uint32& Tile)
		{
			const auto Found = Middles.find(Region);
			if (Found == Middles.end())
			{
				return false;
			}
			Tile = Found->second;
			return true;
		};

		Plots Taken;
		for (const View::RegionView& R : World_.Regions)
		{
			uint32 Tile = 0;
			if (R.Settlement != 0u && MiddleOf(R.Index, Tile))
			{
				Out.Squares.push_back(At(G, Tile, R.Index, R.Settlement));
			}
			if (View::ColonyIn(Net, R.Index) != nullptr && MiddleOf(R.Index, Tile))
			{
				Out.Pits.push_back(At(G, Tile, R.Index, R.Index));
			}
			// Houses: a family's own, in ascending family order, each yielding only
			// to the ones before it; a coarse region's, ceil(people / 5) of them.
			Taken.Begin(G, LandIn(R.Index));
			if (R.Detailed != 0u)
			{
				const auto Found = Families.find(R.Index);
				if (Found != Families.end())
				{
					for (const uint32 Family : Found->second)
					{
						Out.Unplaced +=
							PlaceHouse(G, LandIn(R.Index), R.Index, Family, HouseSalt, 0u, Taken, Out.Houses) ? 0u : 1u;
					}
				}
			}
			else
			{
				const uint32 Count = (R.People + 4u) / 5u;
				for (uint32 k = 0; k < Count; ++k)
				{
					Out.Unplaced += PlaceHouse(G, LandIn(R.Index), R.Index, k, HashCombine(CoarseSalt, R.Index), 1u,
											   Taken, Out.Houses)
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

		PathScratch Scratch;
		for (const View::RouteView& Route : Net.Routes)
		{
			if (Route.Open == 0u)
			{
				continue;
			}
			const View::RegionView* A = View::RegionIn(World_, Route.From);
			const View::RegionView* B = View::RegionIn(World_, Route.To);
			uint32 TA = 0, TB = 0;
			if (A == nullptr || B == nullptr || !MiddleOf(A->Index, TA) || !MiddleOf(B->Index, TB))
			{
				++Out.Unrouted;
				continue;
			}
			RoadPath Road;
			Road.Route = Route.Index;
			Road.Tiles = Path(G, TA, TB, Scratch);
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
