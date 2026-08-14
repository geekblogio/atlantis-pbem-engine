# Engine architecture

> **Audience:** anyone about to modify engine C++.
> **Provenance:** upstream-friendly — nothing here is specific to this fork.

**Read this when** you are about to change engine code and do not already know where the
change belongs.

## The engine / ruleset split

The engine library is every `*.cpp` in the repository root. It is game-agnostic: it declares
functions it never defines, and each **ruleset** directory supplies them. A ruleset compiles
together with the engine into its own standalone executable — `standard`, `basic`, `fracas`,
`havilah`, `kingdoms`, `neworigins`, and the fork-local `neworigins8` and `rimefall`.

Five files per ruleset, all mandatory:

| File | Supplies |
| --- | --- |
| `rules.cpp` | the `GameDefs g` struct assigned to the global `Globals`, plus the `allowed*` progression arrays |
| `extra.cpp` | `Game::SetupFaction`, `Game::CheckVictory`, `ARegion::movement_forbidden_by_ruleset`, `Game::ModifyTablesPerRuleset` |
| `world.cpp` | `Game::CreateWorld`, terrain and race distribution, weather, starting cities |
| `map.cpp` | the `ARegionList::create_*_level` functions and the land-shaping helpers |
| `monsters.cpp` | `Game::CreateVMons`, `Game::GrowVMons` |

See [rulesets.md](rulesets.md) for the contract in detail, including the traps.

## Game data lives in global tables

`gamedata.cpp` holds large global vectors — `ItemDefs`, `SkillDefs`, `ObjectDefs`,
`TerrainDefs`, `MonDefs`, `WeaponDefs`, `SpecialDefs` and more — indexed by the enums in
`gamedata.h` (`I_*`, `S_*`, `O_*`, `R_*`).

These tables are the **union of everything any variant ever needed**. A ruleset carves out its
own game by calling the `Enable*` / `Disable*` / `Modify*` helpers in `modify.cpp` from its
`ModifyTablesPerRuleset`.

Two consequences that catch people:

- **The enum and the table are positionally coupled.** Adding an item means appending to the
  enum in `gamedata.h` *and* inserting the matching entry at the same position in
  `gamedata.cpp`. Nothing checks this.
- **There are two parallel index spaces.** `I_*` indexes `ItemDefs`; `MONSTER_*` and `WEAPON_*`
  index `MonDefs` and `WeaponDefs`. The name of the helper tells you which table it edits —
  `DisableItem(I_BALROG)` and `modify_monster_attacks_and_hits(MONSTER_BALROG, …)` touch
  different rows about the same creature.

## The turn pipeline

`Game::RunGame` → `PreProcessTurn` → `ReadPlayers` → `ReadOrders` → `RunOrders` →
`WriteWorldEvents` → `WriteReport` → `WritePlayers`.

`Game::RunOrders` ([`runorders.cpp:5`](../runorders.cpp)) is the authoritative list of turn
phases, in order: FIND, ENTER/LEAVE, PROMOTE/EVICT, combat, STEAL/ASSASSINATE, GIVE, ENTER NEW,
EXCHANGE, DESTROY, PILLAGE, TAX, GUARD, magic, SELL, BUY, FORGET, mid-turn processing, QUIT,
WITHDRAW, SACRIFICE, movement, teach, month-long orders, economics, teleport, transport,
annihilation, maintenance, migration.

**The order between phases is game-visible behaviour.** Changing it moves snapshot output, and
usually changes who wins a race for a scarce resource.

Parsing is separate from execution. `parseorders.cpp` validates orders and queues them per
unit; each phase then walks all units. The same parser backs the `check` subcommand, which
syntax-checks an order file against a `DummyGame` and needs no `game.in`.

## Core objects

`Game` owns an `ARegionList regions`, a faction list, and `ppUnits`, a unit-number → `Unit*`
lookup. `ARegion` holds `Object`s, each holding `Unit`s; a `Unit` holds an `ItemList` and a
`SkillList`.

Iteration during mutation uses `safe::list` (`safe_list.h`), which tracks live iterators and
fixes them up on erase. Units and objects are routinely destroyed mid-phase, so use it rather
than `std::list` for anything a turn can delete.

## Randomness is a cross-cutting constraint

`rng.hpp` wraps all randomness. The seed is set once in `main.cpp` and reseeded in `NewGame`.
Never call `rand()` directly.

Any change to the **number** of draws in a phase shifts every subsequent draw and therefore all
downstream output — even when the change looks locally harmless. This is the most common cause
of a surprising snapshot failure; see [snapshot-tests.md](snapshot-tests.md).

## Events and reports

`events.cpp` and `events-*.cpp` accumulate `FactBase` records — battles, assassinations,
annihilations — during the turn. `WriteWorldEvents` turns them into the shared world-events
narrative.

Reports are generated as **JSON first**: `Faction::build_json_report` builds a
`nlohmann::json` document, and `text_report_generator.cpp` renders the human-readable report
from it. The JSON is therefore the source of truth, not a side product — a field missing from
the JSON cannot appear in the text report.

Which formats are written is controlled by the ruleset's `REPORT_FORMAT` flags:
`REPORT_FORMAT_TEXT` produces `report.<n>` plus `template.<n>`, `REPORT_FORMAT_JSON` produces
`report.<n>.json`. The report is a **published interface** — see
[interface/json-report.md](interface/json-report.md) before changing a field.

`genrules` renders the complete HTML rulebook from the live data tables (`genrules.cpp`,
`skillshows.cpp`). This is why changing a data table also moves the rules snapshots.

## Where do I add …?

| Change | Goes in |
| --- | --- |
| a new item, skill or object | `gamedata.h` enum **and** `gamedata.cpp` table, then enable it per ruleset |
| a tunable that varies per game | a new `GameDefs` field — **and every ruleset's `rules.cpp`** |
| a new order | `orders.h` enum, `parseorders.cpp`, and the executing phase in `runorders.cpp` |
| behaviour that only one game should have | that ruleset's `extra.cpp`, not the engine |
| a new engine source file | `CMakeLists.txt` **and** `Makefile` |
