#pragma once
#ifndef GAME_TEST_ACCESS_H
#define GAME_TEST_ACCESS_H

#include "game.h"

// The one test seam game.h befriends. It lives here, in one header, because a struct is defined
// once for the whole binary: two test files each defining their own GameTestAccess with different
// members would be an ODR violation the linker does not report.
struct GameTestAccess {
	static void tax(Game *g, Unit *u) { g->ProcessTaxOrder(u, 0); }
	static void entertain(Game *g, Unit *u) { g->ProcessEntertainOrder(u, 0); }
	static void doBuy(Game *g, ARegion *r, Market *m) { g->DoBuy(r, m); }
	// Game() leaves guardfaction unset until a world is made or read.
	static void setGuardFaction(Game *g, int num) { g->guardfaction = num; }
};

#endif
