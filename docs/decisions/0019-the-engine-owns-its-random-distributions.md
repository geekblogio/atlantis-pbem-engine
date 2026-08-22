# 0019 — The engine owns its random distributions

**Status:** accepted, 2026-08-20. Extends [0017](0017-x86-64-is-the-reference-for-draw-order.md),
which fixed one cause of the same symptom and did not reach the larger one.

## Context

0017 found that two RNG draws in one argument list are evaluated in an architecture-dependent
order, and pinned x86-64's. That was real, and it was not why a seeded world still came out
different on an arm64 Mac.

**`std::mt19937` is specified bit for bit. The distributions are not.** `std::uniform_int_distribution`,
`std::shuffle`, `std::binomial_distribution` and `std::discrete_distribution` are all
implementation-defined: each standard library chooses its own mapping from raw generator output to
values, and the choices differ in what they return *and* in how much of the stream they consume.

Measured on one seed, ten draws of range 6:

| | values | raw draws consumed |
| --- | --- | --- |
| libstdc++ (GCC, Linux) | `0 3 5 0 3 3 0 4 2 1` | **10** |
| libc++ (clang, macOS) | `1 4 5 4 1 2 3 0 0 1` | **15** |

The consumption is what makes this fatal rather than cosmetic. After the first distribution call
the two platforms are at different points in the same stream, and everything downstream diverges.
0017's defect swapped two draws; this one replaces the entire sequence.

`std::shuffle` inherits it twice over, being built on `uniform_int_distribution`.

This is not only a portability question. **A standard library upgrade is free to change these
mappings, and libstdc++ has changed the one for `uniform_int_distribution` before.** As long as the
engine calls the standard distributions, every recorded seed and every snapshot fixture depends on
a version of a library nobody in this project chose or tracks.

## Decision

**The engine implements the distributions it depends on, rather than calling the standard ones.
The numbers produced are the ones libstdc++ produces today, so no recorded seed and no running
game changes.**

Three are written out in `rng.hpp` (the third added by `#66`):

- `detail::uniform_below()` — Lemire's multiply-shift, which is what libstdc++ uses. Every other
  entry point (`get_random`, `make_roll`, `one_of`) funnels through it.
- `rng::shuffle()` — libstdc++'s shuffle, **including its pairing optimisation**: once two indices
  fit in one draw it generates them together and halves the draw count. That is observable
  behaviour, not an implementation detail; a plain Fisher-Yates returns different permutations for
  every size above two.

- `rng::get_weighted_index()` — libstdc++'s `discrete_distribution`, over a canonical double
  reproduced from `std::generate_canonical<double, 53, std::mt19937>`, **including its silent
  shortcut for a single weight**, which draws nothing at all. See the correction below.

`bound == 1` deliberately takes the same path as any other: it consumes one draw and yields 0,
because the standard distribution does, and short-circuiting it would shift every later draw.

### Verified, not asserted

Both were checked against the real libstdc++ in a `linux/amd64` container, comparing values **and**
generator state after each draw:

- `uniform_below`: every range from 1 to 4096, every power of two to 2^30 with its neighbours, and
  assorted large values.
- `shuffle`: sizes 2, 3, 4, 5, 10, 17, 50, 101, 1000.

Under **GCC 13.3** — what CI runs — and **GCC 14.2**, so the implementation is not pinned to one
compiler version.

Two candidate implementations were written and discarded on the evidence: the scaling-with-rejection
method described in older libstdc++ sources, which disagrees on 261 of the first 4096 ranges, and
Fisher-Yates without the pairing optimisation, which disagrees for every size above two.

### The result

| | macOS/arm64 | linux/amd64 |
| --- | --- | --- |
| world from a fixed seed | `26805c5c7fe3e2f21b34c7d604f0ffcb` | same checksum |
| turn snapshots, `standard` + `neworigins` | 28 of 28 identical | 28 of 28 identical |
| unit tests | (see below) | 20 suites, all pass |

Linux is unchanged, measured rather than assumed: a clean container build replays every recorded
turn identically and passes the whole unit suite.

## What this does not cover

**`std::binomial_distribution` remains a standard call**, in `rng::calculate_losses` — one site,
for item decay. It is left alone on purpose, and neither reason is convenience:

1. **It cannot be frozen the same way.** libstdc++'s binomial uses `std::log` and `std::exp`, and
   libm results are not guaranteed identical between glibc and Apple's libm. A faithful port could
   still diverge on a last-bit difference that flips a comparison, which would be worse than the
   honest gap: it would look fixed.
2. **A portable replacement would change Linux** — a different draw sequence at that site, and so
   different outcomes in running games. That is a game change, not a bug fix, and needs its own
   decision.

~~`std::discrete_distribution` (`rng::get_weighted_index`) was measured to agree between the two
libraries and is left alone.~~ **Wrong, and corrected by `#66`: the engine owns this one too.**

The caveat this paragraph carried — that the agreement was a fact about today's implementations
and not a promise — turned out to be too kind to the measurement. The two libraries agree from
**two** weights up and part company at **one**: libstdc++'s `param_type` clears the probabilities
below two entries and `operator()` then returns 0 *without touching the generator*, while libc++
draws anyway. The original measurement never saw it, because it was taken where the function does
not run.

It runs in exactly one place — `MakeManUnit()`, reached only when `LEADERS_EXIST` is false, which
is true of `kingdoms` alone — and half the picks during a `kingdoms` world creation offer a single
candidate. So `kingdoms` was building a different world per platform from the same seed the whole
time, unnoticed, because no fixture covered world creation. `#66` reproduces libstdc++'s algorithm
in `rng.hpp` and adds `snapshot-tests/run-worldgen-snapshot.sh` so that the path is watched.

Unlike the binomial above, this one **can** be frozen exactly: it is normalise, accumulate and
`lower_bound` over a canonical double, and nothing in it calls libm.

~~**The unit tests still fail on macOS/arm64** — ten suites — while passing on Linux.~~ Found and
fixed in `#64`: `std::minstd_rand`'s `result_type` is `uint_fast32_t`, which is 32 bits on arm64
and 64 bits on x86-64, so a *negative* seed reached the generator two different ways. Both
platforms now pass the whole suite.

## Consequences

- The snapshot fixtures stop being Linux artefacts. `docs/snapshot-tests.md` said that replaying
  them elsewhere tests the platform rather than the engine; that is no longer true, and it is
  corrected in the same pull request.
- A standard library upgrade can no longer silently rewrite the game's randomness.
- `rng.hpp` now carries the obligation the standard used to: if these functions are ever edited,
  every recorded seed and every fixture changes. The file says so at each site.
