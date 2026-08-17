#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

// Because boost::ut has it's own concept of events, as does Game, we cannot just use do
// using namespace boost::ut; here. Instead, we alias it, and then use the alias inside the
// closure to make the user defined literals and all the other niceness available.
namespace ut = boost::ut;

namespace {
    int count_markets(ARegion *region, Market::MarketType type, int item) {
        return static_cast<int>(std::count_if(region->markets.begin(), region->markets.end(),
            [type, item](Market *m) { return m->type == type && m->item == item; }));
    }

    int count_man_markets(ARegion *region) {
        return static_cast<int>(std::count_if(region->markets.begin(), region->markets.end(),
            [](Market *m) { return ItemDefs[m->item].type & IT_MAN; }));
    }

    // Globals is one shared object for the whole binary, so a test that changes a rule has to put it
    // back or it leaks into every suite that runs afterwards.
    struct ScopedStartCities {
        int saved;
        explicit ScopedStartCities(int value) : saved(Globals->START_CITIES_EXIST) {
            Globals->START_CITIES_EXIST = value;
        }
        ~ScopedStartCities() { Globals->START_CITIES_EXIST = saved; }
    };
}

// MakeStartingCity() replaces a region's town, and add_town() appends a market block rather than
// replacing one. A region that already had a town therefore used to end up with two complete blocks,
// of which only the first could be traded with.
ut::suite<"Starting city"> starting_city_suite = []
{
  using namespace ut;

  "A starting city built over an existing town keeps one market block"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();

    // The test world already made this region a starting city, so it has a town and a market block.
    ARegion *region = helper.get_region(0, 0, 0);
    expect(region->town != nullptr);
    expect(count_markets(region, Market::MarketType::M_SELL, I_GRAIN) == 1_i);

    int men_before = count_man_markets(region);
    expect(men_before > 0_i); // recruiting: the local race, and leaders where they exist

    {
        // Five of the seven rulesets set this to 0, and that is the path where the bug lived: the
        // clear-and-rebuild further down MakeStartingCity() sits below the early return.
        ScopedStartCities no_start_cities(0);
        region->MakeStartingCity();
    }

    // A town sells grain, always, so a second block is not a matter of what the market roll picked.
    expect(count_markets(region, Market::MarketType::M_SELL, I_GRAIN) == 1_i);

    // And the region can still recruit: those markets belong to it rather than to the town it lost,
    // and nothing on this path would build them again.
    expect(count_man_markets(region) == men_before);
  };

  "Rebuilding a starting city leaves no market listed twice"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();

    ARegion *region = helper.get_region(0, 0, 0);

    {
        ScopedStartCities no_start_cities(0);
        region->MakeStartingCity();
    }

    std::set<std::pair<int, int>> seen;
    int repeats = 0;
    for (const auto m : region->markets) {
        if (!seen.insert({ static_cast<int>(m->type), m->item }).second) repeats++;
    }
    expect(repeats == 0_i);
  };
};
