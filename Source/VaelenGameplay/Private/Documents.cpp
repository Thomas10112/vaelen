// VAELEN - VaelenGameplay
// Phase 12 task 12.03: documents.
//
// STATUS: PROTOTYPE (Phase 12) - unit/integration/deterministic/edge tests in Tests/Gameplay
#include "Vaelen/Gameplay/Documents.h"

#include "Vaelen/Core/Hash.h"
#include "Vaelen/Sim/Noise.h"
#include "Vaelen/Sim/World.h"

#include <algorithm>
#include <cstdio>

namespace Vaelen::Gameplay
{
	namespace
	{
		constexpr uint64 DocumentSalt = 0x646f63756d656e74ull; // "document"

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

		EntityHandle DocumentHandle(const World& W, const DocumentTypes& Documents, uint32 Document)
		{
			EntityHandle Found;
			W.Components()
				.GetPool(Documents.Document)
				.ForEach(
					[&](EntityHandle H, const DocumentInfo& D)
					{
						if (D.Index == Document && Found.IsNull())
						{
							Found = H;
						}
					});
			return Found;
		}

		uint32 NextIndex(const World& W, const DocumentTypes& Documents)
		{
			uint32 Next = 0;
			W.Components()
				.GetPool(Documents.Document)
				.ForEach([&](EntityHandle, const DocumentInfo& D) { Next = D.Index > Next ? D.Index : Next; });
			return Next + 1u;
		}
	} // namespace

	DocumentTypes DocumentTypes::Declare(World& W)
	{
		DocumentTypes T;
		T.Document = W.Types().Register<DocumentInfo>("DocumentInfo");
		W.Components().CreatePool(T.Document);
		return T;
	}

	uint32 WriteDocument(World& W, const Population::PersonTypes& Persons, const ReputeTypes& Repute,
						 const DocumentTypes& Documents, uint32 Writer, uint32 About, SimTick Now)
	{
		if (Writer == 0 || About == 0 || Writer == About)
		{
			return 0;
		}
		// Nobody writes about somebody they have never met. What goes on the
		// page is what the writer thinks NOW, and that is the whole point: it
		// stops being what they think the moment after.
		const Player::Opinion* Held = OpinionOf(W, Persons, Repute, About, Writer);
		if (Held == nullptr)
		{
			return 0;
		}
		const EntityHandle WH = HandleOf(W, Persons, Writer);
		if (WH.IsNull())
		{
			return 0;
		}
		const Population::PersonInfo* P = W.Components().GetPool(Persons.Person).TryGet(WH);
		DocumentInfo D;
		D.Index = NextIndex(W, Documents);
		D.Writer = Writer;
		D.About = About;
		D.Says = Held->Regard;
		D.Region = P != nullptr ? P->Region : 0u;
		D.Holder = Writer;
		D.Written = Now;
		D.Identity =
			Noise::LatticeHash(W.Config().Seed ^ DocumentSalt, static_cast<int32>(D.Index), static_cast<int32>(About));
		const EntityHandle H = W.CreateEntity(IdKind::Document);
		W.Components().GetPool(Documents.Document).Add(H, D);
		W.Events().Publish(
			Now, DocumentWrittenEvent,
			Player::ActPayload{Writer, D.Index, About, static_cast<uint32>(D.Says < 0 ? -D.Says : D.Says)},
			W.Entities().GetId(H));
		return D.Index;
	}

	uint32 CopyDocument(World& W, const DocumentTypes& Documents, uint32 Document, uint32 Holder, SimTick Now)
	{
		const EntityHandle From = DocumentHandle(W, Documents, Document);
		if (From.IsNull())
		{
			return 0;
		}
		DocumentInfo* Original = W.Components().GetPool(Documents.Document).TryGet(From);
		if (Original == nullptr || Original->Lost != 0)
		{
			return 0; // a lost document is not copied out of anybody's memory
		}
		DocumentInfo D = *Original;
		D.Index = NextIndex(W, Documents);
		D.From = Original->Index;
		D.Holder = Holder;
		D.Copies = 0;
		D.Written = Now;
		// Says, Writer, About and the writing's date are NOT changed. A copy is
		// worth exactly what the original was worth, however wrong it has
		// become - which is what makes a copy dangerous rather than harmless.
		D.Identity = Noise::LatticeHash(W.Config().Seed ^ DocumentSalt, static_cast<int32>(D.Index),
										static_cast<int32>(Original->Index));
		++Original->Copies;
		const EntityHandle H = W.CreateEntity(IdKind::Document);
		W.Components().GetPool(Documents.Document).Add(H, D);
		W.Events().Publish(Now, DocumentCopiedEvent, Player::ActPayload{Holder, D.Index, Original->Index, 0u},
						   W.Entities().GetId(H));
		return D.Index;
	}

	bool ReadDocument(World& W, const Population::PersonTypes& Persons, const ReputeTypes& Repute,
					  const DocumentTypes& Documents, const DocumentRules& Rules, uint32 Document, uint32 Reader,
					  SimTick Now)
	{
		const EntityHandle DH = DocumentHandle(W, Documents, Document);
		if (DH.IsNull() || Reader == 0)
		{
			return false;
		}
		const DocumentInfo* D = W.Components().GetPool(Documents.Document).TryGet(DH);
		if (D == nullptr || D->Lost != 0 || D->About == Reader)
		{
			return false; // lost, or the reader is who it speaks of
		}
		const EntityHandle AH = HandleOf(W, Persons, D->About);
		if (AH.IsNull())
		{
			return false;
		}
		PersonRepute* R = W.Components().GetPool(Repute.Repute).TryGet(AH);
		if (R == nullptr)
		{
			PersonRepute Fresh;
			Fresh.Since = Now;
			W.Components().GetPool(Repute.Repute).Add(AH, Fresh);
			R = W.Components().GetPool(Repute.Repute).TryGet(AH);
			if (R == nullptr)
			{
				return false;
			}
		}
		const int64 Worth = int64{D->Says} * int64{Rules.ReadPerMille} / 1000;
		// The reader's opinion of the subject moves by what the page says, and
		// the page says what its writer thought on the day - not what the writer
		// thinks now, and not what is true.
		// The same slot the system uses, eviction and all. A second copy of this
		// got it wrong: without the oldest-first eviction a page could teach
		// nobody anything about a person already thought of by the full handful,
		// which is every person worth writing about.
		Player::Opinion* Slot = SlotFor(*R, Reader, Now);
		if (Slot == nullptr)
		{
			return false;
		}
		const ReputeRules Scale;
		const int64 Moved = int64{Slot->Regard} + Worth;
		Slot->Regard = static_cast<int32>(std::min<int64>(Scale.Most, std::max<int64>(Scale.Least, Moved)));
		Slot->Last = Now;
		++R->Heard;
		W.Events().Publish(
			Now, DocumentReadEvent,
			Player::ActPayload{Reader, D->Index, D->About, static_cast<uint32>(Worth < 0 ? -Worth : Worth)},
			W.Entities().GetId(DH));
		return true;
	}

	bool LoseDocument(World& W, const DocumentTypes& Documents, uint32 Document, SimTick Now)
	{
		const EntityHandle H = DocumentHandle(W, Documents, Document);
		if (H.IsNull())
		{
			return false;
		}
		DocumentInfo* D = W.Components().GetPool(Documents.Document).TryGet(H);
		if (D == nullptr || D->Lost != 0)
		{
			return false;
		}
		D->Lost = Now;
		W.Events().Publish(Now, DocumentLostEvent, Player::ActPayload{D->Writer, D->Index, D->About, 0u},
						   W.Entities().GetId(H));
		return true;
	}

	const DocumentInfo* DocumentOf(const World& W, const DocumentTypes& Documents, uint32 Document)
	{
		const EntityHandle H = DocumentHandle(W, Documents, Document);
		return H.IsNull() ? nullptr : W.Components().GetPool(Documents.Document).TryGet(H);
	}

	void NameDocument(const World& W, const History::PreHistoryTypes& Types, const DocumentTypes& Documents,
					  uint32 Document, std::string& Out)
	{
		(void)Types;
		const DocumentInfo* D = DocumentOf(W, Documents, Document);
		if (D == nullptr)
		{
			Out += "a page nobody has";
			return;
		}
		char Buffer[128];
		std::snprintf(Buffer, sizeof(Buffer), "%s %llu, written in year %llu",
					  D->From == 0 ? "the account" : "a copy of the account", static_cast<unsigned long long>(D->Index),
					  static_cast<unsigned long long>(D->Written / History::TicksPerYear));
		Out += Buffer;
	}

	DocumentStats MeasureDocuments(const World& W, const Population::PersonTypes& Persons,
								   const DocumentTypes& Documents)
	{
		DocumentStats Out;
		Hash64 Digest = HashConstants::Fnv1a64Offset;
		W.Components()
			.GetPool(Documents.Document)
			.ForEach(
				[&](EntityHandle, const DocumentInfo& D)
				{
					++Out.Written;
					Out.Standing += D.Lost == 0 ? 1u : 0u;
					Out.Copies += D.From != 0 ? 1u : 0u;
					const EntityHandle WH = HandleOf(W, Persons, D.Writer);
					const Population::PersonInfo* P =
						WH.IsNull() ? nullptr : W.Components().GetPool(Persons.Person).TryGet(WH);
					const bool Gone = P == nullptr || P->State != static_cast<uint8>(Population::LifeState::Alive);
					Out.Orphaned += Gone && D.Lost == 0 ? 1u : 0u;
					Digest = HashCombine(Digest, HashBytes(reinterpret_cast<const char*>(&D), sizeof(DocumentInfo)));
				});
		for (const Event& E : W.Log().All())
		{
			Out.Read += E.Is(DocumentReadEvent) ? 1u : 0u;
		}
		Out.Digest = Digest;
		return Out;
	}
} // namespace Vaelen::Gameplay
