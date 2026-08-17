# 0016 — A town owns its markets; the region owns recruiting

**Status:** proposed, 2026-08-17. Settles how a replaced town's markets are disposed of, and
**amends [0015](0015-one-production-per-item-and-skill.md)**, which claimed the market list could not
reach the shape this record is about.

## Context

`ARegion::MakeStartingCity` replaces a region's town:

```cpp
if (town) delete town;
add_town(TOWN_CITY);
if (!Globals->START_CITIES_EXIST) return;
…
for (auto& m : markets) delete m;
markets.clear();
```

`add_town` calls `SetupCityMarket`, which **appends** to `markets` rather than replacing what is
there. The clearing pass sits below the `START_CITIES_EXIST` early return, so five of the seven
rulesets never reach it. A region that already had a town therefore ends up carrying its old town's
market block *and* a second, complete one, with different amounts and different prices.

Only the first block is tradeable. `Game::DoBuy` and `Game::DoSell` walk every unit's orders at the
first market of that type and erase each one they satisfy, and `Game::ProcessBuyOrder` folds several
BUY lines for one item into a single order. The second block runs, finds no orders, and does
nothing — it only shows up in the report.

**Measured**, by generating a world with and without the fix and comparing `game.out`: `kingdoms`
64×64 and 72×72 each carried one affected region; the region held 28 markets where 14 is a full set,
with 5 `(type, item)` pairs repeated outright.

### Which rulesets this can reach

Two globals decide it, and only their combination matters:

| Ruleset | `NEXUS_EXISTS` | `START_CITIES_EXIST` | Reachable |
| --- | --- | --- | --- |
| `kingdoms`, `havilah`, `neworigins`, `rimefall` | 1 | 0 | **yes** |
| `standard`, `basic` | 1 | 1 | no — the clear below runs |
| `fracas` | 0 | 0 | no — see below |

`fracas` is the interesting one. Its `SetACNeighbors` call is guarded by `if
(Globals->NEXUS_EXISTS)`, and its other `MakeStartingCity` call site needs `NEXUS_IS_CITY`, which is
also 0. **`fracas` never builds a starting city at all**, which twelve generated worlds confirm: not
one differs before and after the fix.

Incidence tracks town density rather than anything structural. `kingdoms` sets `TOWN_PROBABILITY` to
200 against `havilah`'s 100 — 114 settlements against 80 on a 64×64 world — and seven generated
`havilah` worlds happened to place none of their start cities on an existing town. The code path is
identical; it is a question of odds, not of kind.

## Decision

**Markets created for a town are dropped when that town is replaced. Recruiting markets survive.**

`ARegion::remove_town_markets()` keeps every market for an `IT_MAN` item and deletes the rest, and
`MakeStartingCity` calls it between `delete town` and `add_town`.

### Why recruiting has to be excepted

The obvious fix — clear `markets` outright before `add_town` — was written, built and measured
first. **It removes the ability to recruit from every starting city in the game.** On a `kingdoms`
64×64 world all six start cities came out with no market for men and none for leaders.

The recruiting markets belong to the region, not to its town: `SetupEconomy` builds them once from
the local race, and the path through the `START_CITIES_EXIST` early return never rebuilds them. Only
the branch below that return does, which is why the bug and its naive fix are both invisible in
`standard` and `basic`.

Telling the two kinds apart needs no bookkeeping. `SetupCityMarket` never creates a market for a man
item, because races carry `IT_MAN` without `IT_NORMAL` — the same distinction `UpdateEditRegion`
already relies on when it replaces man selling.

## What this amends in 0015

0015 stated, under *Context*:

> The same shape does not occur in markets. `ARegion::SetupCityMarket` zeroes `demand[i]` and
> `supply[i]` as it consumes them […] so no item can reach the list twice with the same market type.

**The reasoning is correct and the conclusion is wrong.** `SetupCityMarket` cannot produce a
duplicate *within one block*, which is what was examined. It says nothing about the function being
called twice on the same region, which is what happens here.

The scan offered as evidence — `fracas`, `standard`, `neworigins` and `havilah` worlds — could not
have found it either: `fracas` builds no start cities, `standard` clears its markets, and the two
that can reach it are the two where it is rare. `kingdoms`, where it is common, was not among them.
**A clean scan over the wrong four rulesets reads exactly like a clean scan.**

0015's own decision is untouched: it concerns productions, and nothing here changes it.

## Consequences

- **Nothing changes for `standard` and `basic`.** The new call is redundant there, because the clear
  below removes what `add_town` just appended as well. Verified: a seeded 32×32 `standard` world
  generates byte-identical before and after.
- **Existing worlds are not repaired.** Unlike 0015's fix, this one only runs at world creation, and
  the duplicate block is indistinguishable from a legitimate one once it is in a save file — there
  is no marker saying which markets came from which town. A game already running keeps its dead
  block. Repairing it would mean guessing, and the block is inert.
- **Eight copies, because `MakeStartingCity` is per-ruleset.** The logic lives in one engine
  function; each ruleset gains one call and three lines of comment. The `unittest` ruleset carries
  the same copy, which is what makes the regression test possible.
- **The regression test flips `START_CITIES_EXIST` and puts it back.** `Globals` is one shared
  object for the whole test binary, and the `unittest` ruleset sets that flag to 1 — the path that
  does not have the bug.
- **Upstream-worthy, and not offered.** `MakeStartingCity`, `add_town` and `SetupCityMarket` are all
  upstream's, and the fix is not conditional on any ruleset. Per
  [0008](0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.
