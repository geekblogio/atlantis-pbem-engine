# 0018 — `kingdoms` keeps its lakeless coastline

**Status:** proposed, 2026-08-18. Records a finding **and the decision not to act on it**, so that
the next reader does not spend the afternoon rediscovering it and then "repairing" it.

## Context

`ARegionList::CleanUpWater` converts water that ends up enclosed by land. Six of the seven rulesets
roll for it:

```cpp
if (rng::get_random(100) < Globals->LAKES) {
    reg->type = R_LAKE;
} else reg->type = R_NUM;
```

**`kingdoms` has no such roll.** Its copy sets `reg->type = R_NUM` unconditionally, and enclosed
water always becomes land. This is the same shape as the missing guard in
[0016](0016-a-town-owns-its-markets-the-region-owns-recruiting.md): six ruleset copies agree and
one does not.

And here the value makes it bite. `kingdoms` sets `LAKES` to **20**, the highest in the tree —
twenty times `havilah`'s and four times `neworigins`'. Measured over four generated 64×64 worlds:

| Ruleset | `LAKES` | lakes per world |
| --- | --- | --- |
| `kingdoms` | 20 | **0, 0, 0, 0** |
| `havilah` | 1 | 11, 13, 13 |

`kingdoms` does have a second, much weaker path in `GrowTerrain`, which rolls `LAKES / 10 + 1`
percent — 3 % for `kingdoms` — on hexes not yet assigned a terrain. It produced nothing in four
worlds. So a ruleset that declares the highest lake density in the repository ships without lakes.

**Neither the code nor the value is ours.** `CleanUpWater` in `kingdoms/map.cpp` is byte-identical
to `upstream/master`, and `LAKES = 20` is upstream's value too.

## Decision

**Nothing changes. The finding is recorded and left alone.**

Three reasons, in order of weight:

1. **It is not a defect.** The two bugs fixed alongside it each left a dead thing behind that the
   report advertised to players — an unharvestable production, an untradeable market block. Here
   nothing is dead and nothing is misreported. A `kingdoms` world simply has a different coastline
   than a global suggests. That is a discrepancy in the data, not misbehaviour of the engine.
2. **Changing it is a game change, and those are the game master's.** Any fix rewrites every future
   `kingdoms` world. Per the standing rule that bug fixes must not alter games, this is out of
   scope for maintenance and belongs to whoever runs the games.
3. **The obvious repair would be wrong anyway.** `LAKES` is scaled differently by the two code
   paths: `CleanUpWater` reads it as a straight percentage, `GrowTerrain` as `LAKES / 10 + 1`.
   `kingdoms`' 20 was plausibly tuned against the path it actually has, at 3 %. Feeding the same 20
   to `CleanUpWater` would turn **a fifth** of all enclosed water into lakes, against `standard`'s
   1 % and `neworigins`' 5 % — taking `kingdoms` from the ruleset with the fewest lakes to the one
   with by far the most, on a number nobody validated for that use.

## Consequences

- **If lakes in `kingdoms` are ever wanted**, that is its own decision and its own record, and the
  version to write is the roll *plus* a `LAKES` value in the 1–5 range the other rulesets use — not
  the roll alone with 20 left standing.
- **CI cannot catch a change here.** `kingdoms` has rulebook snapshots but no turn fixtures, and
  world generation is outside the snapshot suite in any case (see
  [../interface/compatibility.md](../interface/compatibility.md)). Evidence for any future change
  has to come from generated worlds, the way the numbers above did.
- **Not registered in `docs/fork/patches.md` as divergence**, because none is created: this record
  documents upstream's behaviour and our decision to leave it.
- **Not offered upstream.** There is nothing to offer — no patch, and a preference about another
  project's world generation is not a bug report.
