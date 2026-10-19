#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

// Because boost::ut has it's own concept of events, as does Game, we cannot just use do
// using namespace boost::ut; here. Instead, we alias it, and then use the alias inside the
// closure to make the user defined literals and all the other niceness available.
namespace ut = boost::ut;

namespace {
    // The three surface hexes of the test world that are not its starting city — (0,0) carries a
    // town and whatever the world put there, and these tests want to decide everything themselves.
    struct Hexes {
        ARegion *a;
        ARegion *b;
        ARegion *c;
    };

    Hexes hexes(UnitTestHelper& helper) {
        return { helper.get_region(1, 1, 0), helper.get_region(0, 2, 0), helper.get_region(1, 3, 0) };
    }

    Faction *guards(UnitTestHelper& helper) {
        Faction *guard = helper.create_faction("Guards");
        helper.set_guard_faction(guard);
        return guard;
    }
}

// The arrival cascade of MOVE IN, and the count `map starts` derives from it (Game::gateway_landing,
// Game::gateway_start_capacity). The expected numbers are worked out from the cascade's rule, not
// read back from the function: towns first, empty ones before guarded ones; a guarded town that
// already holds a faction before any empty hex; an empty hex before anything at all.
ut::suite<"Gateway start"> gateway_start_suite = []
{
  using namespace ut;

  "An empty town is level 0, a town with only the guard level 1"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    Faction *guard = guards(helper);
    auto [a, b, c] = hexes(helper);
    expect(fatal(a != nullptr && b != nullptr && c != nullptr));
    a->town = new TownInfo();
    b->town = new TownInfo();
    helper.create_unit(guard, b);

    std::vector<ARegion *> landing;

    expect(helper.gateway_landing({ b }, landing) == 1_i);
    expect(helper.gateway_landing({ a, b }, landing) == 0_i);
    expect(fatal(landing.size() == 1_ul));
    expect(landing.front() == a);
  };

  "A guarded town that holds a faction comes before an empty hex"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    Faction *guard = guards(helper);
    Faction *player = helper.create_faction("Player");
    auto [a, b, c] = hexes(helper);
    expect(fatal(a != nullptr && b != nullptr && c != nullptr));
    a->town = new TownInfo();
    helper.create_unit(guard, a);
    helper.create_unit(player, a);

    std::vector<ARegion *> landing;
    expect(helper.gateway_landing({ a, c }, landing) == 2_i);
    expect(fatal(landing.size() == 1_ul));
    expect(landing.front() == a);
  };

  "A claimed hex counts as held by somebody else"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    auto [a, b, c] = hexes(helper);
    expect(fatal(a != nullptr && c != nullptr));
    a->town = new TownInfo();

    std::vector<ARegion *> landing;
    // The only town is taken, nothing is guarded: the empty hex is next.
    expect(helper.gateway_landing({ a, c }, landing, { a }) == 3_i);
    expect(fatal(landing.size() == 1_ul));
    expect(landing.front() == c);
  };

  "With no guarded town, every town and then every empty hex takes a faction of its own"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    guards(helper);
    auto [a, b, c] = hexes(helper);
    expect(fatal(a != nullptr && b != nullptr && c != nullptr));
    a->town = new TownInfo();
    b->town = new TownInfo();

    // Two towns, one empty hex: three arrivals land alone, the fourth shares.
    expect(helper.gateway_start_capacity({ a, b, c }, a) == 3_i);
  };

  "One guarded town stops the count at the towns"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    Faction *guard = guards(helper);
    auto [a, b, c] = hexes(helper);
    expect(fatal(a != nullptr && b != nullptr && c != nullptr));
    a->town = new TownInfo();
    b->town = new TownInfo();
    helper.create_unit(guard, b);

    // Both towns take a faction each; the third arrival goes to the guarded town, which by then
    // holds one, before the empty hex is ever tried.
    expect(helper.gateway_start_capacity({ a, b, c }, a) == 2_i);
  };

  "A town somebody already stands in is not a start of its own"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    guards(helper);
    Faction *monsters = helper.create_faction("Monsters");
    auto [a, b, c] = hexes(helper);
    expect(fatal(a != nullptr && b != nullptr && c != nullptr));
    a->town = new TownInfo();
    b->town = new TownInfo();
    helper.create_unit(monsters, b);

    // The occupied town is skipped at every level that hands out a hex of one's own; the empty
    // town and the empty hex remain.
    expect(helper.gateway_start_capacity({ a, b, c }, a) == 2_i);
  };

  "With no destination the arrival stays where the gateway points"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    Faction *player = helper.create_faction("Player");
    auto [a, b, c] = hexes(helper);
    expect(fatal(a != nullptr && c != nullptr));

    expect(helper.gateway_start_capacity({}, c) == 1_i);
    helper.create_unit(player, c);
    expect(helper.gateway_start_capacity({}, c) == 0_i);
  };
};
