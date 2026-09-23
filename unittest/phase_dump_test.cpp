#include "../external/boost/ut.hpp"

#include "../game.h"
#include "../rng.hpp"
#include "testhelper.hpp"

#include <sstream>
#include <string>
#include <vector>

// Because boost::ut has it's own concept of events, as does Game, we cannot just use do
// using namespace boost::ut; here. Instead, we alias it, and then use the alias inside the
// closure to make the user defined literals and all the other niceness available.
namespace ut = boost::ut;

// ATLANTIS_PHASE_DUMPS writes the world after every phase of a turn through Game::write_game, the
// body of SaveGame. The files are only worth having if writing them leaves the turn alone, so the
// property that matters is that write_game draws nothing and changes nothing.
ut::suite<"Phase dumps"> phase_dump_suite = []
{
    using namespace ut;

    "writing the world draws nothing from the generator"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        rng::seed_random(1234);
        int without = rng::get_random(1000000);

        rng::seed_random(1234);
        helper.write_game(0);
        int with = rng::get_random(1000000);

        expect(with == without) << "write_game must consume no draws";
    };

    "writing the world twice writes the same text"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        expect(helper.write_game(0) == helper.write_game(0)) << "write_game must not change the world";
    };

    "the seed line carries the seed it is given"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        std::istringstream text(helper.write_game(4711));
        std::vector<std::string> lines;
        std::string line;
        while (lines.size() < 7 && std::getline(text, line)) lines.push_back(line);

        // atlantis_game, the engine version, the ruleset name and version, year, month, seed.
        expect(lines.size() == 7_ul);
        expect(lines[0] == std::string("atlantis_game"));
        expect(lines[6] == std::string("4711")) << "the seventh line is the seed";
    };
};
