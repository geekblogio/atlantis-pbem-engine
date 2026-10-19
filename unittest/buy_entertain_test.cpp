#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "unit.h"
#include "faction.h"
#include "market.h"
#include "orders.h"
#include "game_test_access.h"

#include <string>

// boost::ut exposes an `events` namespace that collides with the game's Events class, so a
// file-scope `using namespace boost::ut;` will not compile. Alias it, and pull the literals
// in inside the suite body where the collision does not apply.
namespace ut = boost::ut;

// The order handlers under test are private to Game; the seam game.h befriends is shared.

namespace {
	// Restores a Globals field however the test exits (a fatal() abort included), so a
	// flipped flag cannot leak into another suite.
	struct GlobalsGuard {
		int *field;
		int saved;
		GlobalsGuard(int *f, int value) : field(f), saved(*f) { *field = value; }
		~GlobalsGuard() { *field = saved; }
	};

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

	Object *addObject(ARegion *reg)
	{
		Object *o = new Object(reg);
		o->num = 1;
		o->type = O_DUMMY;
		reg->objects.Add(o);
		return o;
	}

	Unit *addUnit(Object *obj, int num, Faction *fac, int silver)
	{
		Unit *u = new Unit(num, fac, 0);
		u->MoveUnit(obj);
		u->items.SetNum(I_LEADERS, 1);
		if (silver) u->items.SetNum(I_SILVER, silver);
		return u;
	}

	void addBuy(Unit *u, int item, int num)
	{
		BuyOrder *o = new BuyOrder;
		o->item = item;
		o->num = num; // -1 is BUY ALL
		u->buyorders.Add(o);
	}

	bool hasError(Faction *fac, const std::string &needle)
	{
		forlist(&fac->errors) {
			std::string line = ((AString *) elem)->Str();
			if (line.find(needle) != std::string::npos) return true;
		}
		return false;
	}

	// A plain char array, not a std::string: Boost.UT runs the tests from its runner's destructor
	// at exit, after namespace-scope std::string objects of this file have been destroyed.
	constexpr const char *CANNOT_AFFORD = "BUY: Unit attempted to buy more than it could afford.";
}

// ENTERTAIN after TAX/PILLAGE. With TAX_PILLAGE_MONTH_LONG, taxing is a month-long order kept in
// Unit::taxing rather than in monthorders; every other month order clears it, ENTERTAIN did not,
// so a unit with TAX then ENTERTAIN collected both incomes.
ut::suite<"ENTERTAIN replaces TAX"> entertain_tax_suite = []
{
	using namespace ut;

	Game *game = makeGame();

	"TAX then ENTERTAIN leaves only the entertain order"_test = [game]
	{
		GlobalsGuard monthlong(&Globals->TAX_PILLAGE_MONTH_LONG, 1);
		Faction *fac = new Faction(3);
		Unit *u = new Unit(100, fac, 0);

		GameTestAccess::tax(game, u);
		expect(eq(u->taxing, (int) TAX_TAX)) << "precondition: TAX sets the flag";

		GameTestAccess::entertain(game, u);
		expect(eq(u->taxing, (int) TAX_NONE)) << "ENTERTAIN must cancel the earlier TAX";
		expect(fatal(u->monthorders != nullptr));
		expect(eq(u->monthorders->type, (int) O_PRODUCE));
		ProduceOrder *p = (ProduceOrder *) u->monthorders;
		expect(eq(p->item, (int) I_SILVER));
		expect(eq(p->skill, (int) S_ENTERTAINMENT));
		expect(hasError(fac, "ENTERTAIN: Overwriting previous month-long order."))
			<< "the overwrite is still reported";
	};

	"PILLAGE then ENTERTAIN leaves only the entertain order"_test = [game]
	{
		GlobalsGuard monthlong(&Globals->TAX_PILLAGE_MONTH_LONG, 1);
		Faction *fac = new Faction(3);
		Unit *u = new Unit(101, fac, 0);
		u->taxing = TAX_PILLAGE; // what ProcessPillageOrder sets

		GameTestAccess::entertain(game, u);
		expect(eq(u->taxing, (int) TAX_NONE));
		expect(u->monthorders != nullptr);
	};

	// Pins the order that already worked, so the fix is seen not to disturb it.
	"ENTERTAIN then TAX leaves only the tax order"_test = [game]
	{
		GlobalsGuard monthlong(&Globals->TAX_PILLAGE_MONTH_LONG, 1);
		Faction *fac = new Faction(3);
		Unit *u = new Unit(102, fac, 0);

		GameTestAccess::entertain(game, u);
		GameTestAccess::tax(game, u);
		expect(eq(u->taxing, (int) TAX_TAX));
		expect(u->monthorders == nullptr);
	};

	// Without TAX_PILLAGE_MONTH_LONG, TAX is an instant order and may legitimately be combined
	// with a month order; the fix must not touch that.
	"without TAX_PILLAGE_MONTH_LONG, TAX and ENTERTAIN both stand"_test = [game]
	{
		GlobalsGuard monthlong(&Globals->TAX_PILLAGE_MONTH_LONG, 0);
		Faction *fac = new Faction(3);
		Unit *u = new Unit(103, fac, 0);

		GameTestAccess::tax(game, u);
		GameTestAccess::entertain(game, u);
		expect(eq(u->taxing, (int) TAX_TAX));
		expect(u->monthorders != nullptr);
		expect(!hasError(fac, "Overwriting"));
	};
};

// BUY: GetBuyAmount checks every order of a market against the purse as it stood before any of
// them was paid, and ConsumeSharedMoney stops silently at an empty purse. Before the fix several
// orders on one purse were all delivered and only the first was paid.
ut::suite<"BUY pays for every order"> buy_purse_suite = []
{
	using namespace ut;

	Game *game = makeGame();
	const int PRICE = 10;

	// Unlimited market: no random rounding involved, every number below is exact.
	auto unlimited = [PRICE]() { return new Market(M_BUY, I_SWORD, PRICE, -1, 0, 0, 0, 0); };

	"one affordable BUY is unchanged"_test = [game, unlimited]
	{
		ARegion *reg = new ARegion();
		Faction *fac = new Faction(3);
		Unit *u = addUnit(addObject(reg), 100, fac, 100);
		addBuy(u, I_SWORD, 5);

		GameTestAccess::doBuy(game, reg, unlimited());
		expect(u->items.GetNum(I_SWORD) == 5_i);
		expect(u->items.GetNum(I_SILVER) == 50_i);
		expect(fac->errors.Num() == 0_i);
	};

	"two BUY orders in one unit: the second is capped to what is left"_test = [game, unlimited]
	{
		ARegion *reg = new ARegion();
		Faction *fac = new Faction(3);
		Unit *u = addUnit(addObject(reg), 100, fac, 100);
		addBuy(u, I_SWORD, 10);
		addBuy(u, I_SWORD, 10);

		GameTestAccess::doBuy(game, reg, unlimited());
		// before the fix: 20 swords for 100 silver
		expect(u->items.GetNum(I_SWORD) == 10_i);
		expect(u->items.GetNum(I_SILVER) == 0_i);
		expect(hasError(fac, CANNOT_AFFORD));
	};

	"BUY ALL twice buys one purse's worth"_test = [game, unlimited]
	{
		ARegion *reg = new ARegion();
		Faction *fac = new Faction(3);
		Unit *u = addUnit(addObject(reg), 100, fac, 95);
		addBuy(u, I_SWORD, -1);
		addBuy(u, I_SWORD, -1);

		GameTestAccess::doBuy(game, reg, unlimited());
		expect(u->items.GetNum(I_SWORD) == 9_i);
		expect(u->items.GetNum(I_SILVER) == 5_i);
	};

	"two buyers on one SHARE purse pay for what they get"_test = [game, unlimited]
	{
		ARegion *reg = new ARegion();
		Faction *fac = new Faction(3);
		Object *obj = addObject(reg);
		Unit *a = addUnit(obj, 100, fac, 0);
		Unit *b = addUnit(obj, 101, fac, 0);
		Unit *sharer = addUnit(obj, 102, fac, 100);
		sharer->SetFlag(FLAG_SHARING, 1);
		addBuy(a, I_SWORD, 10);
		addBuy(b, I_SWORD, 10);

		GameTestAccess::doBuy(game, reg, unlimited());
		// before the fix: a and b got 10 each, the sharer paid for a only
		expect(a->items.GetNum(I_SWORD) == 10_i);
		expect(b->items.GetNum(I_SWORD) == 0_i);
		expect(sharer->items.GetNum(I_SILVER) == 0_i);
		expect(hasError(fac, CANNOT_AFFORD));
	};

	// Limited stock: the share-out draws random numbers. Whatever they are, what changes hands
	// must be paid for, and the draws themselves are not affected by the cap.
	"limited market: every item delivered is paid for"_test = [game, PRICE]
	{
		seedrandom(1);
		ARegion *reg = new ARegion();
		Faction *fac = new Faction(3);
		Unit *u = addUnit(addObject(reg), 100, fac, 100);
		addBuy(u, I_SWORD, 10);
		addBuy(u, I_SWORD, 10);
		Market *m = new Market(M_BUY, I_SWORD, PRICE, 20, 0, 0, 0, 0);

		GameTestAccess::doBuy(game, reg, m);
		int bought = u->items.GetNum(I_SWORD);
		int spent = 100 - u->items.GetNum(I_SILVER);
		expect(eq(spent, bought * PRICE));
		expect(le(bought, 10));
		expect(m->amount == 20_i) << "DoBuy restores the market's stock";
	};
};
