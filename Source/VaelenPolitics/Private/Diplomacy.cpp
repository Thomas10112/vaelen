// VAELEN - VaelenPolitics
// Phase 07.06: diplomacy.
//
// STATUS: VALIDATED (Phase 07) - unit/integration/deterministic/edge tests in Tests/Politics

#include "Vaelen/Politics/Diplomacy.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::Politics
{
	namespace
	{
		constexpr uint64 TreatySalt = 0x54524541545900ull; // "TREATY"
	} // namespace

	const char* StanceName(Stance S) noexcept
	{
		switch (S)
		{
		case Stance::Pact:
			return "Pact";
		case Stance::Peace:
			return "Peace";
		case Stance::Rivalry:
			return "Rivalry";
		case Stance::War:
			return "War";
		default:
			return "Unknown";
		}
	}

	DiplomacyTypes DiplomacyTypes::Declare(World& W)
	{
		DiplomacyTypes T;
		T.Relation_ = W.Types().Register<Relation>("Relation");
		T.Contested = W.Types().Register<RegionInPlay>("RegionInPlay");
		W.Components().CreatePool(T.Relation_);
		W.Components().CreatePool(T.Contested);
		return T;
	}

	void DiplomacySystem::Tick(TickContext& Context)
	{
		if (Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		std::vector<EntityHandle> RegionHandles;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (R.Index >= RegionHandles.size())
					{
						RegionHandles.resize(usize{R.Index} + 1u);
					}
					RegionHandles[R.Index] = H;
				});
		const usize N = RegionHandles.size();
		if (N <= 1)
		{
			return;
		}
		if (GraphRegions != static_cast<uint32>(N))
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Types.World.Regions);
			GraphRegions = static_cast<uint32>(N);
		}

		std::vector<uint32> RuledBy(N, 0u);
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(RegionHandles[R]);
			RuledBy[R] = Rule != nullptr ? Rule->Polity : 0u;
		}

		struct Seat
		{
			uint32 Index = 0;
			uint32 Culture = 0;
			uint32 Held = 0;
			bool Standing = false;
		};
		std::vector<Seat> Seats;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach([&](EntityHandle, const PolityInfo& P)
					 { Seats.push_back(Seat{P.Index, P.Culture, 0u, P.Dissolved == 0}); });
		std::sort(Seats.begin(), Seats.end(), [](const Seat& A, const Seat& B) { return A.Index < B.Index; });
		auto Find = [&](uint32 Polity) -> Seat*
		{
			const auto At = std::lower_bound(Seats.begin(), Seats.end(), Polity,
											 [](const Seat& S, uint32 V) { return S.Index < V; });
			return At != Seats.end() && At->Index == Polity ? &*At : nullptr;
		};
		for (uint32 R = 1; R < N; ++R)
		{
			Seat* S = RuledBy[R] != 0 ? Find(RuledBy[R]) : nullptr;
			if (S != nullptr)
			{
				++S->Held;
			}
		}

		// 1. Who touches whom, and how long the border is. Region pairs are
		//    walked in region order so the count never depends on pool order.
		struct Touch
		{
			uint32 A = 0;
			uint32 B = 0;
			uint32 Border = 0;
			uint32 Roads = 0;
		};
		std::vector<Touch> Touches;
		for (uint32 R = 1; R < N; ++R)
		{
			const uint32 Mine = RuledBy[R];
			if (Mine == 0 || R >= Graph.Neighbours.size())
			{
				continue;
			}
			for (const uint16 Next : Graph.Neighbours[R])
			{
				if (Next == 0 || Next >= N)
				{
					continue;
				}
				const uint32 Theirs = RuledBy[Next];
				if (Theirs == 0 || Theirs == Mine || Theirs < Mine)
				{
					continue; // counted once, from the lower polity's side
				}
				const auto At =
					std::lower_bound(Touches.begin(), Touches.end(), std::pair<uint32, uint32>{Mine, Theirs},
									 [](const Touch& T, const std::pair<uint32, uint32>& V)
									 { return std::pair<uint32, uint32>{T.A, T.B} < V; });
				if (At != Touches.end() && At->A == Mine && At->B == Theirs)
				{
					++At->Border;
				}
				else
				{
					Touches.insert(At, Touch{Mine, Theirs, 1u, 0u});
				}
			}
		}
		// The roads of trade that cross a border warm the two sides that own them.
		W.Components()
			.GetPool(Trade.Route)
			.ForEach(
				[&](EntityHandle, const Economy::RouteInfo& Road)
				{
					if (Road.Closed != 0 || Road.From >= N || Road.To >= N)
					{
						return;
					}
					uint32 A = RuledBy[Road.From];
					uint32 B = RuledBy[Road.To];
					if (A == 0 || B == 0 || A == B)
					{
						return;
					}
					if (A > B)
					{
						std::swap(A, B);
					}
					const auto At = std::lower_bound(Touches.begin(), Touches.end(), std::pair<uint32, uint32>{A, B},
													 [](const Touch& T, const std::pair<uint32, uint32>& V)
													 { return std::pair<uint32, uint32>{T.A, T.B} < V; });
					if (At != Touches.end() && At->A == A && At->B == B)
					{
						++At->Roads;
					}
				});

		// 2. The relations that already exist, in index order.
		struct Known
		{
			EntityHandle Handle;
			uint32 Index = 0;
			uint32 A = 0;
			uint32 B = 0;
		};
		std::vector<Known> Existing;
		uint32 Highest = 0;
		W.Components()
			.GetPool(Relations.Relation_)
			.ForEach(
				[&](EntityHandle H, const Relation& R)
				{
					Highest = std::max(Highest, R.Index);
					Existing.push_back(Known{H, R.Index, R.A, R.B});
				});
		std::sort(Existing.begin(), Existing.end(), [](const Known& X, const Known& Y)
				  { return std::pair<uint32, uint32>{X.A, X.B} < std::pair<uint32, uint32>{Y.A, Y.B}; });
		auto Lookup = [&](uint32 A, uint32 B) -> const Known*
		{
			const auto At = std::lower_bound(Existing.begin(), Existing.end(), std::pair<uint32, uint32>{A, B},
											 [](const Known& K, const std::pair<uint32, uint32>& V)
											 { return std::pair<uint32, uint32>{K.A, K.B} < V; });
			return At != Existing.end() && At->A == A && At->B == B ? &*At : nullptr;
		};

		// 3. Warm, cool, and turn. A pair no longer touching keeps its relation
		//    as it stands: they simply stop drifting towards each other.
		for (const Touch& T : Touches)
		{
			const Seat* Left = Find(T.A);
			const Seat* Right = Find(T.B);
			if (Left == nullptr || Right == nullptr || !Left->Standing || !Right->Standing)
			{
				continue;
			}
			const Known* Row = Lookup(T.A, T.B);
			Relation* Bond = Row != nullptr ? W.Components().GetPool(Relations.Relation_).TryGet(Row->Handle) : nullptr;
			if (Bond == nullptr)
			{
				++Highest;
				Relation Fresh;
				Fresh.Index = Highest;
				Fresh.A = T.A;
				Fresh.B = T.B;
				Fresh.Stance_ = static_cast<uint32>(Stance::Peace);
				Fresh.Warmth = std::min(Rules.WarmthAtContact, Rules.WarmthCeiling);
				Fresh.Border = T.Border;
				Fresh.Met = Context.Tick;
				Fresh.Turned = Context.Tick;
				Fresh.Identity =
					Noise::LatticeHash(W.Config().Seed ^ TreatySalt, static_cast<int32>(T.A), static_cast<int32>(T.B));
				const EntityHandle H = W.CreateEntity(IdKind::Treaty);
				W.Components().GetPool(Relations.Relation_).Add(H, Fresh);
				Existing.insert(std::lower_bound(Existing.begin(), Existing.end(), std::pair<uint32, uint32>{T.A, T.B},
												 [](const Known& K, const std::pair<uint32, uint32>& V)
												 { return std::pair<uint32, uint32>{K.A, K.B} < V; }),
								Known{H, Fresh.Index, T.A, T.B});
				Context.Events->Publish(Context.Tick, ContactMadeEvent, PolityPayload{T.A, 0, T.B, Fresh.Warmth},
										W.Entities().GetId(H));
				continue; // the year of meeting is not a year of drifting
			}
			Bond->Border = T.Border;
			int64 Drift = 0;
			Drift += Left->Culture == Right->Culture ? static_cast<int64>(Rules.WarmthSameCulture) : 0;
			Drift += static_cast<int64>(uint64{T.Roads} * Rules.WarmthPerRoute);
			Drift -= static_cast<int64>(uint64{T.Border} * Rules.ChillPerBorder);
			const uint32 Bigger = std::max(Left->Held, Right->Held);
			const uint32 Smaller = std::min(Left->Held, Right->Held);
			Drift -= static_cast<int64>(uint64{Bigger - Smaller} * Rules.ChillPerSizeStep);
			const int64 Warm = static_cast<int64>(Bond->Warmth) + Drift;
			Bond->Warmth =
				Warm <= 0 ? 0u : static_cast<uint32>(std::min<int64>(Warm, static_cast<int64>(Rules.WarmthCeiling)));

			// The stance follows the warmth, but only once it has passed the
			// edge of where it stands: a pact is not lost to one bad year.
			const uint32 Was = Bond->Stance_;
			const uint32 Edge = Rules.Hysteresis;
			uint32 Now = Was;
			auto Above = [&](uint32 Bar) { return Bond->Warmth >= Bar + Edge; };
			auto Below = [&](uint32 Bar) { return Bond->Warmth + Edge < Bar; };
			if (Above(Rules.PactAt))
			{
				Now = static_cast<uint32>(Stance::Pact);
			}
			else if (Below(Rules.RivalryAt))
			{
				Now = static_cast<uint32>(Stance::War);
			}
			else if (Above(Rules.PeaceAt) && Below(Rules.PactAt))
			{
				Now = static_cast<uint32>(Stance::Peace);
			}
			else if (Above(Rules.RivalryAt) && Below(Rules.PeaceAt))
			{
				Now = static_cast<uint32>(Stance::Rivalry);
			}
			if (Now != Was)
			{
				Bond->Stance_ = Now;
				Bond->Turned = Context.Tick;
				Context.Events->Publish(Context.Tick, StanceChangedEvent, PolityPayload{T.A, 0, T.B, Now},
										W.Entities().GetId(Row->Handle));
			}
		}

		// 4. What a war puts in play: the border regions of the weaker side.
		//    Written on the region, so the reach system can take one without
		//    ever learning what a war is.
		std::vector<uint32> Marked(N, 0u);
		for (const Known& K : Existing)
		{
			const Relation* Bond = W.Components().GetPool(Relations.Relation_).TryGet(K.Handle);
			if (Bond == nullptr || Bond->Stance_ != static_cast<uint32>(Stance::War))
			{
				continue;
			}
			const Seat* Left = Find(Bond->A);
			const Seat* Right = Find(Bond->B);
			if (Left == nullptr || Right == nullptr || !Left->Standing || !Right->Standing)
			{
				continue;
			}
			const uint32 Strong = Left->Held >= Right->Held ? Bond->A : Bond->B;
			const uint32 Weak = Strong == Bond->A ? Bond->B : Bond->A;
			for (uint32 R = 1; R < N; ++R)
			{
				if (RuledBy[R] != Weak || R >= Graph.Neighbours.size() || Marked[R] != 0)
				{
					continue;
				}
				for (const uint16 Next : Graph.Neighbours[R])
				{
					if (Next != 0 && Next < N && RuledBy[Next] == Strong)
					{
						Marked[R] = Strong;
						break;
					}
				}
			}
		}
		for (uint32 R = 1; R < N; ++R)
		{
			if (RegionHandles[R].IsNull())
			{
				continue;
			}
			RegionInPlay* Play = W.Components().GetPool(Relations.Contested).TryGet(RegionHandles[R]);
			if (Marked[R] == 0)
			{
				if (Play != nullptr && Play->By != 0)
				{
					Play->By = 0;
				}
				continue;
			}
			if (Play == nullptr)
			{
				W.Components().GetPool(Relations.Contested).Add(RegionHandles[R], RegionInPlay{Marked[R], 0u});
				Context.Events->Publish(Context.Tick, RegionContestedEvent, PolityPayload{Marked[R], R, RuledBy[R], 0},
										W.Entities().GetId(RegionHandles[R]));
				continue;
			}
			if (Play->By != Marked[R])
			{
				Play->By = Marked[R];
				Context.Events->Publish(Context.Tick, RegionContestedEvent, PolityPayload{Marked[R], R, RuledBy[R], 0},
										W.Entities().GetId(RegionHandles[R]));
			}
		}
	}

	const Relation* RelationBetween(const World& W, const DiplomacyTypes& Relations, uint32 A, uint32 B)
	{
		if (A == 0 || B == 0 || A == B)
		{
			return nullptr;
		}
		if (A > B)
		{
			std::swap(A, B);
		}
		const Relation* Found = nullptr;
		W.Components()
			.GetPool(Relations.Relation_)
			.ForEach(
				[&](EntityHandle H, const Relation& R)
				{
					if (R.A == A && R.B == B && Found == nullptr)
					{
						Found = W.Components().GetPool(Relations.Relation_).TryGet(H);
					}
				});
		return Found;
	}

	void NeighboursOf(const World& W, const DiplomacyTypes& Relations, uint32 Polity, std::vector<uint32>& Out)
	{
		Out.clear();
		if (Polity == 0)
		{
			return;
		}
		W.Components()
			.GetPool(Relations.Relation_)
			.ForEach(
				[&](EntityHandle, const Relation& R)
				{
					if (R.A == Polity)
					{
						Out.push_back(R.B);
					}
					else if (R.B == Polity)
					{
						Out.push_back(R.A);
					}
				});
		std::sort(Out.begin(), Out.end());
	}

	DiplomacyStats MeasureDiplomacy(const World& W, const History::PreHistoryTypes& Types, const PolityTypes& Polities,
									const DiplomacyTypes& Relations, const DiplomacyRules& Rules)
	{
		DiplomacyStats S;
		std::vector<uint32> Standing;
		W.Components()
			.GetPool(Polities.Polity)
			.ForEach(
				[&](EntityHandle, const PolityInfo& P)
				{
					if (P.Dissolved == 0)
					{
						Standing.push_back(P.Index);
					}
				});
		std::sort(Standing.begin(), Standing.end());

		std::vector<uint32> RuledBy;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionRule* Rule = W.Components().GetPool(Polities.Rule).TryGet(H);
					if (R.Index >= RuledBy.size())
					{
						RuledBy.resize(usize{R.Index} + 1u, 0u);
					}
					RuledBy[R.Index] = Rule != nullptr ? Rule->Polity : 0u;
				});

		std::vector<Relation> All;
		W.Components().GetPool(Relations.Relation_).ForEach([&](EntityHandle, const Relation& R) { All.push_back(R); });
		std::sort(All.begin(), All.end(), [](const Relation& X, const Relation& Y) { return X.Index < Y.Index; });

		std::vector<std::pair<uint32, RegionInPlay>> Plays;
		W.Components()
			.GetPool(Types.World.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					const RegionInPlay* P = W.Components().GetPool(Relations.Contested).TryGet(H);
					if (P != nullptr)
					{
						Plays.push_back({R.Index, *P});
					}
				});
		std::sort(Plays.begin(), Plays.end(), [](const auto& X, const auto& Y) { return X.first < Y.first; });

		Hash64 D = HashString("Diplomacy");
		for (const Relation& R : All)
		{
			++S.Relations_;
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&R), sizeof(R)));
			S.Bad += R.A == 0 || R.B == 0 || R.A >= R.B ? 1u : 0u;
			S.Bad += R.Warmth > Rules.WarmthCeiling ? 1u : 0u;
			S.Bad += R.Stance_ > static_cast<uint32>(Stance::War) ? 1u : 0u;
			const bool Live = std::binary_search(Standing.begin(), Standing.end(), R.A) &&
							  std::binary_search(Standing.begin(), Standing.end(), R.B);
			if (!Live)
			{
				continue; // a relation outlives the polities in it, as a record
			}
			switch (static_cast<Stance>(R.Stance_))
			{
			case Stance::Pact:
				++S.Pacts;
				break;
			case Stance::Peace:
				++S.Peaces;
				break;
			case Stance::Rivalry:
				++S.Rivalries;
				break;
			case Stance::War:
				++S.Wars;
				break;
			default:
				break;
			}
		}
		for (const auto& [Region, Play] : Plays)
		{
			D = HashCombine(D, HashUInt64(Region));
			D = HashCombine(D, HashBytes(reinterpret_cast<const char*>(&Play), sizeof(Play)));
			if (Play.By == 0)
			{
				continue;
			}
			++S.InPlay;
			// A region in play is ruled by somebody who is not the one who may take it.
			const uint32 Holder = Region < RuledBy.size() ? RuledBy[Region] : 0u;
			S.Bad += Holder == 0 || Holder == Play.By ? 1u : 0u;
			S.Bad += std::binary_search(Standing.begin(), Standing.end(), Play.By) ? 0u : 1u;
		}
		for (const Event& E : W.Log().All())
		{
			S.Contacts += E.Is(ContactMadeEvent) ? 1u : 0u;
			S.Turns += E.Is(StanceChangedEvent) ? 1u : 0u;
			S.Contests += E.Is(RegionContestedEvent) ? 1u : 0u;
		}
		S.Digest = D;
		return S;
	}
} // namespace Vaelen::Politics
