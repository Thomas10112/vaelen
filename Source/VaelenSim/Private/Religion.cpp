// VAELEN - VaelenSim
// Phase 03.04: religions.
//
// STATUS: VALIDATED (Phase 03) - unit/deterministic/edge tests in Tests/Sim

#include "Vaelen/Sim/Religion.h"

#include "Vaelen/Core/Assert.h"
#include "Vaelen/Sim/History.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>

namespace Vaelen::History
{
	namespace
	{
		FaithState* FindState(World& W, const ReligionTypes& Types) noexcept
		{
			FaithState* Found = nullptr;
			W.Components()
				.GetPool(Types.State)
				.ForEach(
					[&](EntityHandle, FaithState& S)
					{
						if (Found == nullptr)
						{
							Found = &S;
						}
					});
			return Found;
		}

		std::vector<EntityHandle> RegionHandles(World& W, const WorldGen::WorldSetup& Setup)
		{
			std::vector<EntityHandle> Handles;
			W.Components()
				.GetPool(Setup.RegionTypes_.Region)
				.ForEach(
					[&](EntityHandle H, const WorldGen::RegionInfo& R)
					{
						if (Handles.size() <= R.Index)
						{
							Handles.resize(R.Index + 1u);
						}
						Handles[R.Index] = H;
					});
			return Handles;
		}

		// Believers of a region, creating the component on first use.
		RegionFaith& FaithOf(World& W, const ReligionTypes& Types, EntityHandle Region)
		{
			RegionFaith* F = W.Components().GetPool(Types.Faith).TryGet(Region);
			if (F == nullptr)
			{
				F = &W.Components().GetPool(Types.Faith).Add(Region, RegionFaith{});
			}
			return *F;
		}

		uint32 PopulationOf(World& W, const PopulationTypes& Population, EntityHandle Region) noexcept
		{
			const RegionPopulation* P = W.Components().GetPool(Population.Population).TryGet(Region);
			return P != nullptr ? P->Total : 0u;
		}

		uint32 MajorityCultureOf(World& W, const PopulationTypes& Population, EntityHandle Region) noexcept
		{
			const RegionPopulation* P = W.Components().GetPool(Population.Population).TryGet(Region);
			return P != nullptr ? P->Majority : 0u;
		}

		// Converts People of a region to a religion: the unconverted first, then
		// every other faith proportionally. Returns the believers actually added.
		uint32 Convert(RegionFaith& F, uint32 Religion, uint32 People, uint32 PopulationTotal) noexcept
		{
			const uint32 Believers = F.Total();
			const uint32 Unconverted = PopulationTotal > Believers ? PopulationTotal - Believers : 0u;
			uint32 FromUnconverted = People < Unconverted ? People : Unconverted;
			uint32 Remaining = People - FromUnconverted;
			if (FromUnconverted > 0 && !F.Add(Religion, FromUnconverted))
			{
				return 0;
			}
			if (Remaining > 0)
			{
				const uint32 Others = Believers - (F.SlotOf(Religion) < RegionFaith::MaxFaiths
													   ? F.Adherents[F.SlotOf(Religion)] - FromUnconverted
													   : 0u);
				uint32 Taken = 0;
				for (uint32 S = 0; S < RegionFaith::MaxFaiths && Others > 0; ++S)
				{
					if (F.Religion[S] != 0 && F.Religion[S] != Religion && F.Adherents[S] > 0)
					{
						const uint32 Share = static_cast<uint32>(uint64{Remaining} * F.Adherents[S] / Others);
						Taken += F.Remove(F.Religion[S], Share);
					}
				}
				F.Add(Religion, Taken);
				FromUnconverted += Taken;
			}
			F.Recount();
			return FromUnconverted;
		}
	} // namespace

	// ── RegionFaith ──────────────────────────────────────────────────────────

	uint32 RegionFaith::SlotOf(uint32 ReligionIndex) const noexcept
	{
		for (uint32 S = 0; S < MaxFaiths; ++S)
		{
			if (Religion[S] == ReligionIndex && ReligionIndex != 0)
			{
				return S;
			}
		}
		return MaxFaiths;
	}

	uint32 RegionFaith::Total() const noexcept
	{
		uint32 Sum = 0;
		for (uint32 S = 0; S < MaxFaiths; ++S)
		{
			Sum += Religion[S] != 0 ? Adherents[S] : 0u;
		}
		return Sum;
	}

	bool RegionFaith::Add(uint32 ReligionIndex, uint32 People) noexcept
	{
		if (ReligionIndex == 0)
		{
			return false;
		}
		if (People == 0)
		{
			return true; // nothing to add, no slot taken
		}
		uint32 S = SlotOf(ReligionIndex);
		if (S == MaxFaiths)
		{
			for (uint32 K = 0; K < MaxFaiths; ++K)
			{
				if (Religion[K] == 0)
				{
					S = K;
					break;
				}
			}
			if (S == MaxFaiths)
			{
				return false;
			}
			Religion[S] = ReligionIndex;
			Adherents[S] = 0;
		}
		Adherents[S] = Adherents[S] + People < Adherents[S] ? 0xffffffffu : Adherents[S] + People;
		Recount();
		return true;
	}

	uint32 RegionFaith::Remove(uint32 ReligionIndex, uint32 People) noexcept
	{
		const uint32 S = SlotOf(ReligionIndex);
		if (S == MaxFaiths)
		{
			return 0;
		}
		const uint32 Removed = People < Adherents[S] ? People : Adherents[S];
		Adherents[S] -= Removed;
		if (Adherents[S] == 0)
		{
			Religion[S] = 0;
		}
		Recount();
		return Removed;
	}

	void RegionFaith::Recount() noexcept
	{
		Majority = 0;
		uint32 Best = 0;
		for (uint32 S = 0; S < MaxFaiths; ++S)
		{
			if (Religion[S] == 0 || Adherents[S] == 0)
			{
				Religion[S] = 0; // a slot without believers is free
				Adherents[S] = 0;
				continue;
			}
			if (Adherents[S] > Best || (Adherents[S] == Best && Best > 0 && Religion[S] < Majority))
			{
				Best = Adherents[S];
				Majority = Religion[S];
			}
		}
	}

	// ── Types and state ──────────────────────────────────────────────────────

	ReligionTypes ReligionTypes::Declare(World& W)
	{
		ReligionTypes T;
		T.Religion = W.Types().Register<ReligionInfo>("ReligionInfo");
		T.Faith = W.Types().Register<RegionFaith>("RegionFaith");
		T.State = W.Types().Register<FaithState>("FaithState");
		W.Components().CreatePool(T.Religion);
		W.Components().CreatePool(T.Faith);
		W.Components().CreatePool(T.State);
		return T;
	}

	EntityHandle InitializeFaith(World& W, const ReligionTypes& Types)
	{
		const bool Fresh = FindState(W, Types) == nullptr;
		VAELEN_CHECKF(Fresh, "InitializeFaith called twice");
		if (!Fresh)
		{
			return EntityHandle{};
		}
		const EntityHandle H = W.CreateEntity(IdKind::Entity);
		W.Components().GetPool(Types.State).Add(H, FaithState{});
		return H;
	}

	// ── Tenets ───────────────────────────────────────────────────────────────

	Tenets DeriveTenets(Hash64 Identity) noexcept
	{
		Tenets T;
		Hash64 H = HashCombine(Identity, HashString("Tenets"));
		for (uint32 A = 0; A < Tenets::Axes; ++A)
		{
			H = HashUInt64(H + A);
			T.Value[A] = static_cast<uint8>(H & 0xffu);
		}
		return T;
	}

	Tenets SchismTenets(const Tenets& Parent, Hash64 Salt) noexcept
	{
		Tenets T = Parent;
		Hash64 H = HashCombine(Salt, HashString("Schism"));
		const uint32 Count = 1u + static_cast<uint32>(H & 1u);
		uint32 First = 0xffffffffu;
		for (uint32 i = 0; i < Count; ++i)
		{
			H = HashUInt64(H + 7u);
			uint32 Axis = static_cast<uint32>(H % Tenets::Axes);
			if (Axis == First)
			{
				Axis = (Axis + 1u) % Tenets::Axes;
			}
			First = Axis;
			const uint32 Delta = 32u + static_cast<uint32>((H >> 8) % 96u); // 32..127 either way
			const bool Up = ((H >> 16) & 1u) != 0;
			const int32 Moved =
				static_cast<int32>(T.Value[Axis]) + (Up ? static_cast<int32>(Delta) : -static_cast<int32>(Delta));
			T.Value[Axis] = static_cast<uint8>(Moved < 0 ? 0 : (Moved > 255 ? 255 : Moved));
			if (T.Value[Axis] == Parent.Value[Axis])
			{
				T.Value[Axis] = static_cast<uint8>(Parent.Value[Axis] ^ 0x80u); // always a visible change
			}
		}
		return T;
	}

	// ── Requests ─────────────────────────────────────────────────────────────

	bool ReligionSystem::RequestFounding(uint32 Region, PersistentId Cause, FoundingKind Kind)
	{
		FaithState* S = FindState(*Owner, Types);
		if (S == nullptr)
		{
			return false;
		}
		if (Region == 0 || !Cause.IsValid() || S->PendingCount >= FaithState::MaxPending)
		{
			++S->Refused;
			return false;
		}
		for (uint32 i = 0; i < S->PendingCount; ++i)
		{
			if (S->Pending[i].Region == Region)
			{
				++S->Refused;
				return false;
			}
		}
		S->Pending[S->PendingCount] = FaithRequest{Region, static_cast<uint32>(Kind), Cause.Value};
		++S->PendingCount;
		++S->Requested;
		return true;
	}

	// ── The system ───────────────────────────────────────────────────────────

	void ReligionSystem::Tick(TickContext& Context)
	{
		FaithState* S = FindState(*Owner, Types);
		if (S == nullptr || Context.Events == nullptr)
		{
			return;
		}
		World& W = *Owner;
		const ITileLayer* RegionLayer = W.Map().GetLayerBase(Setup.Regions.RegionIndex.Index);
		if (RegionLayer == nullptr)
		{
			return;
		}
		const Hash64 Digest = RegionLayer->Hash();
		if (Digest != GraphDigest)
		{
			Graph = WorldGen::BuildRegionGraph(W.Map(), Setup.Regions);
			GraphDigest = Digest;
		}
		const std::vector<EntityHandle> Regions = RegionHandles(W, Setup);

		// 1. Believers never exceed the population; tiny faiths fade.
		struct Change
		{
			EntityHandle Handle;
			uint32 Region;
			uint32 Previous;
		};
		std::vector<Change> Changes;
		std::vector<RegionFaith> Before(Regions.size());
		for (uint32 R = 1; R < Regions.size(); ++R)
		{
			if (Regions[R].IsNull())
			{
				continue;
			}
			RegionFaith* F = W.Components().GetPool(Types.Faith).TryGet(Regions[R]);
			if (F == nullptr)
			{
				continue;
			}
			const uint32 People = PopulationOf(W, Population, Regions[R]);
			const uint32 Believers = F->Total();
			if (Believers > People)
			{
				const uint32 Excess = Believers - People;
				for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
				{
					if (F->Religion[K] != 0)
					{
						const uint32 Share = static_cast<uint32>(uint64{Excess} * F->Adherents[K] / Believers);
						F->Remove(F->Religion[K], Share);
					}
				}
				while (F->Total() > People && F->Majority != 0)
				{
					F->Remove(F->Majority, F->Total() - People);
				}
			}
			for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
			{
				if (F->Religion[K] != 0 && uint64{F->Adherents[K]} * 1000u < uint64{People} * Rules.FadeSharePerMille)
				{
					F->Remove(F->Religion[K], F->Adherents[K]);
				}
			}
			Before[R] = *F;
		}

		// 2. Foundings requested since the last yearly tick, in request order.
		std::vector<ReligionInfo> Religions;
		W.Components()
			.GetPool(Types.Religion)
			.ForEach([&](EntityHandle, const ReligionInfo& Rg) { Religions.push_back(Rg); });
		auto FindReligion = [&](uint32 Index) -> const ReligionInfo*
		{
			for (const ReligionInfo& Rg : Religions)
			{
				if (Rg.Index == Index)
				{
					return &Rg;
				}
			}
			return nullptr;
		};
		const uint32 PendingCount = S->PendingCount;
		FaithRequest Pending[FaithState::MaxPending];
		for (uint32 i = 0; i < PendingCount; ++i)
		{
			Pending[i] = S->Pending[i];
			S->Pending[i] = FaithRequest{};
		}
		S->PendingCount = 0;
		for (uint32 i = 0; i < PendingCount; ++i)
		{
			const FaithRequest& Q = Pending[i];
			if (Q.Region >= Regions.size() || Regions[Q.Region].IsNull())
			{
				++S->Refused;
				continue;
			}
			const uint32 People = PopulationOf(W, Population, Regions[Q.Region]);
			if (People == 0)
			{
				++S->Refused;
				continue;
			}
			RegionFaith& F = FaithOf(W, Types, Regions[Q.Region]);
			const ReligionInfo* Parent = Q.Kind == static_cast<uint32>(FoundingKind::Schism) && F.Majority != 0
											 ? FindReligion(F.Majority)
											 : nullptr;
			ReligionInfo Rg;
			Rg.Index = S->ReligionCount + 1u;
			Rg.Culture = MajorityCultureOf(W, Population, Regions[Q.Region]);
			Rg.Parent = Parent != nullptr ? Parent->Index : 0u;
			Rg.Generation = Parent != nullptr ? Parent->Generation + 1u : 0u;
			Rg.HomeRegion = Q.Region;
			Rg.Kind = Q.Kind;
			Rg.Founded = Context.Tick;
			Rg.FoundingEvent = Q.Cause;
			Rg.Identity = Noise::LatticeHash(W.Config().Seed ^ 0x52454c47ull, static_cast<int32>(Rg.Index),
											 static_cast<int32>(Q.Region));
			Rg.Creed = Parent != nullptr ? SchismTenets(Parent->Creed, Rg.Identity) : DeriveTenets(Rg.Identity);
			const uint32 Wanted = static_cast<uint32>(uint64{People} * Rules.FoundingSharePerMille / 1000u);
			const uint32 Converted = Convert(F, Rg.Index, Wanted > 0 ? Wanted : 1u, People);
			if (Converted == 0)
			{
				++S->Refused; // no slot free in the region: the faith never takes
				continue;
			}
			++S->ReligionCount;
			const EntityHandle H = W.CreateEntity(IdKind::Religion);
			W.Components().GetPool(Types.Religion).Add(H, Rg);
			Religions.push_back(Rg);
			Context.Events->Publish(Context.Tick, Rg.Parent != 0 ? SchismEvent : ReligionFoundedEvent,
									ReligionPayload{Rg.Index, Rg.HomeRegion, Rg.Culture, Rg.Parent},
									W.Entities().GetId(H), PersistentId{Q.Cause});
		}

		// 3. Spread, decided on the start-of-year state in region order.
		for (uint32 R = 1; R < Regions.size(); ++R)
		{
			if (Regions[R].IsNull() || Before[R].Majority == 0)
			{
				continue;
			}
			const uint32 Faith = Before[R].Majority;
			{
				RegionFaith& F = FaithOf(W, Types, Regions[R]);
				const uint32 People = PopulationOf(W, Population, Regions[R]);
				const uint32 Believers = Before[R].Total();
				const uint32 Unconverted = People > Believers ? People - Believers : 0u;
				const uint32 Converts = static_cast<uint32>(uint64{Unconverted} * Rules.ConvertPerMille / 1000u) +
										(Unconverted > 0 ? 1u : 0u);
				// Live room: other regions may have spread into this one this year.
				const uint32 Room = People > F.Total() ? People - F.Total() : 0u;
				if (Converts > 0 && Room > 0)
				{
					F.Add(Faith, Converts < Room ? Converts : Room);
				}
			}
			if (R >= Graph.Neighbours.size())
			{
				continue;
			}
			for (uint16 N : Graph.Neighbours[R])
			{
				if (N >= Regions.size() || Regions[N].IsNull() || Before[N].Majority == Faith)
				{
					continue;
				}
				const uint32 People = PopulationOf(W, Population, Regions[N]);
				if (People == 0)
				{
					continue;
				}
				const uint32 Believers = Before[N].Total();
				const uint32 Unconverted = People > Believers ? People - Believers : 0u;
				const uint32 Converts = static_cast<uint32>(uint64{Unconverted} * Rules.SpreadPerMille / 1000u);
				if (Converts > 0)
				{
					RegionFaith& F = FaithOf(W, Types, Regions[N]);
					const uint32 Room = People > F.Total() ? People - F.Total() : 0u;
					if (Room > 0)
					{
						F.Add(Faith, Converts < Room ? Converts : Room);
					}
				}
			}
		}

		// 4. Majority changes are events.
		for (uint32 R = 1; R < Regions.size(); ++R)
		{
			if (Regions[R].IsNull())
			{
				continue;
			}
			const RegionFaith* F = W.Components().GetPool(Types.Faith).TryGet(Regions[R]);
			const uint32 Now = F != nullptr ? F->Majority : 0u;
			if (Now != Before[R].Majority)
			{
				const uint32 Adherents = F != nullptr && Now != 0 ? F->Adherents[F->SlotOf(Now)] : 0u;
				Context.Events->Publish(Context.Tick, RegionConvertedEvent,
										ConversionPayload{R, Now, Before[R].Majority, Adherents},
										W.Entities().GetId(Regions[R]));
			}
		}

		// 5. Names, in the founding culture's language.
		if (HasLanguages)
		{
			W.Components()
				.GetPool(Types.Religion)
				.ForEach(
					[&](EntityHandle H, const ReligionInfo& Rg)
					{
						if (NameOf(W, Languages, H) != nullptr)
						{
							return;
						}
						const LanguageInfo* Language = nullptr;
						EntityHandle LanguageHandle;
						W.Components()
							.GetPool(Languages.Language)
							.ForEach(
								[&](EntityHandle LH, const LanguageInfo& L)
								{
									if (L.Culture == Rg.Culture && Language == nullptr)
									{
										Language = &L;
										LanguageHandle = LH;
									}
								});
						if (Language == nullptr)
						{
							return;
						}
						for (uint32 Salt = 0; Salt < 256; ++Salt)
						{
							const NameText Text = GenerateName(Language->Sounds, NameScope::Religion, Rg.Index, Salt);
							if (IsNameUsed(W, Languages, NameScope::Religion, Text))
							{
								continue;
							}
							NameInfo N;
							N.Language = Language->Index;
							N.Scope = static_cast<uint32>(NameScope::Religion);
							N.Key = Rg.Index;
							N.Salt = Salt;
							N.Generation = Language->Generation;
							N.Text = Text;
							W.Components().GetPool(Languages.Name).Add(H, N);
							++W.Components().GetPool(Languages.Language).Get(LanguageHandle).Names;
							Context.Events->Publish(Context.Tick, NamedEvent, NamePayload{N.Language, N.Scope, N.Key},
													W.Entities().GetId(H));
							break;
						}
					});
		}
	}

	// ── The listener ─────────────────────────────────────────────────────────

	void FaithListener::Listen(EventBus& Bus)
	{
		Bus.Subscribe(MigrationWaveEvent.TypeHash, this);
		Bus.Subscribe(CultureSplitEvent.TypeHash, this);
		Bus.Subscribe(EraOpenedEvent.TypeHash, this);
	}

	void FaithListener::OnEvent(const Event& E)
	{
		World& W = *Owner;
		if (E.Is(MigrationWaveEvent))
		{
			// Believers travel with the wave in the source's proportions.
			const RegionPeople Wave = E.Get<RegionPeople>();
			const std::vector<EntityHandle> Regions = RegionHandles(W, Setup);
			const uint32 From = Wave.Reserved;
			const uint32 To = Wave.Region;
			if (From == 0 || To == 0 || From >= Regions.size() || To >= Regions.size() || Regions[From].IsNull() ||
				Regions[To].IsNull() || Wave.People == 0)
			{
				return;
			}
			RegionFaith* Source = W.Components().GetPool(Types.Faith).TryGet(Regions[From]);
			if (Source == nullptr || Source->Total() == 0)
			{
				return;
			}
			const uint32 PeopleBefore = PopulationOf(W, Population, Regions[From]) + Wave.People;
			const RegionFaith Snapshot = *Source;
			for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
			{
				if (Snapshot.Religion[K] == 0)
				{
					continue;
				}
				const uint32 Carried = static_cast<uint32>(uint64{Snapshot.Adherents[K]} * Wave.People /
														   (PeopleBefore > 0 ? PeopleBefore : 1u));
				if (Carried == 0)
				{
					continue;
				}
				RegionFaith& Target = FaithOf(W, Types, Regions[To]);
				const uint32 Room = PopulationOf(W, Population, Regions[To]);
				const uint32 Free = Room > Target.Total() ? Room - Target.Total() : 0u;
				if (Free == 0)
				{
					continue; // believers never exceed the people of a region
				}
				const uint32 Removed = Source->Remove(Snapshot.Religion[K], Carried < Free ? Carried : Free);
				if (!Target.Add(Snapshot.Religion[K], Removed))
				{
					Source->Add(Snapshot.Religion[K], Removed); // no slot at the destination: they stay
				}
				Source = W.Components().GetPool(Types.Faith).TryGet(Regions[From]); // pool may have grown
				if (Source == nullptr)
				{
					return;
				}
			}
			return;
		}
		if (E.Is(CultureSplitEvent) && Rules.SchismOnSplit != 0)
		{
			const uint32 Region = E.Get<CulturePayload>().Region;
			const std::vector<EntityHandle> Regions = RegionHandles(W, Setup);
			if (Region == 0 || Region >= Regions.size() || Regions[Region].IsNull())
			{
				return;
			}
			const RegionFaith* F = W.Components().GetPool(Types.Faith).TryGet(Regions[Region]);
			if (F == nullptr || F->Majority == 0)
			{
				return;
			}
			if (HashUInt64(E.Id.Value ^ 0x534348ull) % 1000u < Rules.SchismPerMille)
			{
				System->RequestFounding(Region, E.Id, FoundingKind::Schism);
			}
			return;
		}
		if (E.Is(EraOpenedEvent) && Rules.FoundOnEra != 0)
		{
			// The home region of the largest culture, when it has no faith yet.
			std::vector<uint64> People;
			W.Components()
				.GetPool(Population.Population)
				.ForEach(
					[&](EntityHandle, const RegionPopulation& P)
					{
						for (uint32 K = 0; K < RegionPopulation::MaxCultures; ++K)
						{
							if (P.Culture[K] != 0)
							{
								if (People.size() <= P.Culture[K])
								{
									People.resize(P.Culture[K] + 1u, 0);
								}
								People[P.Culture[K]] += P.Count[K];
							}
						}
					});
			uint32 Largest = 0;
			for (uint32 C = 1; C < People.size(); ++C)
			{
				if (Largest == 0 || People[C] > People[Largest])
				{
					Largest = C;
				}
			}
			if (Largest == 0)
			{
				return;
			}
			uint32 Home = 0;
			W.Components()
				.GetPool(Population.Culture)
				.ForEach(
					[&](EntityHandle, const CultureInfo& C)
					{
						if (C.Index == Largest)
						{
							Home = C.HomeRegion;
						}
					});
			const std::vector<EntityHandle> Regions = RegionHandles(W, Setup);
			if (Home == 0 || Home >= Regions.size() || Regions[Home].IsNull())
			{
				return;
			}
			const RegionFaith* F = W.Components().GetPool(Types.Faith).TryGet(Regions[Home]);
			if (F != nullptr && F->Majority != 0)
			{
				return;
			}
			System->RequestFounding(Home, E.Id, FoundingKind::Era);
		}
	}

	// ── Queries ──────────────────────────────────────────────────────────────

	FaithStats MeasureFaith(const World& W, const PopulationTypes& Population, const ReligionTypes& Types)
	{
		FaithStats S;
		std::vector<uint64> PerReligion;
		W.Components()
			.GetPool(Types.Religion)
			.ForEach(
				[&](EntityHandle, const ReligionInfo& Rg)
				{
					++S.Religions;
					S.Schisms += Rg.Parent != 0 ? 1u : 0u;
					if (PerReligion.size() <= Rg.Index)
					{
						PerReligion.resize(Rg.Index + 1u, 0);
					}
				});
		W.Components()
			.GetPool(Population.Population)
			.ForEach(
				[&](EntityHandle H, const RegionPopulation& P)
				{
					++S.Regions;
					S.People += P.Total;
					const RegionFaith* F = W.Components().GetPool(Types.Faith).TryGet(H);
					if (F == nullptr || F->Total() == 0)
					{
						return;
					}
					++S.RegionsWithFaith;
					S.ConvertedRegions += F->Majority != 0 ? 1u : 0u;
					for (uint32 K = 0; K < RegionFaith::MaxFaiths; ++K)
					{
						if (F->Religion[K] != 0)
						{
							S.Adherents += F->Adherents[K];
							if (F->Religion[K] < PerReligion.size())
							{
								PerReligion[F->Religion[K]] += F->Adherents[K];
							}
						}
					}
				});
		for (uint32 R = 1; R < PerReligion.size(); ++R)
		{
			if (PerReligion[R] > S.LargestAdherents)
			{
				S.LargestAdherents = PerReligion[R];
				S.LargestReligion = R;
			}
		}
		W.Components()
			.GetPool(Types.State)
			.ForEach(
				[&](EntityHandle, const FaithState& St)
				{
					S.Pending = St.PendingCount;
					S.Refused = St.Refused;
				});
		return S;
	}

	void ExportFaithAscii(const World& W, const WorldGen::WorldSetup& Setup, const PopulationTypes& Population,
						  const ReligionTypes& Types, uint32 Columns, std::string& Out)
	{
		const WorldMap& Map = W.Map();
		const WorldGrid& Grid = Map.Grid();
		const TileLayer<uint16>& RegionIx = Map.GetLayer(Setup.Regions.RegionIndex);
		std::vector<char> Glyph;
		W.Components()
			.GetPool(Setup.RegionTypes_.Region)
			.ForEach(
				[&](EntityHandle H, const WorldGen::RegionInfo& R)
				{
					if (Glyph.size() <= R.Index)
					{
						Glyph.resize(R.Index + 1u, ' ');
					}
					const RegionPopulation* P = W.Components().GetPool(Population.Population).TryGet(H);
					const RegionFaith* F = W.Components().GetPool(Types.Faith).TryGet(H);
					if (P == nullptr || P->Total == 0)
					{
						Glyph[R.Index] = ' ';
					}
					else if (F == nullptr || F->Majority == 0)
					{
						Glyph[R.Index] = '.';
					}
					else
					{
						const uint32 M = F->Majority - 1u;
						Glyph[R.Index] = M < 26 ? static_cast<char>('A' + M) : static_cast<char>('a' + (M - 26) % 26);
					}
				});
		// Same cells as ExportCultureAscii (two tiles high per column tile), sampled
		// at the cell centre.
		Out.clear();
		const uint32 Cols = Columns == 0 || Columns > Grid.Width ? Grid.Width : Columns;
		const uint32 CellW = Grid.Width / Cols;
		const uint32 CellH = CellW * 2 > Grid.Height ? Grid.Height : CellW * 2;
		const uint32 Rows = Grid.Height / CellH;
		for (uint32 Row = 0; Row < Rows; ++Row)
		{
			for (uint32 C = 0; C < Cols; ++C)
			{
				const uint32 X = C * CellW + CellW / 2;
				const uint32 Y = Row * CellH + CellH / 2;
				const uint16 R = RegionIx[Y * Grid.Width + X];
				Out += R == 0 ? '~' : (R < Glyph.size() ? Glyph[R] : '?');
			}
			Out += '\n';
		}
	}
} // namespace Vaelen::History
