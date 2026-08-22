# 0015 — One production per item and skill, in a region

**Status:** accepted, 2026-08-17. Settles where the fix for a duplicated region production belongs,
because two of the three candidate places would have changed what a region yields.

## Context

`ARegion::products` is a flat vector, and nothing ever stopped the same item appearing in it twice.
`Game::ModifyTerrainItems` lets a ruleset write any item into any slot of a terrain, and `fracas`
does exactly that in `fracas/extra.cpp`:

```cpp
ModifyTerrainItems(R_DESERT, 5, I_MITHRIL, 10, 5);
```

Slot 4 of the desert already holds `{I_MITHRIL, 20, 3}` from the shared table in `gamedata.cpp`.
`ARegion::SetupProds` rolls every slot separately, so at 20 % and 10 % a small percentage of deserts
draw mithril from both slots and end up producing it twice, with different amounts.

**Only the first entry can ever be harvested.** `Game::RunProduceOrders` does loop over every
production of the region, but `Game::RunAProduction` deletes the month order of every unit that
worked the production it was given; by the time the second entry for the same item is run there is
no unit left holding a matching order. `Game::RunUnitProduce` returns at the first `(item, skill)`
match, and `ARegion::get_production_for_skill` and `ARegion::produces_item` both `find_if` and stop
at the first hit. `ARegion::UpdateProducts` then restores both entries from `baseamount` every turn,
so the situation is permanent rather than a one-off.

What the player sees is a region report — JSON and text alike — listing *3 mithril* and *5 mithril*,
of which the 5 does not exist. Measured on a seeded 64×64 `fracas` world: 8 of the surface level's
157 deserts, and no other region of any level.

The same shape does not occur in markets. `ARegion::SetupCityMarket` zeroes `demand[i]` and
`supply[i]` as it consumes them, its four trade indices are drawn distinct, and no item carries both
`IT_NORMAL` and `IT_TRADE`, so no item can reach the list twice with the same market type. Checked
by generating `fracas`, `standard`, `neworigins` and `havilah` worlds and scanning every region.

## Decision

**A region holds at most one production per `(item, skill)` pair. The first one wins.**

`ARegion::remove_duplicate_products` drops any later entry repeating a pair, and is called from two
places: at the end of `SetupProds`, so a new world never contains one, and from `ARegion::Readin`,
so an existing world is repaired the first time it is loaded. The load path logs each repair.

`(item, skill)` rather than item alone, because silver legitimately appears twice — once with no
skill for `WORK` and once with `S_ENTERTAINMENT` for `ENTERTAIN`.

### Why the first entry and not the sum

Keeping the first and discarding the rest is the only one of the three obvious choices that changes
no yield anywhere. The first entry is precisely the one the region has been handing out all along,
so a running game notices nothing except that the phantom line disappears from its report. Adding
the amounts together would silently make every affected desert richer, and taking the larger would
do the same less predictably. Neither is a fix for the bug; both are balance changes wearing one.

### Why not teach production to fall through

The alternative was to let a unit whose first deposit ran dry see the next production of the same
item — `RunAProduction` or `RunUnitProduce` — which has the attraction of repairing existing worlds
without touching their save file. It was not taken:

- it changes what a region yields, in the one place where nobody asked for a change: an affected
  desert would go from 3 mithril a month to 8;
- it makes `get_production_for_skill` and `produces_item` wrong rather than merely first-match —
  every caller asking *"how much of this does the region have?"* would have to be found and taught
  to sum;
- and it keeps the report honest only by accident: the region would still print two lines for one
  resource, and the player would have to add them up.

The dead entry is the bug. Making it live is a bigger, riskier change than deleting it.

### Why not deduplicate the report only

That leaves the unreachable production sitting in the region, where every future reader of
`products` has to know it is a lie. The two accessors that exist today happen to be safe because
they stop at the first match; a third that sums, or an editor that lists, would not be. A report
filter hides the symptom from the player and from us, and the save file keeps the bug.

## Consequences

- **`fracas` deserts keep their extra chance at mithril.** The ruleset line stays as written. With
  two slots rolling, mithril now appears in about 28 % of deserts instead of 20 %, at amount 3 when
  the table's own slot hits and 5 when only the added one does. That is what the ruleset author
  appears to have been reaching for, and it is what the ruleset has effectively been doing since the
  line was added — the second entry was never the harvestable one.
- **No other ruleset is affected.** Every `ModifyTerrainItems` call in the tree was checked against
  the base table: `havilah`'s `R_JUNGLE` call rewrites the slot that already held `I_IRONWOOD` and
  so replaces it rather than repeating it, and the rest write into free or differently-itemed slots.
  `fracas`'s desert is the only duplicate.
- **Worlds already in play change on their next turn**, by exactly the removed lines. Verified: a
  world generated before the fix and then run through the fixed binary produces a `game.out`
  byte-identical to the same world generated with the fix.
- **The rest of the world does not move.** The duplicate is discarded *after* the slot has been
  rolled and the `Production` constructed, so the sequence of random draws is untouched and every
  other region of a seeded world is unchanged.
- **The snapshot suite could never have caught this.**
  [../interface/compatibility.md](../interface/compatibility.md) already names the two blind spots:
  world generation, and `fracas` beyond its rulebook. This bug lived in both at once.
- **Upstream-worthy, and not offered.** The engine, the shared terrain table and `fracas` are all
  upstream's; nothing here is fork-local. Per
  [0008](0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered, and offering
  it stays a separate decision.
