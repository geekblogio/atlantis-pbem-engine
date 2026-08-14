# 0012 — A ruleset hook for gateway destinations

**Status:** accepted, 2026-08-15. **Amends [0010](0010-climate-banded-single-continent-ruleset.md)
section 0**, which forbade changing any file outside `rimefall/`. 0010 stays accepted; this record
records the one exception and its boundary.

## Context

0010 section 9 settled how a `rimefall` game starts: the Nexus holds gateways keyed on **latitude
band**, a player picks a start by moving into one, and the per-band counts express the density
curve in section 7. That was believed to be pure ruleset work, because `ARegionList::SetACNeighbors`
— which builds the gateway objects — is defined in the ruleset's own `map.cpp`.

Implementing it showed the belief was wrong. **A gateway's destination does not survive the move.**
In `monthorders.cpp`:

```cpp
newreg = regions.GetRegion(obj->inner);
if (obj->type == O_GATEWAY) {
    auto start_locations = level->get_starting_region_candidates(newreg->type);
    // ... occupancy cascade ...
    newreg = candidates[index];
}
```

The engine reads the gateway's destination, takes only its **terrain**, rebuilds the candidate set
from the whole map, and picks from that. The specific region a ruleset chose is discarded. Two
gateways pointing at different regions of the same terrain are indistinguishable in effect.

Terrain cannot stand in for a band. Measured across the six worlds generated for `#39`: tundra
occurs in bands 0–1, desert in 3–4, swamp in 2–4, plain in 0–4, and **mountain in all five** — the
last deliberately, because adamantium comes from mountain alone. No terrain identifies a band.

**The ruleset cannot fix this from inside.** `get_starting_region_candidates` is declared in
`aregion.h` but **defined in `aregion.cpp`**, unlike `MakeLand`, `SetACNeighbors`, `GetRegType` and
`create_surface_level`, which are declared there and defined per ruleset. No ruleset defines it and
none can: a second definition is a duplicate symbol. The planning note that listed the
ruleset-owned world-generation chain simply did not include this function.

`ARegion::movement_forbidden_by_ruleset` runs immediately afterwards and can refuse the move, but
refusing is a veto, not a placement: the unit stays where it is and rolls again next turn.

## Decision

**Add one ruleset hook to the engine, and nothing else.**

A ruleset-supplied function is called from the gateway branch in `monthorders.cpp`, after the
candidate list is built and before the occupancy cascade runs, and may narrow that list:

```cpp
void Game::filter_gateway_destinations(Object *gateway, ARegion *nominal,
                                       std::vector<ARegion *>& candidates);
```

It is declared in an engine header and **defined in every ruleset's `extra.cpp`**, beside
`movement_forbidden_by_ruleset`. Every ruleset except `rimefall` defines it empty, so its candidate
list is untouched and its behaviour is bit-for-bit what it is today. `rimefall` narrows the list to
the band the gateway belongs to.

### Why this shape and not the others

Two alternatives were weighed and rejected.

**A `GameDefs` flag making the engine honour `obj->inner` literally.** 0010 section 9 explicitly
sanctions a `GameDefs` field for a *behavioural switch* of exactly this kind, so this was the
closest contender. Rejected on failure mode: `GameDefs` is a positionally-initialised struct, so a
ruleset that is not updated still compiles and silently reads a zero. A missing function definition
is a **link error**. When a change has to reach nine rulesets including `unittest/`, the one that
cannot be forgotten silently wins.

It also loses more than it saves. Honouring `inner` literally skips the engine's occupancy cascade
— empty towns, then towns with guardsmen, then shared towns, then empty hexes — which 0010 section
9 chose deliberately to inherit rather than reinvent (*"Congestion is already handled"*). The hook
keeps that cascade and only narrows what it chooses from.

**Reusing `movement_forbidden_by_ruleset` as the filter.** Tempting, because it already exists in
every ruleset and is already called a few lines later, making this a single-file engine change. It
fails on information: the predicate receives the unit and the source region, not the gateway, so it
cannot tell which band's gateway was used. Giving it that argument is a signature change reaching
all nine rulesets anyway, and it would conflate "may this unit enter here" with "which starts does
this gateway offer".

### What is not decided here

This record permits **one** engine hook for **one** purpose. It is not a general licence. 0010
section 0 otherwise stands: anything else `rimefall` needs is ruleset work, and a second engine
change needs its own record and the same argument made again from scratch.

## Consequences

- **`docs/fork/patches.md` gains a divergence that is genuinely engine-level**, unlike `#38`
  (registration files) and `#39` (none). This one can conflict on an upstream sync, and it is the
  first divergence this ruleset creates that will need re-applying.
- **Every ruleset gains a function it does not use**, including `unittest/`. That is nine files of
  near-empty boilerplate in shared sources. It is the price of the safe failure mode, and it is
  paid once.
- **Existing behaviour must be provably unchanged.** With every other ruleset filtering nothing,
  the candidate list, the cascade and therefore the `rng` draw sequence are identical. The recorded
  turn snapshots for `standard` and `neworigins` are the evidence and must stay byte-identical.
- **It is arguably upstream's bug, but that is a separate decision.** By the criterion in
  `docs/rulesets.md` — a behaviour belongs in a ruleset if another variant could reasonably want it
  different — start-selection policy is ruleset-legal, and the engine hard-coding *scatter by
  terrain* is the thing out of place. Per [0008](0008-prepare-upstream-fixes-do-not-submit.md) it
  is prepared and registered, **not offered**. Offering it is its own decision.
- **The hook also removes a latent inconsistency.** Today the cascade can pick a region that
  `movement_forbidden_by_ruleset` then refuses at the next call site, and the player's move simply
  fails. A ruleset that narrows the list up front avoids choosing a region it would reject.
- 0010 section 9's *residual unfairness* — Nexus units processed in creation order — is untouched
  and stays open. Nothing here makes it better or worse.
