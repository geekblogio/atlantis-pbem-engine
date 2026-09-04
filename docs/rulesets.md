# Rulesets

> **Audience:** anyone adding or changing a game variant.
> **Provenance:** upstream-friendly.

**Read this when** you are adding or modifying a ruleset, or you need a change not to leak into
the others.

## The variants

Eight rulesets ship here, plus the minimal one under `unittest/`. They are not variations on a
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
| `rimefall/` | Rimefall | 1.0.0 | nexus + surface | two horde sources, then an election |

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

- **`neworigins`** offers three surface generators (*Original*, *Parametrical*, *Island Ring*) and
  asks which to use; every other ruleset has one and asks nothing. It also runs with `ODD_TERRAIN`
  at 80 against 0–20 elsewhere and `TOWNS_NOT_ADJACENT` at 100. It is one of the rulesets with
  **no weather** at all — `neworigins8` and `rimefall` inherit that from it.
- **`rimefall`** is the only ruleset whose `GetRegType` is **monotonic in latitude**. Every other
  one folds latitude about the equator (`if (lat > 3) lat = 7 - lat`), producing a world that is
  cold at both edges and warm in the middle. Rimefall is cold at one edge and dry at the other,
  which is the whole point of its map, so it must not fold. It is also the only one that confines
  land to a shape — a taper, wide north and narrow south — rather than letting continents seed
  anywhere.
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

`neworigins8/` is the shim pattern in practice, and the first of this fork's two own rulesets. It exists
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

## `rimefall`

`rimefall/` is the second fork-local ruleset, and it is now complete. It builds its own continent
— one landmass tapering from a wide, frozen north to a narrow, dry south — starts its factions
across five latitude bands, has neighbours begin as allies, drives two invasion fronts, and is won
by taking both horde sources and then being elected.

What is still `neworigins` underneath: `monsters.cpp`, permanently, and the quest machinery in
`extra.cpp`. **The annihilation ending is gone** — it was inherited live rather than dormant, with
`victory_type` set and its altars, monoliths, entity cages and the ANNIHILATE skill all enabled,
which offered a second and undesigned way to win. `rimefall` stores nothing in
`rulesetSpecificData` at all now, because nothing put there survives a save.

Unlike `neworigins8`, its `RULESET_NAME` and `RULESET_VERSION` are **deliberately distinct**. Its
world is incompatible with every existing one, so `Game::OpenGame` refusing another variant's
`game.in` is the wanted behaviour rather than an obstacle.

`rules.cpp`, `world.cpp`, `map.cpp` and `extra.cpp` are its own. `rimefall_intro.html` is written:
it is the ONLY channel for ruleset-specific rulebook prose, because `genrules` generates everything
else from the data tables, so the bands, the gateways, the fronts, the election and above all the
nature of the starting alliances are documented there or nowhere.

Its recorded turns are in `snapshot-tests/rimefall_turns` — seven turns, covering world load, a
faction claiming a start location through a gateway, the seal that follows, and two factions
entering the *same* gateway in one month so that one of them is substituted onto another slot.
**They do not reach the fronts or the election**, which need a long game; those were verified
separately.

**It writes `rimefall.json` every turn**, and it is the only ruleset that writes anything of its
own. The file carries the front's row, the three threat terms *separately*, the wave actually
placed on each front, whether the dragons are awake, both sources' state and the election standing
— see [../interface/file-formats.md](../interface/file-formats.md). The engine log and the times
articles say the same things to a person; this says them to a program, which is what tuning several
simulated games needs. The ruleset writes it itself rather than adding to `report.<n>.json`,
because [0012](decisions/0012-a-ruleset-hook-for-gateway-destinations.md) permits one engine hook
for one purpose and says plainly that it is not a general licence — and because the engine's report
is read by other projects.

`rimefall.h` holds the shape constants — the band count and the taper — because `world.cpp` and
`map.cpp` both read them, and from stage 3 the bands also key the gateways and the start slots.
One header is what stops those from disagreeing about how many bands the world has.

**The taper and `OCEAN` are one setting, not two.** `MakeLand` grows land until the ocean count
falls below its target; rimefall confines land to the taper. If the taper cannot hold the land the
target demands, the loop can never finish. That is why `OCEAN` is 65 here against NewOrigins' 55 —
`SEA_LIMIT` scales it up a further 12% internally, and at 55 the demand exceeds what the taper can
supply. `MakeLand` checks the two against each other before it starts and throws with both numbers
rather than hanging, and a stall counter catches the same failure from the other direction.

**Every band carries mountains, deliberately, and the southern bands carry extra.** With no
underworld, mountain and desert are the only sources of mithril and rootstone — but *adamantium
comes from mountain alone*; desert does not yield it. The far south has the least land and so the
fewest terrain anchors to roll on, so bands 3 and 4 are weighted up to compensate. At the first
weighting tried, one generated world in five came out with a single mountain in the far south.

### Start slots, and the registry that is not stored

`rimefall` is the only ruleset with a **bounded number of starting locations**. Twenty on a 64×64
map, distributed by the density curve in `rimefall.h` — 3, 5, 8, 3, 1 from the frozen north to the
far south — so the middle is roughly twice as crowded per hex of land as either edge.

**The curve is a shape, not a count** ([0024](decisions/0024-the-start-count-scales-with-the-land.md)).
Its twenty slots were measured on a 64×64 world; the number of starts follows the land a world
actually holds, at one per `RIMEFALL_LAND_PER_START` (28) land hexes, and the curve decides how they
spread across the bands. It scales both ways — 88 land hexes on a 24×24 world gives three starts,
589 on a 64×64 gives twenty-one, 931 on a 64×96 gives thirty-three — so **the map size sets how
many factions a game can hold**. The southern bands lose their slots first when scaling down,
because the curve weights them least. The constant is a first number and expected to move once a
real game has produced balancing data.

Two more things follow from the same thin-world problem. **A band that cannot fill its quota hands
the slots to the nearest band that still has candidates**, so the world keeps the number of starts
its land earns — the far south rolls jungle, its only wood, at 4 in 64, and
`get_starting_region_candidates` requires wood outright, so that band occasionally has no candidate
at all. And **the slots are placed by a farthest-point sweep**: each goes to the candidate whose
nearest already-placed start is furthest away, with only the first drawn at random. The sampler it
replaces drew fifty candidates per slot, which approximates the same answer on a large pool and
keeps redrawing the same handful on a small one. The curve is
set against *measured* land rather than against 0010 table 7's assumption that the far north holds
the most: it is the widest band, but `MakeLand` never seeds the rows beside the pole, so it holds
less land than the middle.

**One gateway object is one start slot, and the gateway objects are the registry.** There is
nowhere else to keep it: `rulesetSpecificData` is not persisted, and `ARegion::Writeout` has no
field for "this hex is a start slot" — adding one would change the `game.in` format, a published
interface. Objects are persisted, so the registry is read back out of the Nexus every time it is
needed. A gateway's **band is derived from its destination's latitude**, never parsed out of its
name, so the names can be reworded freely.

Occupancy is derived the same way — a slot is taken while a player faction stands on it — which
means a slot returns to the pool when its holder dies or walks away, without anything having to
maintain it. `CheckVictory` renames a taken slot to `Sealed gateway to …` rather than removing the
object, because these carry `buildingseq` numbers and churning them would move object numbers
around in reports and in the JSON.

**A start slot carries a settlement**, per
[0023](decisions/0023-a-rimefall-start-carries-a-settlement.md), and it takes two steps because one
is not enough. `SetACNeighbors` draws from candidates that already have a town and falls back to
open ground only when a band runs out; `Game::CreateWorld` then founds a village on whatever is
still townless. `get_starting_region_candidates` weighs resources and ignores towns, and the
engine's three town-only match levels cannot repair that here — they are handed one hex. Preference
alone reaches 17 of 19 slots at 64×64 and 8 of 20 at 24×24, because a small world holds fewer
settlements than the density curve asks for starts.

The founding lives in `world.cpp` rather than next to the selection because `ARegion::add_town` is
private and `Game` is a friend of `ARegion` while `ARegionList` is not. That is the whole reason the
two halves are in different files.

**A gateway whose slot is gone substitutes another one rather than refusing the move**, per
[0022](decisions/0022-a-taken-gateway-substitutes-a-start-location.md). Two factions choosing the
same gateway in the same month is unavoidable, so the loser of that race is set down on the nearest
slot still available: same band and same terrain, then same band, then same terrain anywhere, then
anywhere. `Game::filter_gateway_destinations` emits the substitution as an event on the unit,
because the engine picks the hex afterwards and nothing else in the chain knows a substitute was
used. An overrun slot is substituted for the same way and is never itself a substitute.

`rimefall_slot_offerable` is the single predicate for *can a newcomer be put here* — free and not
overrun — and the substitution, the refusal in `movement_forbidden_by_ruleset` and the free-slot
count that closes registration all read it. They disagreed before: the count ignored the overrun
state, so a faction could be admitted into a world with nothing but lost ground left.

An **empty candidate list means the world is full**, and nothing narrower. It used to mean the
band was full, which is why the refusal it produced named this hex rather than the world.

Two traps are worth knowing before touching this:

- **`movement_forbidden_by_ruleset` is called on every move in the game**, not just on arrivals.
  `rimefall`'s implementation keys on the *source* region being the Nexus. Without that test, every
  start region would become permanently unenterable for the rest of the game — including for its
  own owner walking home.
- **Object names lose `(` and `)`.** `filter::legal_characters` strips them, and
  `filter::strip_number` cuts everything from the last `(` when a save file is read back, so a
  parenthesised suffix survives world creation and is then truncated on the next load. The gateway
  names use a comma.

### The two powers, and renaming without touching `gamedata.cpp`

Six existing monsters are reskinned through `ModifyItemName` and a direct write to `MonDefs`. **No
entry is added to `gamedata.cpp`** — that table is the union of every variant's needs and is shared
with upstream, so adding to it buys a permanent merge conflict for flavour a rename gives free.

| code | was | is |
| --- | --- | --- |
| `ICEW` | ice wurm | rimeworm |
| `IDRA` | ice dragon | hoarwyrm |
| `SKEL` | skeleton | frostbound |
| `UNDE` | undead | thawless |
| `LICH` | lich | winterwright |
| `DRAG` | dragon | saltdrake |

The north takes frost words and the east takes salt, because they have to read as two powers rather
than one monster pool with two doors — the same reason ice dragons are kept off the eastern front.

**Both tables have to be renamed, and it is easy to get half right.** `ModifyItemName` covers
`ItemDefs`, which is what a player sees in an item list. But a monster *unit* is named from
`MonDefs` — `Game::MakeLMon` passes `monster.name` straight to `MakeWMon` — so renaming only the
item leaves every creature in the world still called an ice wurm. There is no
`modify_monster_name()` among the `modify_monster_*` helpers and none is needed: `find_monster()`
is declared in `items.h` and returns a mutable reference, which is what those helpers use
internally.

One label stays out of reach. `MakeLMon` names a crypt's stack with the hard-coded literal
`"Undead"`, as it does for `"Demons"` and `"Evil Mages"`, so a crypt still reports a unit called
Undead whose contents are frostbound, thawless and winterwrights. Reaching it would mean a third
engine change, which [0012](decisions/0012-a-ruleset-hook-for-gateway-destinations.md) does not
allow without its own record, and it is a group label on ordinary lair furniture rather than
anything to do with the invasion fronts.

### The two invasion fronts

**Position and strength are separate mechanisms.** The northern front's spawn band creeps south as
a pure function of the turn number — nothing players do slows it — while a *threat score*
recomputed from scratch each turn decides whether it attacks and how hard. Killing a front's source
is the only thing that stops it, and it stops it completely.

The front creeps from **its own source's row**, not from row 0. `MakeLand` never seeds the rows
beside the pole, so a front starting at zero spends twenty-odd turns crossing empty water doing
nothing — which looks like a working grace period and is really just geography.

The threat score is three terms, and the weights exist to stop any one of them deciding it alone.
The first attempt had prosperity at 747 against time at 90 over 45 turns, an eight-to-one split
that made the other two decoration; prosperity is now counted per ten thousand people rather than
per thousand, and at turn 45 the two sit at 104 and 90. **There is deliberately no separate grace
period** — the time term starts near zero and that is the grace, with the threshold first crossed
around turn twelve.

Discord counts only **player-versus-player** battles, identified by exclusion: a battle referenced
by neither the monster faction nor the guard faction was necessarily between players. The filter
under-counts — `Faction::battles` collects mere bystanders, so a player fight a wandering monster
stood next to is dropped — and that direction is the point. It cannot drive a feedback loop where a
large wave produces battles and therefore a larger wave.

`CheckVictory` writes one line per turn to the engine log giving the score and its three terms
separately. A game master tuning a live game cannot otherwise tell which term is driving the front.

**The sources have to be enterable, and this is easy to get wrong.** A front is defeated when a
player unit holds its source object. `O_ICECAVE` was the obvious choice for the north and is a
trap: it carries no `CANENTER` flag, so a player unit can never set foot in one — the report says
*"closed to player units"* — and the game would have been unwinnable. The north uses `O_ATEMPLE`
instead, whose garrison is the northern front's own creatures, and the east `O_DCLIFFS`, which is
enterable already.

A source that stands empty is re-garrisoned, **unless a player is in its region**. Entry to an
occupied object is refused while its monsters hold it, so taking a source means clearing it one
turn and entering the next; refilling on the turn it was cleared closes that window and makes both
sources permanently untakeable. Refilling once the attacker has *gone* is still right, and is what
makes a front something to be taken and kept rather than raided.

Bands the front has overrun stop offering start slots. That state is carried in the gateway's
**name** — `Lost gateway to …` — because `ARegion::movement_forbidden_by_ruleset` receives no
`Game` and so cannot work out where the front is; the name is persisted and `CheckVictory`
refreshes it. It also means registration closes itself as the world falls.

### Starting alliances, and why they read the event rather than the state

Neighbours begin at `ALLY`, written on both sides, to the holders of every other start slot within
`RIMEFALL_ALLY_RADIUS` — by **proximity, not band**, because the middle band holds by far the most
slots and a band-wide alliance would turn the most contested zone into one bloc.

**`ALLY` is deliberately the heavy attitude.** It permits `GIVE UNIT`, it defeats theft and
assassination attempts against a partner while an ally is watching, and it bypasses guard
restrictions. That weight is what makes the alliance worth something and therefore worth betraying,
and because attitudes are one-directional a faction can drop to neutral while its partner still
extends `ALLY` — a silent, asymmetric betrayal rather than a declared one. **It is a starting
default and not a pact**, and the rulebook has to say so or a player who assumes otherwise learns
expensively.

The alliance is applied **once, on arrival**, and the mechanism is the point. Nothing can be
stored to remember it has been done, so the obvious approach is to test whether a faction *looks*
unallied — and that cannot tell a newcomer from someone who has renounced every alliance. A faction
that quit its bloc would be dragged back into it next turn, which would make the alliance
compulsory and contradict the whole design.

So it reads the **event** instead: a gateway still named `Gateway to …` whose land is now held can
only mean its holder arrived since the last turn, and the rename to `Sealed gateway to …` in the
same pass makes it unrepeatable. The seal is persisted in `game.out`, so the memory survives a save
cycle without any new state. A renunciation is therefore permanent, which is verified by test.

An existing attitude is never overwritten — a faction that has already declared something about a
neighbour has said what it means. A faction that arrives with nobody inside the radius is told so
rather than being congratulated on neighbours it does not have.

This is also the ruleset where a game master meets a **refused faction**: once every slot is held,
`SetupFaction` returns 0, the newcomer is skipped and the turn runs normally for everyone else. See
[GAMEMASTER.md](../GAMEMASTER.md) section 3.3, and
[decisions/0014](decisions/0014-a-refused-faction-does-not-abort-the-turn.md) for why that is not
an abort.

Two fields carry `MUST stay 0` comments because flipping either leaves a ruleset that still
builds, still runs, and is quietly no longer the game:

- **`OPEN_ENDED`** — `runorders.cpp` calls `Game::CheckVictory` only when it is 0, and that is the
  only per-turn ruleset hook. The gateway rebuild, both invasion fronts and the election all hang
  off it.
- **`START_CITIES_EXIST`** — 0 selects the curated gateway path in `ARegionList::SetACNeighbors`,
  which is what the latitude-band start selection is built on.

The design is settled in writing before the code:
[0010](decisions/0010-climate-banded-single-continent-ruleset.md) for what the ruleset is, and
[0011](decisions/0011-rimefall-invasion-triggers-and-victory.md) for the invasion triggers, the
front's clock and the victory condition. Read both before touching `rimefall/`; the governing
constraint is that **nothing outside that directory changes**, and the registration touchpoints in
this ruleset's first commit are the one stated exception.

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
