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

    // Use binomial distribution: 'amount' trials, 'probability' chance of success (loss) per trial
    std::binomial_distribution<> distribution(amount, probability);
    return distribution(detail::get_initialized_generator());
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
    for(const auto& weight : weights) weight_sum += weight;

    if (weight_sum == 0) return std::nullopt;

    std::discrete_distribution<size_t> distribution(std::ranges::begin(weights), std::ranges::end(weights));
    return distribution(detail::get_initialized_generator());
}

} // namespace rng
#endif // _RNG_HPP
