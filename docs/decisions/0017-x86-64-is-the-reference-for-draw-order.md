# 0017 — x86-64 is the reference architecture for RNG draw order

**Status:** proposed, 2026-08-18. Fixes a defect whose fix has two equally correct forms, where the
choice between them is not a technical one.

## Context

```cpp
reg = pArr->GetRegion(rng::get_random(pArr->x), rng::get_random(pArr->y));
```

Two calls that each advance the shared generator, as two arguments of one call. **C++ leaves the
order in which function arguments are evaluated unspecified**, and GCC resolves it differently per
architecture: right to left on x86-64, left to right on aarch64. `x` and `y` are therefore drawn in
opposite orders on the two platforms, and since `rng::get_random()` re-parametrises its distribution
per range, the two draws differ in value and not merely in which variable receives which.

The same seed builds a different world on each architecture. Measured on a seeded `fracas` world,
same source, same `debian:trixie-slim`, both platforms built the same day:

| seed, size | aarch64 | x86-64 |
| --- | --- | --- |
| 1791250632, 64×64 | Bulverde (48,38), village | Tannersville (5,25), village |
| 895341848, 72×72 | Benoit (4,26), village | Alpine (25,57), **city** |
| 314730366, 80×80 | Fairway (7,41), village | Arab (28,36), village |

**Only `fracas` exposes it.** Every other ruleset sets `NEXUS_EXISTS` to 1 and places a new faction
at `regions.front()` without drawing. `fracas` is the only one whose start hex comes out of the
generator, which is why a thousand agreeing NewOrigins seeds would have proved nothing.

Twelve sites carry the defect, in `SetupFaction` for all seven rulesets, in `CheckVictory` for
`neworigins` and `rimefall`, in `RunAnnihilateOrders`, and in `ARegion::Pillage`. Seven of them are
unreachable with the globals as shipped — `MULTI_HEX_NEXUS` is 0 everywhere, and the six rulesets
with a Nexus never enter the drawing branch — but they are the same defect and were fixed too.

## Decision

**The draws are sequenced into locals, and the order pinned is the one x86-64 already produced: the
rightmost draw first.**

Both orders are equally defensible as C++. Neither is more correct, because the language never
promised either. What settles it is which platform the engine actually runs on: **the servers are
x86-64, and every seed anyone has recorded reproduces a world under x86-64's order.** Pinning that
order means:

- no world already generated becomes irreproducible from its seed;
- no game in progress changes, on the platform every game is in progress on;
- `aarch64` — a development machine, and no game — changes instead.

The reverse choice, which reads more naturally as code because `x` then `y` matches the argument
list, would have moved every x86-64 world and left the machine nobody plays on untouched.

Verified both ways: an x86-64 build with the fix reproduces the three worlds above **byte for byte**
against the same build without it, and an aarch64 build with the fix reproduces the x86-64 worlds
exactly, hashes included.

### The comment is part of the fix

`const int ry` before `const int rx` looks like a slip, and the next reader will want to tidy it.
Every one of the twelve sites therefore says in place why the order is inverted and that it must not
be swapped. A fix that reads as a mistake does not survive.

## Consequences

- **Nothing changes on x86-64.** The snapshot suite was run inside an x86-64 container: 14 recorded
  `standard` turns and 14 `neworigins` turns, all identical. **No fixture needs re-recording**, and
  none had to be: no recorded turn reaches a live site — none casts `ANNIHILATE`, none enters the
  end-game anomaly path, and `Faction: new` in the fixtures runs through the `NEXUS_EXISTS` branch
  that never draws.
- **`aarch64` results move**, and that is the point rather than a side effect. Anyone comparing a
  world generated on a Mac against one from the server will now get the same answer.
- **A CI job refuses the shape**, `scripts/check-rng-draw-order.py`, wired in as *RNG Draw Order*.
  It reports two draws of different ranges inside one argument list and is deliberately narrow:
  a braced initialiser list is sequenced since C++11 and `<<` since C++17, so neither is flagged,
  and two draws of the same range are accepted because swapping them cannot change a sum. It does
  not see two different-range draws combined outside a call; catching those would mean parsing C++
  rather than scanning it. Validated in both directions — twelve findings on the tree before the
  fix, none after.
- **`docs/interface/compatibility.md` lists world generation as what the snapshot suite cannot
  catch.** That is still true, but the reason has changed: it was not comparable across platforms
  before, and now it is. Recording a world-generation fixture has become possible. Not done here.
- **`spells.cpp`'s gate month was sequenced as well**, and changes nothing anywhere: all seven
  rulesets set `GATES_NOT_PERENNIAL` to 0, so the enclosing branch never runs, and even inside it
  `get_random(0)` returns without touching the generator. It is written out so that a ruleset which
  does set the value gets a defined order rather than the compiler's.
- **Upstream-worthy, and not offered.** Unspecified evaluation order is upstream's bug in upstream's
  code, and the fix is not conditional on a ruleset. Per
  [0008](0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered. The choice of
  x86-64 as the reference is ours, and upstream may reasonably want the other one.
