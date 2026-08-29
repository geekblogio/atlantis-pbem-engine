# 0020 — The engine owns its binomial draw too

**Status:** accepted, 2026-08-23. Completes [0019](0019-the-engine-owns-its-random-distributions.md),
whose "what this does not cover" section this record overturns.

## Context

0019 wrote out `uniform_int_distribution` and `shuffle`; `#66` added `discrete_distribution`. One
was left: **`std::binomial_distribution`, in `rng::calculate_losses`** — the monthly decay of
summoned undead, the only call site.

0019 gave two reasons for leaving it, and **the second was wrong**:

1. *It cannot be frozen the same way*, because libstdc++'s implementation calls `log`, `exp`,
   `sqrt` and `lgamma`, and libm is not guaranteed bit-identical between glibc and Apple's libm.
2. *A portable replacement would change Linux*, so it is a game change and needs its own decision.

Point 2 described **replacing** the distribution — n Bernoulli draws instead of one binomial draw,
which is the same rule stated differently but consumes different randomness. That would indeed
change running games. It was never the only option, and proposing it under the banner of "so that
games do not change" was backwards.

**Freezing is not replacing.** Copying libstdc++'s algorithm means our copy calls the *same* libm,
with the same arguments in the same order, on the machine the games run on. Linux is then unchanged
by construction, exactly as in `#61`, `#64` and `#66`.

Point 1 survives, but only as a limit on what can be *promised*, not as a reason to do nothing.

## Decision

**`rng::detail::binomial()` reproduces libstdc++'s `std::binomial_distribution`** — Devroye's
rejection method above `t * p == 8`, the waiting-time method below it — together with
`normal_source`, which reproduces libstdc++'s `std::normal_distribution`, since the rejection loop
draws from it.

The normal distribution's **cache is part of the sequence, not an optimisation**: Marsaglia's polar
method produces two values per pair and keeps the second for the next call. A copy that recomputed
both would consume twice the stream. `calculate_losses` builds a fresh distribution per call, so
the cache always starts empty, and `normal_source` does too.

### What was measured

| | result |
| --- | --- |
| against real `std::binomial_distribution`, **GCC 13.3** (what CI runs) | 5984 cases, **0 divergences** |
| the same under **GCC 14.2** | 5984 cases, **0 divergences** |
| macOS/arm64 against linux/amd64, dense sweep | **263 736 draws, identical hash** |

Each case compares the value **and** the generator state after it. The dense sweep covers
everything `calculate_losses` can ask for: every whole percentage from 1 to 99, item counts 1..200
and 250..5000 in fifties, three seeds, three repetitions each — both branches of the algorithm,
many times over.

**So the libm worry did not materialise.** It is not thereby disproved: `log` and `exp` remain
outside our control, and a future glibc or Apple libm could differ where today's do not. What
changed is that the risk is now measured and bounded instead of assumed, and that the fallback is
benign — a divergence would be a macOS replay differing from the server, which is what we had
before this change anyway.

## Consequences

- **Linux is unchanged, and that is provable rather than probable**: identical values and identical
  generator state against the real distribution, under both compilers.
- **A libstdc++ upgrade can no longer rewrite item decay in a running game.** It could before, and
  nobody would have noticed until a snapshot moved.
- **A glibc upgrade still could.** `detail::binomial()` is the only place in `rng.hpp` that calls
  libm, and it is commented as such. This is the residue 0019's point 1 was right about.
- The recorded turns do not exercise this path — no fixture has a player holding undead — so
  `unittest/rng_test.cpp` pins both branches instead.
