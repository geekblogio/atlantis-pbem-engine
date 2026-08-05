# Rulesets

> **Audience:** anyone adding or changing a game variant.
> **Provenance:** upstream-friendly.

**Read this when** you are adding or modifying a ruleset, or you need a change not to leak into
the others.

## The variants

Seven rulesets ship here, plus the minimal one under `unittest/`. They are not variations on a
theme: of the 138 `GameDefs` fields, **97 hold different values in at least two of them**. Only
41 are constant everywhere — `TOWNS_EXIST`, `RACES_EXIST`, `FACTION_LIMIT_TYPE` (always
`FACLIM_FACTION_TYPES`), `CONQUEST_GAME` (always 0) and the like.

| Directory | `RULESET_NAME` | `RULESET_VERSION` | Levels | Victory condition |
| --- | --- | --- | --- | --- |
| `standard/` | **Wyreth** | 2.0.0 | nexus + surface | the Black Keep |
| `basic/` | **Standard Atlantis** | 5.0.0 | + 1 underworld | none |
| `fracas/` | Fracas | 4.0.0 | surface, nexus unused | none |
| `havilah/` | Havilah | 1.0.0 | + 1 underworld | quests |
| `kingdoms/` | Kingdoms | 1.0.0 | + 1 underworld | none |
| `neworigins/` | NewOrigins | 3.0.0 | + 1 underworld | quests, annihilation |
| `neworigins8/` | NewOrigins | 3.0.0 | as `neworigins` | as `neworigins` |

The first trap is in the table: **the directory named `standard` is not standard Atlantis.** It
calls itself Wyreth, carries its own mythology, and holds the only real win condition in the
repository. Ordinary open-ended Atlantis is `basic`.

Everything said about `neworigins` below holds for `neworigins8` as well — the two differ in
exactly one value.

`UNDERDEEP_LEVELS` and `ABYSS_LEVEL` are 0 in every ruleset, so the levels column only reflects
`UNDERWORLD_LEVELS` plus the nexus. Every `CreateWorld` calls `create_nexus_level`
unconditionally — `fracas` sets `NEXUS_EXISTS` to 0 and keeps the level anyway, deliberately, so
that the engine code stays general. With the flag off there are no gates out of it and level 0
of the world is the surface.

### Victory

The sharpest divide. `basic`, `fracas` and `kingdoms` return `nullptr` from `CheckVictory` — the
game is open-ended and players set their own goals. `standard` looks for the object `O_BKEEP`:
once the Black Keep stands empty, the faction guarding that region wins. `havilah` and
`neworigins` run the quest system (`VISIT`, `SLAY`, `BUILD`, `HARVEST`, `DEMOLISH`).

`neworigins` goes furthest, switching end-games through `rulesetSpecificData["victory_type"]`:
**`annihilation`** is active — six altars around the world's centre, the ANNIHILATE spell, and no
new factions once all six are empowered — while **`city_vote`** sits commented out beside it.
That machinery is why `neworigins/extra.cpp` is by far the largest ruleset source here.

### Economy and upkeep

| | standard / basic / fracas / kingdoms | havilah | neworigins | neworigins8 |
| --- | --- | --- | --- | --- |
| `UPKEEP_FOOD_VALUE` | 10 | 10 | 30 | **50** |
| `REGIONS_ECONOMY` | 0 | 0 | 1 | 1 |
| `MORE_PROFITABLE_TRADE_GOODS` | 0 | 1 | 1 | 1 |
| `SPOILS_NO_TRADE`, `BUILD_NO_TRADE` | 0 | 0 | 1 | 1 |

That single meal price is the *entire* difference between `neworigins` and `neworigins8`; see
[below](#neworigins8-and-why-it-shares-a-name).

### Magic and faction points

`neworigins` starts and ends differently: `allowedMages` is `{1, 2, 3, 4, 5, 6}` against
`{0, 1, 2, 3, 5, 7}` everywhere else — a faction begins with a mage but tops out one lower. Its
tax and trade progressions are stretched too (`{0, 15, 30, 50, 75, 100}` against
`{0, 10, 24, 40, 60, 100}`), and it is the only ruleset with `SKILL_LIMIT_NONLEADERS` off, so
common men may learn any number of skills.

`havilah` is the only one with `MAGE_NONLEADERS`, letting ordinary men cast at all, and the only
one to rename apprentices — `APPRENTICE_NAME` is `"acolyte"`.

### World generation and movement

- **`neworigins`** offers three surface generators (*Original*, *Parametrical*, *Island Ring*);
  every other ruleset has one. It also runs with `ODD_TERRAIN` at 80 against 0–20 elsewhere,
  `TOWNS_NOT_ADJACENT` at 100, and is the only ruleset with **no weather** at all.
- **`standard`** is the only one whose `CreateWorld` never asks for map dimensions: they are
  pinned at 64×64 in the source.
- **`fracas`** is the only shipped ruleset with `NEXUS_EXISTS` off. It also allows unrestricted
  flight over water (`WFLIGHT_UNLIMITED` against `WFLIGHT_MUST_LAND` or `WFLIGHT_NONE`) and is
  alone in enabling `COASTAL_FISH`.
- **`kingdoms`** is the outlier overall: **no leaders** (`LEADERS_EXIST` is 0),
  `REQUIRED_EXPERIENCE` 50, `ADVANCED_FORTS`, `DYNAMIC_POPULATION`, `GROW_RACES` 2,
  `MAX_ASSASSIN_FREE_ATTACKS` 2, and `MAX_INACTIVE_TURNS` 90 instead of 10.
- **`basic`** is the plainest — no transport network, no `DEFAULT_WORK_ORDER`, `WFLIGHT_NONE`,
  none of which is unique to it — but it is **the only ruleset that writes no GM report**
  (`GM_REPORT` is 0).

### Starting cities

Only `standard` and `basic` have them. Where they exist the fit-out differs sharply: `standard`
posts 500 guards in plate with 4 mages and 4 tacticians, `basic` posts 120 guards with none of
the three.

### Reports

**Every ruleset writes both `report.<n>` and `report.<n>.json`.** `neworigins` carried
`REPORT_FORMAT_JSON` from upstream, `havilah` and then the remaining four were switched on here —
see [fork/patches.md](fork/patches.md). Which keys actually appear still varies with the ruleset,
because the document is built conditionally: a `standard` faction report has no `statistics`,
since `FACTION_STATISTICS` is off there. [interface/json-report.md](interface/json-report.md)
describes the shape.

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
