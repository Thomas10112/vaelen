// VAELEN - Tests/View
// Phase 14.02: the PROBE. Every view header but Take.h, compiled with ONLY
// VaelenCore/Public, VaelenView/Public and VaelenPlayer/Public on the include
// path and linked to nothing. If any leaf ever grows an include that reaches
// the kernel again, this stops compiling and CTest View.Leaf is red before a
// UI is written against it. Compiling IS the test; running it prints one line.
//
// STATUS: PROTOTYPE (Phase 14)
#include "Vaelen/View/Chronicle.h"
#include "Vaelen/View/Climate.h"
#include "Vaelen/View/Delta.h"
#include "Vaelen/View/Eye.h"
#include "Vaelen/View/Folk.h"
#include "Vaelen/View/Frame.h"
#include "Vaelen/View/Land.h"
#include "Vaelen/View/Life.h"
#include "Vaelen/View/Net.h"
#include "Vaelen/View/Panel.h"
#include "Vaelen/View/ViewApi.h"

#include "Vaelen/Player/Intent.h"

#include <cstdio>

int main()
{
	using namespace Vaelen::View;
	WorldView Frame;
	MapView Map;
	NetView Net;
	PeopleView People;
	Eye At;
	LifeView Life;
	ChronicleView Chronicle;
	PanelView Panel;
	ClimateView Climate;
	Vaelen::Player::PlayerCommand C;
	std::printf("[probe] the view headers are leaves: frame %u, map %u, net %u, people %u, eye %u, command %u, life "
				"%u, chronicle %u, panel %u, climate %u bytes\n",
				static_cast<unsigned>(sizeof(Frame)), static_cast<unsigned>(sizeof(Map)),
				static_cast<unsigned>(sizeof(Net)), static_cast<unsigned>(sizeof(People)),
				static_cast<unsigned>(sizeof(At)), static_cast<unsigned>(sizeof(C)),
				static_cast<unsigned>(sizeof(Life)), static_cast<unsigned>(sizeof(Chronicle)),
				static_cast<unsigned>(sizeof(Panel)), static_cast<unsigned>(sizeof(Climate)));
	return 0;
}
