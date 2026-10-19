#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "unit.h"
#include "faction.h"
#include "game_test_access.h"

#include <set>
#include <vector>

// boost::ut exposes an `events` namespace that collides with the game's Events class, so a
// file-scope `using namespace boost::ut;` will not compile. Alias it, and pull the literals
// in inside the suite body where the collision does not apply.
namespace ut = boost::ut;

namespace {
	// Game() pushes the faction types onto the process-wide FactionTypes vector (as it does
	// once per real run). Undo that so faction_test.cpp, which sets them up itself, sees no
	// difference whichever suite initialises first.
	Game *makeGame()
	{
		size_t before = FactionTypes->size();
		Game *g = new Game();
		FactionTypes->resize(before);
		return g;
	}

	// A bare region with one object to stand in. Only what the cascade reads is set: the town
	// pointer and the units.
	ARegion *makeRegion(bool town)
	{
		ARegion *reg = new ARegion();
		reg->town = town ? new TownInfo() : nullptr;
		Object *o = new Object(reg);
		o->num = 1;
		o->type = O_DUMMY;
		reg->objects.Add(o);
		return reg;
	}

	void stand(ARegion *reg, Faction *fac, int num)
	{
		Unit *u = new Unit(num, fac, 0);
		u->MoveUnit((Object *) reg->objects.First());
	}
}

// The arrival cascade of MOVE IN on this tree, and the count `map starts` derives from it
// (master's decision 0026, ported). The expected numbers come from the cascade's rule: towns first,
// empty ones before guarded ones; a guarded town before any other hex; then any hex the guard alone
// holds. A Game that has made no world leaves guardfaction unset, so the seam sets it.
ut::suite<"Gateway start"> gateway_start_suite = []
{
	using namespace ut;

	Game *game = makeGame();
	GameTestAccess::setGuardFaction(game, 1);
	Faction *guard = new Faction(1);
	Faction *player = new Faction(3);

	"An empty town is level 0, a town with only the guard level 1, a guarded town held by someone 2"_test = [=]
	{
		ARegion *empty = makeRegion(true);
		ARegion *guarded = makeRegion(true);
		ARegion *held = makeRegion(true);
		stand(guarded, guard, 100);
		stand(held, guard, 101);
		stand(held, player, 102);

		std::vector<ARegion *> landing;
		std::vector<ARegion *> one = { guarded };
		std::vector<ARegion *> both = { guarded, empty };
		std::vector<ARegion *> taken = { held };
		expect(eq(game->gateway_landing(one, landing, {}), 1));
		expect(eq(game->gateway_landing(both, landing, {}), 0));
		expect(fatal(eq(landing.size(), 1u)));
		expect(landing.front() == empty);
		expect(eq(game->gateway_landing(taken, landing, {}), 2));
	};

	"A hex the guard alone holds is level 3 on this tree"_test = [=]
	{
		ARegion *land = makeRegion(false);
		stand(land, guard, 110);

		std::vector<ARegion *> landing;
		std::vector<ARegion *> only = { land };
		expect(eq(game->gateway_landing(only, landing, {}), 3));
	};

	"With guarded towns the count stops at the towns"_test = [=]
	{
		ARegion *a = makeRegion(true);
		ARegion *b = makeRegion(true);
		ARegion *c = makeRegion(false);
		stand(a, guard, 120);
		stand(b, guard, 121);

		// Both towns take a faction each; the third goes to a guarded town that holds one.
		std::vector<ARegion *> all = { a, b, c };
		expect(eq(game->gateway_start_capacity(all, a), 2));
	};

	"With no guarded town every town and every empty hex takes a faction of its own"_test = [=]
	{
		ARegion *a = makeRegion(true);
		ARegion *b = makeRegion(false);
		ARegion *c = makeRegion(false);
		stand(c, player, 130);

		// The town, then the empty hex; the occupied one is never a start of its own.
		std::vector<ARegion *> all = { a, b, c };
		expect(eq(game->gateway_start_capacity(all, a), 2));
	};
};
