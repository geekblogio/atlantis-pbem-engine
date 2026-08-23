#include "external/boost/ut.hpp"

#include "rng.hpp"

namespace ut = boost::ut;

// The randomness the engine hands out is a published interface in everything but name: every
// recorded seed, every snapshot fixture and every running game depends on these exact values. The
// standard library used to supply them, and did not supply the same ones everywhere -- see 0019.
// These tests pin what rng.hpp now produces, so that a rewrite of the arithmetic or a change of
// compiler is a failing test rather than a quietly different world.
ut::suite<"RNG"> rng_suite = [] {
    using namespace ut;

    "weighted index picks the recorded elements"_test = [] {
        // The values libstdc++'s std::discrete_distribution produces for this seed, which is what
        // rng.hpp reproduces. Verified against GCC 13.3 and 14.2.
        rng::seed_random(42);
        std::vector<unsigned int> three{1, 2, 3};
        std::vector<size_t> picked;
        for (int i = 0; i < 10; i++) picked.push_back(rng::get_weighted_index(three).value());

        expect(picked == std::vector<size_t>{2, 1, 2, 1, 0, 2, 1, 2, 1, 1}) << "recorded sequence";
    };

    "a single weight is chosen without touching the generator"_test = [] {
        // THE DEFECT THIS GUARDS. libstdc++ returns 0 for a one-element weight vector without
        // drawing at all; libc++ drew twice. Half the weighted picks in a kingdoms world creation
        // have one candidate, so the two platforms built different worlds from the same seed.
        rng::seed_random(1234);
        int without = rng::get_random(1000000);

        rng::seed_random(1234);
        std::vector<unsigned int> one{7};
        expect(rng::get_weighted_index(one).value() == 0_ul);
        int with = rng::get_random(1000000);

        expect(with == without) << "a single weight must consume no draws";
    };

    "an empty or weightless container yields nothing"_test = [] {
        std::vector<unsigned int> none{};
        std::vector<unsigned int> zeroes{0, 0, 0};
        expect(!rng::get_weighted_index(none).has_value());
        expect(!rng::get_weighted_index(zeroes).has_value());
    };

    "a zero weight is never picked"_test = [] {
        rng::seed_random(99);
        std::vector<unsigned int> edges{0, 5, 0};
        for (int i = 0; i < 50; i++) expect(rng::get_weighted_index(edges).value() == 1_ul);
    };

    "item decay keeps its recorded values"_test = [] {
        // calculate_losses() is the one draw that reaches libm, through detail::binomial(). Both
        // of its branches are pinned: the waiting-time method below t * p == 8, and Devroye's
        // rejection method above it.
        rng::seed_random(2026);
        std::vector<int> small;
        for (int i = 0; i < 6; i++) small.push_back(rng::calculate_losses(20, 8));   // t*p = 1.6
        expect(small == std::vector<int>{1, 0, 2, 0, 2, 3}) << "waiting-time branch";

        rng::seed_random(2026);
        std::vector<int> large;
        for (int i = 0; i < 6; i++) large.push_back(rng::calculate_losses(500, 8));  // t*p = 40
        expect(large == std::vector<int>{42, 46, 31, 38, 50, 39}) << "rejection branch";

        expect(rng::calculate_losses(100, 0) == 0_i) << "no chance, no losses";
        expect(rng::calculate_losses(100, 100) == 100_i) << "certain loss takes everything";
        expect(rng::calculate_losses(0, 50) == 0_i) << "nothing to lose";
    };

    "uniform draws and the shuffle keep their recorded values"_test = [] {
        rng::seed_random(7);
        std::vector<int> rolls;
        for (int i = 0; i < 8; i++) rolls.push_back(rng::get_random(100));
        expect(rolls == std::vector<int>{38, 52, 12, 87, 63, 42, 14, 48}) << "recorded draws";

        rng::seed_random(7);
        std::vector<int> deck{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        rng::shuffle(deck);
        expect(deck == std::vector<int>{7, 2, 3, 5, 1, 8, 6, 9, 0, 4}) << "recorded shuffle";
    };
};
