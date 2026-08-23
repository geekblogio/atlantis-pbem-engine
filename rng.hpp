#pragma once
#ifndef _RNG_HPP
#define _RNG_HPP

#include <random>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <optional>
#include <type_traits>
#include <iterator>
#include <concepts>
#include <cmath>

namespace rng {
namespace detail {

// Returns a reference to the lazily initialized generator
inline std::mt19937& get_initialized_generator() {
    // This static generator is initialized only on the first call to this function.
    static std::mt19937 twister = [] {
        std::minstd_rand minstd_rand(std::random_device{}());
        std::seed_seq seed_seq{
            minstd_rand(), minstd_rand(), minstd_rand(),
            minstd_rand(), minstd_rand(), minstd_rand()
        };
        return std::mt19937(seed_seq);
    }();
    return twister;
}

// Seeds the generator
inline void seed_generator(std::seed_seq& seed_seq) {
    get_initialized_generator().seed(seed_seq);
}

// Draws a value in [0, bound), consuming raw generator output exactly as libstdc++'s
// std::uniform_int_distribution does.
//
// WHY THIS IS WRITTEN OUT INSTEAD OF USING THE STANDARD DISTRIBUTION. std::mt19937 is specified
// bit for bit and produces the same stream everywhere. The DISTRIBUTIONS ARE NOT SPECIFIED. Each
// standard library picks its own mapping from raw draws to values, and the mappings differ in
// what they return *and* in how many raw draws they consume -- so once one is used, the two
// libraries' streams diverge permanently. Measured on one seed, ten draws of range 6:
//
//     libstdc++ (GCC/Linux)   0 3 5 0 3 3 0 4 2 1     10 raw draws
//     libc++    (clang/macOS) 1 4 5 4 1 2 3 0 0 1     15 raw draws
//
// That is why a recorded seed rebuilt a different world on an arm64 Mac than on the x86-64
// servers, and it is a far larger effect than the argument-order defect 0017 describes: that one
// swapped two draws, this one replaces the entire stream.
//
// The algorithm below is Lemire's multiply-shift, which is what libstdc++ uses. It is reproduced
// here rather than depended upon, so the numbers are OURS: they no longer change if a standard
// library is swapped, updated or ported. Verified against libstdc++ under GCC 13.3 (what CI runs)
// and GCC 14.2, for every range from 1 to 4096, all powers of two to 2^30 with their neighbours,
// and assorted large values -- identical results AND identical generator state after each draw.
//
// See docs/decisions/0019.
inline std::uint64_t uniform_below(std::uint64_t bound) {
    auto& gen = get_initialized_generator();

    // mt19937 has min() == 0 and max() == 2^32-1, so a draw is exactly 32 random bits.
    std::uint32_t x = static_cast<std::uint32_t>(gen());
    std::uint64_t m = static_cast<std::uint64_t>(x) * bound;
    std::uint32_t l = static_cast<std::uint32_t>(m);

    // The low half decides fairness: only the first (2^32 mod bound) products are biased, and
    // rejecting exactly those makes the result uniform. The test is skipped entirely unless the
    // draw falls in that window, which is why the common case costs one draw and no division.
    if (l < bound) {
        const std::uint32_t threshold = static_cast<std::uint32_t>((0x100000000ull - bound) % bound);
        while (l < threshold) {
            x = static_cast<std::uint32_t>(gen());
            m = static_cast<std::uint64_t>(x) * bound;
            l = static_cast<std::uint32_t>(m);
        }
    }

    // bound == 1 deliberately falls through the same path: it consumes one draw and yields 0,
    // which is what the standard distribution does for an empty range. Short-circuiting it would
    // silently shift every later draw.
    return m >> 32;
}

// Draws a double in [0, 1), consuming raw generator output exactly as libstdc++'s
// std::generate_canonical<double, 53, std::mt19937> does. Only get_weighted_index() needs it;
// see the comment there for why it is written out rather than called.
//
// mt19937 yields 32 bits per draw and a double carries 53, so libstdc++ takes ceil(53/32) == 2
// draws and reads them as one base-2^32 number. The addition rounds -- 64 bits do not fit in a
// 53-bit mantissa -- but IEEE rounding is specified, so the result is reproducible as long as the
// operations keep this order. Nothing here calls libm.
inline double canonical() {
    auto& gen = get_initialized_generator();

    const double r = 4294967296.0; // mt19937: max() - min() + 1
    double sum = 0.0;
    double tmp = 1.0;
    for (int k = 0; k < 2; k++) {
        sum += static_cast<double>(gen()) * tmp;
        tmp *= r;
    }

    double ret = sum / tmp;
    // libstdc++ guards the rounding case where the quotient reaches 1.0, and so must we: the
    // caller's lower_bound would otherwise run past the last cumulative weight.
    if (ret >= 1.0) ret = std::nextafter(1.0, 0.0);
    return ret;
}


// libstdc++'s std::normal_distribution, written out. Marsaglia's polar method: it draws PAIRS and
// keeps the second value for the next call, so the cache is part of the sequence rather than an
// optimisation. binomial() below draws from it inside a rejection loop, which is exactly where
// that matters. A fresh object has an empty cache, which is the state calculate_losses() starts
// from every time, since it builds a new distribution per call.
struct normal_source {
    double saved = 0.0;
    bool saved_available = false;

    double operator()() {
        if (saved_available) {
            saved_available = false;
            return saved;
        }

        double x, y, r2;
        do {
            x = 2.0 * canonical() - 1.0;
            y = 2.0 * canonical() - 1.0;
            r2 = x * x + y * y;
        } while (r2 > 1.0 || r2 == 0.0);

        const double mult = std::sqrt(-2 * std::log(r2) / r2);
        saved = x * mult;
        saved_available = true;
        return y * mult;
    }
};

// The constants libstdc++ derives once per distribution, for t trials at probability p.
//
// THIS IS THE ONE PLACE IN rng.hpp THAT CALLS libm. log, exp, sqrt and lgamma are not guaranteed
// bit-identical between glibc and Apple's libm, so unlike uniform_below(), shuffle() and
// get_weighted_index() this reproduction is exact on the platform it was copied from and only
// probably exact elsewhere. See docs/decisions/0020 for what was measured.
struct binomial_params {
    double q = 0.0;
    bool easy = true;
    double d1 = 0.0, d2 = 0.0, s1 = 0.0, s2 = 0.0, c = 0.0;
    double a1 = 0.0, a123 = 0.0, s = 0.0, lf = 0.0, lp1p = 0.0;

    binomial_params(int t, double p) {
        const double p12 = p <= 0.5 ? p : 1.0 - p;

        if (t * p12 >= 8) {
            easy = false;
            const double np = std::floor(t * p12);
            const double pa = np / t;
            const double one_p = 1 - pa;

            const double pi_4 = 0.7853981633974483096156608458198757L;
            const double d1x = std::sqrt(np * one_p * std::log(32 * np / (81 * pi_4 * one_p)));
            d1 = std::round(std::max<double>(1.0, d1x));
            const double d2x = std::sqrt(np * one_p * std::log(32 * t * one_p / (pi_4 * pa)));
            d2 = std::round(std::max<double>(1.0, d2x));

            const double spi_2 = 1.2533141373155002512078826424055226L; // sqrt(pi / 2)
            s1 = std::sqrt(np * one_p) * (1 + d1 / (4 * np));
            s2 = std::sqrt(np * one_p) * (1 + d2 / (4 * (t * one_p)));
            c = 2 * d1 / np;
            a1 = std::exp(c) * s1 * spi_2;
            const double a12 = a1 + s2 * spi_2;
            const double s1s = s1 * s1;
            a123 = a12 + (std::exp(d1 / (t * one_p)) * 2 * s1s / d1 * std::exp(-d1 * d1 / (2 * s1s)));
            const double s2s = s2 * s2;
            s = a123 + 2 * s2s / d2 * std::exp(-d2 * d2 / (2 * s2s));
            lf = std::lgamma(np + 1) + std::lgamma(t - np + 1);
            lp1p = std::log(pa / one_p);

            q = -std::log(1 - (p12 - pa) / one_p);
        } else {
            q = -std::log(1 - p12);
        }
    }
};

// The simple waiting-time method, used on its own when t * p < 8 and as the tail of the rejection
// method otherwise.
inline int binomial_waiting(int t, double q) {
    int x = 0;
    double sum = 0.0;

    do {
        if (t == x) return x;
        const double e = -std::log(1.0 - canonical());
        sum += e / (t - x);
        x += 1;
    } while (sum <= q);

    return x - 1;
}

// Devroye's rejection algorithm for t * p >= 8, waiting time below that -- libstdc++'s
// std::binomial_distribution, reproduced draw for draw.
inline int binomial(int t, double p) {
    const double p12 = p <= 0.5 ? p : 1.0 - p;
    const binomial_params param(t, p);
    int ret;

    if (!param.easy) {
        normal_source normal;
        double x;

        const double naf = (1 - std::numeric_limits<double>::epsilon()) / 2;
        const double thr = std::numeric_limits<int>::max() + naf;
        const double np = std::floor(t * p12);

        const double spi_2 = 1.2533141373155002512078826424055226L; // sqrt(pi / 2)
        const double a1 = param.a1;
        const double a12 = a1 + param.s2 * spi_2;
        const double a123 = param.a123;
        const double s1s = param.s1 * param.s1;
        const double s2s = param.s2 * param.s2;

        bool reject;
        do {
            const double u = param.s * canonical();
            double v = 0.0;

            if (u <= a1) {
                const double n = normal();
                const double y = param.s1 * std::abs(n);
                reject = y >= param.d1;
                if (!reject) {
                    const double e = -std::log(1.0 - canonical());
                    x = std::floor(y);
                    v = -e - n * n / 2 + param.c;
                }
            } else if (u <= a12) {
                const double n = normal();
                const double y = param.s2 * std::abs(n);
                reject = y >= param.d2;
                if (!reject) {
                    const double e = -std::log(1.0 - canonical());
                    x = std::floor(-y);
                    v = -e - n * n / 2;
                }
            } else if (u <= a123) {
                const double e1 = -std::log(1.0 - canonical());
                const double e2 = -std::log(1.0 - canonical());

                const double y = param.d1 + 2 * s1s * e1 / param.d1;
                x = std::floor(y);
                v = -e2 + param.d1 * (1 / (t - np) - y / (2 * s1s));
                reject = false;
            } else {
                const double e1 = -std::log(1.0 - canonical());
                const double e2 = -std::log(1.0 - canonical());

                const double y = param.d2 + 2 * s2s * e1 / param.d2;
                x = std::floor(-y);
                v = -e2 - param.d2 * y / (2 * s2s);
                reject = false;
            }

            reject = reject || x < -np || x > t - np;
            if (!reject) {
                const double lfx = std::lgamma(np + x + 1) + std::lgamma(t - (np + x) + 1);
                reject = v > param.lf - lfx + x * param.lp1p;
            }

            reject |= x + np >= thr;
        } while (reject);

        x += np + naf;
        const int z = binomial_waiting(t - static_cast<int>(x), param.q);
        ret = static_cast<int>(x) + z;
    } else {
        ret = binomial_waiting(t, param.q);
    }

    if (p12 != p) ret = t - ret;
    return ret;
}

} // namespace detail

inline void seed_random() {
    // Re-seed with a new random device value - note this creates a new sequence
    std::minstd_rand minstd_rand(std::random_device{}());
    std::seed_seq seed_seq{
        minstd_rand(), minstd_rand(), minstd_rand(),
        minstd_rand(), minstd_rand(), minstd_rand()
    };
    detail::seed_generator(seed_seq);
}

inline void seed_random(int seed) {
    // REDUCE THE SEED HERE RATHER THAN LETTING minstd_rand DO IT.
    //
    // std::minstd_rand's result_type is uint_fast32_t, and that type is NOT the same width
    // everywhere: 32 bits on arm64 macOS, 64 bits on x86-64 Linux. A negative int therefore
    // reaches the engine as two different unsigned values -- truncated on one platform,
    // sign-extended on the other -- and the two streams have nothing to do with each other:
    //
    //     seed 0xdeadbeef, i.e. int -559038737
    //       arm64 macOS  -> 3735928559           -> mt19937 starts 4202512608 ...
    //       x86-64 Linux -> 18446744073150512879 -> mt19937 starts 2983448434 ...
    //
    // Positive seeds are unaffected, which is why every seeded world and every recorded turn
    // agreed while the unit tests -- the one caller passing a value too large for int -- did not.
    //
    // The reduction below is what linear_congruential_engine does internally (s mod m), performed
    // on the sign-extended 64-bit value. That reproduces what x86-64 already produced, so nothing
    // changes there; 0017 settles which platform is the reference when both are equally correct.
    // Passing an already-reduced value is idempotent, so the engine's own reduction is a no-op.
    const std::uint64_t widened = static_cast<std::uint64_t>(static_cast<std::int64_t>(seed));
    const std::uint64_t reduced = widened % 2147483647ull; // minstd_rand's modulus

    std::minstd_rand minstd_rand(static_cast<std::minstd_rand::result_type>(reduced));
    std::seed_seq seed_seq{
        minstd_rand(), minstd_rand(), minstd_rand(),
        minstd_rand(), minstd_rand(), minstd_rand()
    };
    detail::seed_generator(seed_seq); // Seed with a specific value
}

inline int get_random(int range) {
    int neg = (range < 0);
    if (!range) return 0;
    if (neg) range = -range;

    int ret = static_cast<int>(detail::uniform_below(static_cast<std::uint64_t>(range)));
    if (neg) ret = -ret;
    return ret;
}

inline int make_roll(int rolls, int sides) {
    int result = 0;
    for (int i = 0; i < rolls; i++) {
        // Generate a random number in the range [1, sides]
        result += get_random(sides) + 1; // +1 to shift from [0, sides-1] to [1, sides]
    }
    return result;
}

inline std::mt19937& generator() {
    // Return the generator instance
    return detail::get_initialized_generator();
}

// Returns a const reference to a random element from a container
// Requires the container to support .at() and size().
inline const std::string& one_of(const std::vector<std::string>& container) {
    const auto size = container.size();
    if (size == 0) throw std::out_of_range("Cannot select one_of from an empty container.");
    // Ensure range is non-negative for get_random
    if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
         throw std::overflow_error("Container size exceeds representable range for int.");
    }
    int index = get_random(static_cast<int>(size));
    return container.at(index);
}

// Calculates the number of items lost given an amount and a percentage chance of loss per item.
inline int calculate_losses(int amount, int percentage) {
    if (amount <= 0) return 0; // No items to potentially lose

    // Clamp percentage to the valid range [0, 100]
    int clamped_percentage = std::clamp(percentage, 0, 100);

    if (clamped_percentage == 0) return 0; // 0% chance means no losses
    if (clamped_percentage == 100) return amount; // 100% chance means all items are lost


    // Probability of loss for a single item
    double probability = static_cast<double>(clamped_percentage) / 100.0;

    // 'amount' trials, 'probability' chance of losing each -- libstdc++'s binomial distribution,
    // reproduced in detail::binomial() for the reason 0019 gives for the others. This is the one
    // that calls libm, so it is the one whose portability is measured rather than argued: see
    // docs/decisions/0020.
    return detail::binomial(amount, probability);
}

// Shuffles the elements in the given container in place using the internal generator.
//
// Written out for the same reason as uniform_below(): std::shuffle is not specified either, and
// it is built on std::uniform_int_distribution, so it inherited the divergence twice over. This
// reproduces libstdc++'s algorithm exactly, INCLUDING its pairing optimisation -- once the range
// is small enough that two indices fit in one draw, libstdc++ generates them together and the
// draw count halves. A plain Fisher-Yates loop returns different permutations for every size
// above two, so the optimisation is part of the observable behaviour, not an implementation
// detail we may skip. Verified against libstdc++ for sizes 2, 3, 4, 5, 10, 17, 50, 101 and 1000.
template <typename C>
inline void shuffle(C& container) {
    auto first = std::begin(container);
    auto last = std::end(container);
    if (first == last) return;

    using diff_t = typename std::iterator_traits<decltype(first)>::difference_type;
    const std::uint64_t urngrange = 4294967295ull; // mt19937: max() - min()
    const std::uint64_t urange = static_cast<std::uint64_t>(last - first);

    auto i = first + 1;

    // Two indices fit in one draw exactly when urange * urange <= urngrange; written as a
    // division so it cannot wrap.
    if (urngrange / urange >= urange) {
        // An even number of elements means an odd number of swaps, so the first is done alone
        // and the rest go in pairs.
        if ((urange % 2) == 0) {
            std::iter_swap(i++, first + static_cast<diff_t>(detail::uniform_below(2)));
        }
        while (i != last) {
            const std::uint64_t swap_range = static_cast<std::uint64_t>(i - first) + 1;
            const std::uint64_t both = detail::uniform_below(swap_range * (swap_range + 1));
            std::iter_swap(i++, first + static_cast<diff_t>(both / (swap_range + 1)));
            std::iter_swap(i++, first + static_cast<diff_t>(both % (swap_range + 1)));
        }
        return;
    }

    for (; i != last; ++i) {
        std::iter_swap(i, first + static_cast<diff_t>(
            detail::uniform_below(static_cast<std::uint64_t>(i - first) + 1)));
    }
}

// Concept to check if a type is a range with an unsigned integral value type
template <typename T>
concept UnsignedIntegralRange = requires(T c) {
    typename T::value_type;
    requires std::ranges::range<T> || requires { c.begin(); c.end(); };
    requires std::unsigned_integral<typename T::value_type>;
};

// Performs a weighted random selection based on the provided weights.
// Returns an optional containing the index of the selected weight in the input container,
// or std::nullopt if the input is invalid (empty, or all weights are 0).
// The container must satisfy the UnsignedIntegralRange concept.
template <UnsignedIntegralRange WeightContainer>
inline std::optional<size_t> get_weighted_index(const WeightContainer& weights) {

    if (std::ranges::empty(weights)) return std::nullopt;

    unsigned long long weight_sum = 0;
    double weight_total = 0.0;
    size_t count = 0;
    for(const auto& weight : weights) {
        weight_sum += weight;
        weight_total += static_cast<double>(weight); // in libstdc++'s order, so the rounding matches
        count++;
    }

    if (weight_sum == 0) return std::nullopt;

    // WRITTEN OUT FOR THE REASON uniform_below() GIVES, AND FOR ONE MORE.
    //
    // std::discrete_distribution is unspecified like every other distribution, and here the two
    // libraries part company at exactly one point: **a single weight**. libstdc++'s param_type
    // clears the probabilities when there are fewer than two, and operator() then returns 0
    // WITHOUT TOUCHING THE GENERATOR; libc++ draws anyway. Everything from two weights up agreed
    // in every case measured -- which is why 0019 recorded the two as agreeing, having measured
    // where the function does not run.
    //
    // It runs in exactly one place: MakeManUnit(), reached only when LEADERS_EXIST is false, which
    // is true of kingdoms alone. Half of the picks during a kingdoms world creation offer one
    // candidate, so macOS burned two draws per pick that Linux did not, and the same seed built a
    // different world. Measured on a 24x24 kingdoms world, seed 12345: game.out differed between
    // the platforms before this, and is identical after.
    //
    // The reproduction below is libstdc++'s: normalise, accumulate, and lower_bound a canonical
    // double. Unlike std::binomial_distribution -- the one distribution still called, see 0019 --
    // it uses no libm, so it can be frozen exactly rather than approximately. Verified against
    // libstdc++ under GCC 13.3 and 14.2 over four seeds and nineteen weight vectors, comparing the
    // chosen index AND the generator state after each call.
    if (count < 2) return 0;

    std::vector<double> cumulative;
    cumulative.reserve(count);
    double running = 0.0;
    for(const auto& weight : weights) {
        running += static_cast<double>(weight) / weight_total;
        cumulative.push_back(running);
    }
    cumulative.back() = 1.0; // libstdc++ pins the last one, so a rounded sum below 1 cannot escape

    const double pick = detail::canonical();
    return static_cast<size_t>(std::lower_bound(cumulative.begin(), cumulative.end(), pick) - cumulative.begin());
}

} // namespace rng
#endif // _RNG_HPP
