// VAELEN - Tools/UiWitness (14.07). Not built by anything: see the README.
//
// The one place a world is held. This file may name Vaelen::Run and Take.h -
// it is the module that owns the simulation - and the fence reads the module's
// Public directory, not this. Its header exposes views and nothing else.
//
// STATUS: UNVERIFIED (Phase 14) - parsed against Tools/EngineShim, built by nothing
#include "VaelenWorldSubsystem.h"

#include "Vaelen/Run/Aelvor.h"
#include "Vaelen/Run/Door.h"
#include "Vaelen/View/Take.h"

namespace
{
	Vaelen::Run::Aelvor* TheWorld = nullptr;
	Vaelen::Run::Door* TheDoor = nullptr;
	Vaelen::WorldGen::RegionGraphCache Ways;
	Vaelen::View::ChronicleView Told;
} // namespace

void UVaelenWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Frames = 0;
}

void UVaelenWorldSubsystem::Deinitialize()
{
	TheDoor = nullptr;
	TheWorld = nullptr;
	Super::Deinitialize();
}

const Vaelen::View::PanelView& UVaelenWorldSubsystem::Page()
{
	if (TheWorld != nullptr)
	{
		Vaelen::View::WorldView Frame;
		Vaelen::View::LifeView Life;
		Vaelen::View::TakeView(TheWorld->Instance(), TheWorld->Sources(), Frame);
		Vaelen::View::TakeLifeView(TheWorld->Instance(), TheWorld->Sources(), Ways, Life);
		Vaelen::View::TakeChronicleView(TheWorld->Instance(), TheWorld->Sources(), Told);
		Vaelen::View::TakePanel(Frame, Life, Told, Panel_);
	}
	++Frames;
	return Panel_;
}

Vaelen::Player::Refusal UVaelenWorldSubsystem::Mean(const Vaelen::Player::PlayerCommand& What)
{
	return TheDoor != nullptr ? TheDoor->Mean(What) : Vaelen::Player::Refusal::NoPlayer;
}

void UVaelenWorldSubsystem::TurnTheDay()
{
	if (TheDoor != nullptr)
	{
		TheDoor->Day();
	}
}
