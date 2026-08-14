# 0010 — A climate-banded single-continent ruleset

**Status:** accepted, 2026-08-14. Working name `rimefall`.

## Context

A new game variant is wanted: one large landmass, wide in the north and narrowing to the south,
with terrain running from tundra at the top to desert at the bottom, and recurring events in
which hordes of monsters invade — ice-bound undead pressing down from the north, dragons striking
inland from the eastern sea against the southern half. Factions start spread across the whole
continent, thinly in the north and densely toward the south, and neighbours begin on cooperative
terms rather than as strangers.

Nothing about that requires new engine concepts, but it does require decisions that are hard to
reverse once world generation has produced a live game. This record settles them before any code
exists.

## Decision

### 0. No engine changes

**Nothing outside `rimefall/` is modified.** Every decision below is satisfied with ruleset code,
and where a design would have been cleaner as an engine change, the ruleset-local form is chosen
instead and the cost is recorded.

This is affordable because the ruleset contract is wider than it first appears: `aregion.h`
*declares* the world-generation chain — `MakeLand`, `SetupAnchors`, `GrowTerrain`, `AssignTypes`,
`create_surface_level` — but every one of them is **defined in the ruleset's own `map.cpp`**. Land
shape and terrain assignment are already ruleset property; nothing needs to be opened up to reach
them.

Should implementation reveal something genuinely unsolvable this way, it is not written and not
worked around silently: work stops and the question is put explicitly before any engine file is
touched.

### 1. A new ruleset directory, shimmed onto `neworigins`

`rimefall/` supplies its own `rules.cpp`, `map.cpp`, `extra.cpp` and `monsters.cpp`; `world.cpp`
starts as a copy because the generator call site has to change. Anything that does not actually
differ stays a one-line `#include` shim, per the pattern established in [0006](0006-neworigins8-as-its-own-ruleset.md).

`RULESET_NAME` and `RULESET_VERSION` are **new and distinct** — unlike `neworigins8`, this
variant must not be able to open another variant's `game.in`, and its world is incompatible with
every existing one.

### 2. The classic generator, not the natural one

`rimefall` builds its surface with `create_surface_level` — the `MakeLand` / `SetupAnchors` /
`GrowTerrain` / `AssignTypes` chain — because that chain is ruleset code and can be reshaped
freely.

The natural generator was the first choice and is rejected. `Map::Generate` already derives
temperature from latitude and matches biomes on temperature and rainfall (`mapgen.cpp`), which is
most of the climate model for free — but its latitude mapping is **symmetric about the equator**
(cold at both edges, hot in the middle) and this ruleset needs cold at one edge and hot at the
other. `Map::Generate` is called from inside `ARegionList::create_natural_surface_level` in
`aregion.cpp`, whose helpers are not reachable from a ruleset, so there is no ruleset-side seam
between generation and terrain assignment. Using it would mean an engine change, and section 0
forbids that.

**What this costs.** The natural generator's rainfall simulation, rivers, water bodies and
historical buildings are given up; the world will look like classic Atlantis rather than like
`neworigins`. That is a real loss of texture and it is accepted deliberately: it buys total control
over land shape and terrain bands inside `rimefall/map.cpp`, which is what sections 3 and 7 both
depend on.

### 3. The landmass and the climate bands are both shaped in `MakeLand` and `AssignTypes`

`MakeLand` is ruleset code, so the trapezoid is expressed there directly: land is grown only within
a taper that is wide in the north and narrow in the south, with the existing noise still ragging
the coastline so the outline is irregular rather than geometric.

`AssignTypes` and `GrowTerrain` are likewise ruleset code, so the north-to-south progression is a
latitude-keyed terrain weighting rather than a physical simulation — tundra and forest at the top
grading through plains to desert at the bottom. Simpler than the biome model it replaces, and
directly editable, which matters because section 7 says the band layout will need revision after
the first game.

### 4. Monsters are reskinned, not added

`gamedata.cpp` already carries `ICEW`, `IDRA`, `DRAG`, `SKEL`, `UNDE` and `LICH`, and the ice
entries already declare `R_TUNDRA` habitats. `ModifyTablesPerRuleset` renames and retunes them
via `ModifyItemName` and the `modify_monster_*` helpers.

**No entry is added to `gamedata.cpp`.** That table is the union of every variant's needs and is
shared with upstream; adding to it buys a permanent merge conflict in exchange for flavour that
a rename delivers for free.

### 5. Two invasion fronts, both driven from `CheckVictory`, with no new `GameDefs` fields

`CheckVictory` is called once per turn from `PostProcessTurn` (`runorders.cpp`) and is already
the de-facto per-turn ruleset hook — in `neworigins` it spawns quests and writes Times articles.
Both invasion schedules live there. Spawned units join `monfaction`;
`MONSTER_ADVANCE_MIN_PERCENT` and `MONSTER_ADVANCE_HOSTILE_PERCENT` make them march instead of
standing still, and `EVENT_MONSTER_AGGRESSION` already exists as a report category.

The two fronts differ in geometry and in creature set:

| | northern front | eastern front |
| --- | --- | --- |
| spawn band | northern edge, across the full width | coastal and near-coastal regions east of the landmass |
| latitude reach | the whole map, pushing south | southern half only — mid-continent down to the desert |
| creatures | `ICEW`, `IDRA`, `SKEL`, `UNDE`, `LICH`, reskinned | `DRAG`, reskinned — **no ice dragons** |
| character | a slow, broad wall of attrition | few, fast, deep strikes |

Keeping ice dragons exclusively on the northern front and plain dragons exclusively on the
eastern one is what makes the two threats read as different powers rather than one monster pool
with two spawn points.

**The surface wraps.** `ARegionArray::GetRegion` reduces both coordinates modulo the array size
(`aregion.cpp`), so there is no eastern edge. "The east" is well defined *relative to the
continent* — the ocean on its eastern flank — but that same ocean is the western one. The eastern
spawn band must therefore be derived from the landmass itself, not from `x` near the array width,
or the dragons will appear off the west coast.

Every tunable — cadence, escalation curve, stack sizes, spawn band — is a **constant in
`rimefall/extra.cpp`**, not a `GameDefs` member. A new `GameDefs` field would force an edit to
every ruleset's `rules.cpp` including `unittest/rules.cpp`, and a ruleset that is missed still
compiles and silently reads the wrong values from that point on. The cost is not worth paying for
values only one variant will ever read.

### 6. Names are coined, and the map is not traced

Genre furniture — ice-bound undead, dragons, long winters, a cold north — is not protectable and
is used freely. Proper nouns from any existing work are not used anywhere: not in the ruleset
name, the rulebook, region or monster names, or the intro page. The continent outline is a
generic taper, not a recognisable silhouette. Rulebook prose is written from scratch.

### 7. Five latitude bands, with start density peaking in the middle

Starting locations are spread across the full north-south extent, but **density is the balancing
axis, not land quality**. The middle is the crush zone; the edges are roomy and dangerous.

| band | land | starting locations | effect |
| --- | --- | --- | --- |
| far north | most — the continent is widest here | a fair number | room to grow, poor ground, the northern front |
| near north | ample | many | the frontier: still open, already contested |
| middle | moderate | **by far the most** | the productive ground, and immediately crowded |
| near south | moderate | few | breathing room on good land, within dragon reach |
| far south | least — the taper bites | fewest | the most land per faction anywhere, and the driest |

The pattern that falls out of this is deliberate: **the edges are pressured by monsters, the
middle by other players.** A faction in the far north or far south has space and a front; a
faction in the middle has neither monsters nor room. That gives all three zones a reason to be
chosen and a reason to be feared, without any of them being simply better.

Subdividing the middle matters because a single wide "middle" band would average away exactly the
gradient the map exists to produce. Five bands is the coarsest split that still distinguishes
"frontier" from "crush zone" from "dry frontier".

**Alternatives rejected.** Placing everyone in the habitable south and expanding north turns the
gradient into a difficulty axis and is easier to balance, but discards the texture that is the
point of the map. Distributing factions so each spans comparable terrain bands is fairest and
makes the map decorative.

**The risk, stated plainly:** the compensation — space and quiet in exchange for poverty and
monsters — is a balance claim that cannot be verified except by playing. Band counts are the
parameter most likely to need revision after the first game, and they must therefore be trivially
editable constants, not structure.

### 8. Factions in the same band start mutually ALLY

Factions clustered around the same starting area begin at `AttitudeType::ALLY` toward one another,
written on both sides, so that regional blocs exist from turn one and players leave them rather
than build them.

`SetupFaction` already sets an attitude programmatically — `pFac->set_attitude(monfaction,
AttitudeType::UNFRIENDLY)` — so the mechanism is in place. Two properties of the attitude system
shape the implementation:

- **Attitudes are one-directional.** `set_attitude` records A's view of B and nothing more;
  `neworigins` already checks both directions explicitly for its win condition
  (`neworigins/extra.cpp`). Each starting relationship is written on both sides.
- **Factions arrive over time.** A `Faction: new` line is processed on whatever turn it appears,
  so a newcomer must be related to the factions already present *and* they to it, in the same pass.

**ALLY rather than FRIENDLY, chosen deliberately with the downside as the point.** ALLY is
mechanically heavy: `GIVE UNIT` requires it (`runorders.cpp`), an ally present in a region defeats
theft and assassination attempts against its partner (`runorders.cpp`), and it bypasses guard
restrictions (`aregion.cpp`). That weight is what makes the alliance worth something and therefore
worth betraying — and because attitudes are one-directional, a faction can drop to NEUTRAL while
its partner still extends ALLY, which is a *silent, asymmetric* betrayal rather than a declared
one. FRIENDLY would be the safe default and would make the alliance a formality.

This interacts directly with band density: pressure to break faith rises exactly where space is
scarcest, so the crowded middle is where blocs are least likely to hold. **The betrayal mechanic
and the density gradient are one design, not two.**

Players change any of this with `DECLARE` at any time. **This is a starting default, not a pact.**
The rulebook must say so plainly, because a faction that assumes the alliance is enforced will
learn otherwise expensively.

### 9. Start selection stays inside the engine, on occupancy-aware gateways

**A `rimefall` game must be playable with the binary alone.** The engine is a self-contained batch
turn processor; the two Python projects drive it but have never been required for a game to be
correct. A ruleset that only works when external tooling pre-assigns starting coordinates would
change what this program *is*, and that is a larger cost than the code it saves. Everything below
follows from that.

`neworigins` already implements gateway-based start selection, and `rimefall` inherits it rather
than inventing anything:

- The Nexus holds one `O_GATEWAY` object per destination class, named "Gateway to …"
  (`neworigins/map.cpp`); a player picks a start by moving into one.
- Destinations are **curated**: `get_starting_region_candidates` (`aregion.cpp`) requires wood,
  iron, stone, grain or livestock, and horses or camels within two hexes, on a landmass of at
  least ten hexes.
- Destinations are **spaced**: fifty attempts maximising the minimum planar distance to already
  chosen destinations, stopping once it exceeds twenty (`neworigins/map.cpp`).
- Congestion is **already handled**: on stepping through, the engine walks a preference cascade —
  empty towns, towns with only guardsmen, towns shared with other players, empty hexes, anything
  matching — and picks randomly within the first non-empty level (`monthorders.cpp`).

`rimefall` re-keys the gateways from terrain type to **latitude band**, and allows several
gateways per band so the counts in section 7 can be expressed.

#### Gateways reflect what is still free

A gateway into a band with no free slot **is not offered**. The list is rebuilt every turn from
`CheckVictory`, which runs inside `RunOrders` before `WriteReport` (`game.cpp`), so what a player
reads in their report is the true set of open bands at that moment.

Occupancy is **derived, not stored**: a slot counts as taken while a player faction holds it, using
the same live test the existing cascade already applies. That keeps the `game.in` format — a
published interface — untouched, and it means a slot returns to the pool if its holder dies or
abandons it, which is the right behaviour in a long game.

`ARegion::movement_forbidden_by_ruleset` is the backstop. The engine already calls it on the
resolved destination (`monthorders.cpp`), so a move into a slot that filled up in the same turn is
rejected in ruleset code, with a reason string the player can read.

#### When every slot is taken

The faction is **refused at creation** rather than stranded in a gateless Nexus. `SetupFaction`
returning 0 makes `AddFaction` discard the faction and log it (`game.cpp`), and `neworigins`
already uses exactly that path to close registration once its end-game condition is met
(`neworigins/extra.cpp`).

This is what makes the external signup cap **optional politeness instead of a dependency**. Capping
registrations avoids turning players away; not capping them is handled correctly and visibly by the
engine on its own.

#### The residual unfairness, and why it waits

Movement iterates `for (const auto u : o->units)` over each object's unit list (`monthorders.cpp`);
in the Nexus that is creation order, which is effectively faction number. With per-band slots and
the existing random cascade, this now only decides **which** free slot in a band a claimant gets —
and it only matters at all when a band has fewer free slots than same-turn claimants.

That residue is left alone for now. If play shows it matters, the fix is to randomise the order in
which Nexus units are processed, behind a `GameDefs` flag. **That would be a legitimate `GameDefs`
field, unlike the horde constants in section 5:** the criterion in `docs/rulesets.md` is whether
another variant could reasonably want the behaviour different, and `neworigins` has the identical
bias today. Section 5 refuses `GameDefs` fields for *tuning values only one ruleset reads* — not
for behavioural switches.

#### What this costs

More engine-adjacent work than pushing the draw outside would have: a per-band slot registry, a
per-turn gateway rebuild, a `movement_forbidden_by_ruleset` implementation, and a refusal path in
`SetupFaction`. All of it is ruleset code — none of the four requires an engine change — and it
buys a ruleset that runs correctly from `game.in` and `players.in` alone.

## Cost

Roughly a week and a half of focused work to a playable prototype: skeleton and registration,
monster retuning and the climate band are about a day each; the landmass mask is one to two days,
most of it looking at generated maps in `map_viewer/`; the two invasion fronts are three to five
days together; re-keying the gateways to latitude bands, the slot registry with its per-turn
rebuild, and the starting alliances are two to three days together.

Start selection is still among the cheaper items rather than the most expensive, because sections
7 and 9 buy an existing algorithm rather than a new one — keeping the draw inside the engine adds
roughly a day over pushing it outside. Balancing afterwards is measured in weeks and dominates the
total.

## Consequences

- **No entry in `docs/fork/patches.md`, because nothing outside `rimefall/` changes.** A new
  ruleset directory is additive, so an upstream sync cannot conflict with it. This is the main
  practical dividend of section 0.
- The natural generator stays unused, so `rimefall` worlds have no rivers, no rainfall model and
  no historical buildings. Anyone comparing a `rimefall` map to a `neworigins` map will notice.
- A new binary, so `docs/fork/downstream-consumers.md` applies: the two Python projects gain a
  ruleset they may build, and the JSON report shape must not change to accommodate it.
- **The ruleset stays runnable from the binary alone**, which is the constraint section 9 is built
  around. The Python projects gain nothing they must do; a signup cap is worth having so players
  are not turned away, but the engine handles its absence correctly.
- **A refused faction is a new thing for a game master to see.** `SetupFaction` returning 0 logs a
  failure and discards the faction, which reads like an error rather than a full game. `GAMEMASTER.md`
  has to describe it, and the message should say the world is full rather than that something broke.
- Gateway objects carry numbers from `buildingseq`. Adding and removing them every turn would churn
  those numbers in reports and in the JSON. The rebuild must therefore reuse a fixed object per band
  rather than destroying and recreating it — an implementation detail that is easy to get wrong and
  visible downstream when it is.
- **Starting attitudes are visible in the JSON report** (`object.cpp` passes the attitude into
  `build_json_report`). Factions will see themselves allied to strangers on turn one, so the
  rulebook and the intro page have to explain it or it reads as a bug.
- ALLY as a starting default means turn-one `GIVE UNIT` between strangers is legal, and that
  theft and assassination against a band partner fail while any ally is watching. Both are
  intended. Neither is obvious from a report, so both belong in the rulebook.
- Snapshot registration is easy to get half-right. The ruleset must be added to **both**
  `run-snapshots.sh` and `update-all-rule-snapshots.sh`; missing the second means the next
  re-record silently skips it.
- Every horde tunable lives in one ruleset file, so balancing never touches shared headers — but
  it also means none of it is visible in `game.in`, and a rebalance changes behaviour for games
  already in progress.
- Placement and starting attitudes both draw on `rng`, and both run at faction creation. Adding or
  reordering draws there moves every snapshot downstream, so the placement chooser wants to be
  settled before baselines are recorded.
