# Rulesets

> **Audience:** anyone adding or changing a game variant.
> **Provenance:** upstream-friendly.

**Read this when** you are adding or modifying a ruleset, or you need a change not to leak into
the other five.

## The contract

A ruleset is a directory supplying five source files that the engine declares but never
defines. Every one is mandatory; omitting a definition is a link error, not a warning.

### `rules.cpp`

One `static GameDefs g = { … }` initializer assigned to the global `Globals`, plus the
`allowed*` progression arrays (`allowedMages`, `allowedTaxes`, `allowedTrades`,
`allowedQuartermasters`, `allowedTacticians`, `allowedMartial`, …) and their sizes.

**Every field is positional and identified only by a trailing comment.** Two consequences:

- Field **order matters**. Inserting a value in the wrong place silently shifts every field
  after it, and the game runs with plausible-looking nonsense.
- Adding a `GameDefs` member means **editing every ruleset's `rules.cpp`**, including
  `unittest/rules.cpp`. A ruleset that is not updated compiles — it just reads the wrong values
  from that point on.

Enum-valued fields are bit sets combined with `|`, not `||`:

```cpp
GameDefs::ALLOW_TRANSPORT | GameDefs::QM_AFFECT_COST,  // TRANSPORT
```

### `extra.cpp`

- `Game::SetupFaction` — what a new faction starts with.
- `Game::CheckVictory` — the win condition; return the winning faction or `nullptr`.
- `ARegion::movement_forbidden_by_ruleset` — return a reason string to block a move.
- `Game::ModifyTablesPerRuleset` — the only sanctioned place to shape the data tables.

### `world.cpp` and `map.cpp`

World generation: `Game::CreateWorld`, the `ARegionList::create_*_level` functions, terrain and
race distribution, weather, and starting-city selection. `map.cpp` holds the land-shaping
helpers, `world.cpp` the composition and the per-region setup.

### `monsters.cpp`

`Game::CreateVMons` and `Game::GrowVMons` — wandering monster creation and growth.

## Shaping the data tables

The global tables in `gamedata.cpp` are the union of everything any variant ever needed. A
ruleset selects its subset from `ModifyTablesPerRuleset` using the helpers in `modify.cpp`:

```cpp
DisableItem(I_BALROG);
EnableSkill(S_COOKING);
modify_monster_attacks_and_hits("BALR", 100, 100, 0, 0);
```

Watch which table a helper edits. `I_*` values index `ItemDefs`; monster and weapon helpers
take the abbreviation strings that index `MonDefs` and `WeaponDefs`. Enabling an item does not
enable the skill that produces it, or the object that houses it — each is a separate call.

## Registering a new ruleset

1. Create `<game>/` with the five source files, `<game>_intro.html`, and `html/<game>.css`.
2. Add the name to the `GAMES` list in `CMakeLists.txt`.
3. Add the target and the `<game>-clean` / `<game>-rules` targets in `Makefile`, and to the
   `all` and `all-clean` lists.
4. Add `<game>/obj`, `<game>/<game>` and `<game>/html/<game>.html` to `.gitignore`, and
   `/snapshot-tests/<game>` for the binary the snapshot runner copies in.
5. Record the rules baseline: build it, then `./update-rules-snapshot.sh <game>`, and add the
   game to **both** `run-snapshots.sh` and `update-all-rule-snapshots.sh`. Missing the second
   one means the next re-record silently skips your ruleset.

A ruleset that is a small variation of an existing one does not need a full copy. Only the file
that actually differs has to be duplicated; the rest can be one-line `#include "../<base>/<file>"`
shims, which then track the base ruleset automatically. Quoted includes resolve relative to the
file containing the directive, so a shim behaves exactly like the original.

## `neworigins8`, and why it shares a name

`neworigins8/` is the shim pattern in practice, and the only fork-local ruleset here. It exists
because the live game runs NewOrigins 8, whose sole published rule change against 7 is that
**meals cost 50 silver instead of 30**. `rules.cpp` is a copy of `neworigins/rules.cpp` with
that one value changed; the other four files are shims.

**`RULESET_NAME` and `RULESET_VERSION` are deliberately identical to `neworigins`.**
`Game::OpenGame` refuses a `game.in` whose stored name does not match `Globals->RULESET_NAME`,
and a *higher* version triggers the `upgrade_*_version` path. Renaming or bumping either would
mean the binary could not open the very game files it exists to process. The two executables
are therefore interchangeable on the same game — verified by replaying a recorded `neworigins`
turn under both.

The consequence to keep in mind: nothing in a report or a game file distinguishes the two. The
rulebook is the only visible difference, and the generated `neworigins8` baseline differs from
the `neworigins` one in exactly four lines — the stylesheet reference and the meal price.

## The unit test ruleset

`unittest/` is a ruleset too, and it is deliberately minimal: a 2×4 surface plus a small
underworld, built by `unittest/world.cpp`. It exists so unit tests run against a world small
enough to reason about, and so a change to `standard` or `neworigins` does not silently
invalidate hundreds of assertions.

It counts as a ruleset for the `GameDefs` rule above: a new field must be added there as well.

## Ruleset-legal versus engine-level

A behaviour belongs in a ruleset if another variant could reasonably want it different, and in
the engine if it is a property of Atlantis itself. When in doubt, look at whether the change
would need a `GameDefs` field to express — if it would, it is ruleset-level, and putting it in
the engine will eventually force someone to add that field anyway.
