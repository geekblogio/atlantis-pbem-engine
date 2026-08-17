# Divergence register

> **Audience:** anyone asking "how does this fork differ from upstream, and why?"
> **Provenance:** FORK-LOCAL.

**Read this when** you want to know how this fork diverges — or you are adding divergence and
need to record it.

**Append-only.** Entries are never deleted. When something is accepted upstream and arrives
back through a sync, mark the entry *Landed upstream* and leave it in place; the history of why
we carried it is the point.

## How to use this file

Every commit on `master` that is not on `upstream/master` must correspond to exactly one entry
here, **and the entry must name a key** so the check can be run rather than eyeballed. Two keys
are accepted:

| Key | For | Available |
| --- | --- | --- |
| `#<pull request>` | anything squash-merged — GitHub puts `(#N)` in the subject | **before** the merge |
| short SHA | the `upstream/*` branches, rebase-merged and therefore carrying no number | only after it |

**Prefer the pull request number.** It exists before the merge, so the entry ships with the change
it describes: open the pull request, then write the entry naming `#N` and push it onto the same
branch. A SHA can only be added once the squash-merge has invented it, which costs a second pull
request every single time.

```bash
git log --format='%h %s' upstream/master..master |
  while read -r sha rest; do
    pr=$(printf '%s' "$rest" | grep -oE '\(#[0-9]+\)$' | tr -d '()')
    grep -q "$sha" docs/fork/patches.md && continue
    [ -n "$pr" ] && grep -qE "${pr}([^0-9]|$)" docs/fork/patches.md && continue
    echo "UNREGISTERED $sha $rest"
  done
```

Silence means the register is complete. Any output means either the entry is missing or the
commit should not have landed. Run it after every sync.

This rule used to skip commits that touched nothing but this file, because such a commit cannot
name its own SHA — write it in, and you need a further commit to record *that*, and so on. A
pull request number has no such problem: it is known while the file is being edited, so a
register-only pull request registers itself and the exemption is gone.

The **`Divergence Register`** job in [`ci.yml`](../../.github/workflows/ci.yml) enforces the same
rule from the other end, failing a pull request whose number this file does not name — so the gap
surfaces during review instead of after the merge. It exempts `upstream/*` branches and
bot-authored pull requests: the first have no number to register, the second no way to write one.
Both are added by SHA afterwards.

## Status vocabulary

| Status | Meaning |
| --- | --- |
| **Prepared** | on an `upstream/*` branch, self-contained, ready to offer. Not submitted. |
| **Offered** | a pull request exists at `Atlantis-PBEM/Atlantis` |
| **Landed upstream** | accepted; the divergence disappears at the next sync |
| **Fork-local** | deliberately never offered |
| **Declined** | offered and rejected; we keep it |

**Nothing has been offered.** Per the standing rule in `CLAUDE.md`, upstream candidates are
prepared and registered, not submitted; submitting is a separate, explicit decision. See
[upstream-sync.md](upstream-sync.md) for the mechanics when that decision is made.

---

## Bug fixes — prepared for upstream

Each of these landed here on an `upstream/*` branch, rebase-merged so the individual commits
stay cherry-pickable, and touches nothing fork-local.

### `ee97aa4` — dangling pointers in the data tables

`modify.cpp`. The `modify_*` helpers assign `arg.c_str()` of a `const std::string&` parameter to
raw `char const*` table fields (`MonType::special`, `WeaponType::baseSkill`, `MountType::skill`,
…). The parameter is a temporary at the call site, so the table keeps a pointer into freed
memory and combat crashes later. Fixed with a `static std::list<std::string>` intern pool, seven
call sites converted. Also removes a `delete` on a pointer never allocated with `new`.

**Upstream value: high.** A memory-safety bug with a crash behind it.

### `535a0a0` — unguarded `find_special()` optional

`items.cpp`, `specials.cpp`, `army.cpp`, `battle.cpp` — six sites. `find_special()` returns
`nullopt` for an empty or unknown special string, and some ruleset data carries `""` rather than
`NULL`. Calling `.value()` throws `bad_optional_access`. Each site got the behaviour that fits
it: an empty description, `return 1`, `continue`, `return`, no shield, no mount special.

**Upstream value: high.** A crash, fixed minimally.

### `f6111d2` — use-after-free reading an anomaly's region

`neworigins/extra.cpp`. `far_anomaly` is deleted and then read three times through `->region`.
GCC's `-Wuse-after-free` reports all three. The region pointer is now taken before the delete.

**Upstream value: high.** Unambiguous, one-line shape.

### `e5b090b` — nexus gateways named "Dummy"

`basic/map.cpp`, `standard/map.cpp`, `havilah/map.cpp`, `neworigins/map.cpp`. `set_name()`
refuses to rename an `O_DUMMY` object, which is the constructor default, so every nexus gateway
in a freshly generated world was called *Dummy*. `o->type = O_GATEWAY` now comes first.

A **regression**, introduced by upstream's `89f4d40` *"Update parser to no longer use AString"*.
That is why the committed fixtures carry correct gateway names while fresh worlds do not.

**Upstream value: high.** Player-visible, in four rulesets.

### `a388a00` — `TerrainDefs` indexed past the end

All six rulesets' `map.cpp`. In `ARegionList::RandomTerrain` the `!= R_NUM` guard sat *behind*
the `TerrainDefs[...]` access it was meant to protect. `R_NUM` is 63 and `TerrainDefs` holds 63
entries. Reached on every world generation with an underworld level. The check was moved in
front; nothing was added.

**Upstream value: high.** The original port fixed only `havilah`; the code is byte-identical in
all six.

### `3d888cc` — unescaped angle brackets in the rulebook

`genrules.cpp`. The `ANNIHILATE` example header contains `<5, 5>`, written verbatim into the
HTML rulebook, so a browser swallows it as an unknown tag and the published rules read *"at
coordinates on the surface"*. Escaped. Moves the `neworigins` rules baseline.

**Upstream value: high.** Small, visible, obviously right.

### `85d016c` — `inline` on an out-of-line definition

`faction.cpp`. `Faction::gets_gm_report` is defined out of line and called from `game.cpp`;
`inline` makes that ill-formed. It links only because `-O0` still emits an out-of-line copy —
`nm` shows a weak symbol at `-O0`, no symbol at all at `-O2`.

**Upstream value: high.** A latent link failure that appears the moment anyone optimises.

### `3cc4269` — variables gcc only warns about at `-O2`

`aregion.cpp`, `game.cpp`, `monthorders.cpp`, `spells.cpp`, `unit.cpp`, `neworigins/map.cpp`,
`unittest/market_order_test.cpp`. Fifteen `-Wmaybe-uninitialized` warnings, invisible in an
unoptimised build. Three are real: `mantype` indexing `ItemDefs` when a unit has no man item,
`Province::GetLocation` building its return value from three uninitialised ints, and
`CanUseWeapon` passing an unwritten skill index to `Practice()`.

**Upstream value: medium.** Upstream does not build with `-O2` and so never sees these. The
three real ones justify it; the rest are defensive.

### `d58ff37` — CMake numeric operators on strings

`CompilerSupport.cmake`, plus cleanup on two early exits in `run-game-snapshots.sh`.
`CMAKE_CXX_COMPILER_ID EQUALS "GNU"` uses the *numeric* comparison, so the branch that disables
the ranges probe for GCC below 12 never fires. `STREQUAL` and `VERSION_LESS`.

**Upstream value: high.** Trivial and unambiguous. No effect on any supported toolchain today.

### `c82aaed` — line endings and swallowed snapshot fixtures

`.gitattributes` (new) and `.gitignore`. The bare `game.*` / `report.*` / `players.*` patterns
match the fixtures of a **newly created** turn directory, so `new-turn.sh` silently produces an
incomplete commit and the replay then fails with *"turn N missing"*. `.gitattributes` pins
`eol=lf` on the byte-compared trees, without which a re-record on Windows destroys all of them.

**Upstream value: high.** The same bug exists upstream. Verified content-neutral: the index
holds zero CR bytes across all 325 snapshot files.

### `6e16846` — null skill when recruiting into a unit that already holds men

`unit.cpp`, plus a regression test in `unittest/market_order_test.cpp`. `Unit::AdjustSkills`
looks for the skill with the most study days starting from zero days and a null candidate, so a
unit whose skills *all* have zero days reaches the elimination loop with `maxskill` still null
and dereferences it. Recruiting creates exactly those skills: `Game::DoBuy` seeds specialized
skill experience, which carries experience and no days at all. Any ruleset with
`REQUIRED_EXPERIENCE` — of ours only `kingdoms` — therefore segfaults when a unit that already
holds recruited men buys one more, for every race with more than one specialized skill. Fixed by
falling back to the first skill; *skipping* the elimination instead leaves several skills on a
non-leader and kills the `neworigins` snapshot replay with a bus error.

Reported from the downstream bot project, which hit it in a live game.

**Upstream value: high.** A crash on an ordinary order, fixed in one condition.

---

## Fork-local

### `5b44837` — build at `-O2`, `genrules.cpp` exempt

`Makefile`, `CMakeLists.txt`, `s/configure`, `.github/workflows/ci.yml`. Measured 0.217 s →
0.041 s per turn (5.3×) on gcc 13.3.0. `genrules.cpp` stays at `-O0`: 6.6 s / 0.76 GB against
119.9 s / 1.81 GB, with gcc reporting *variable tracking size limit exceeded*.

Also switched the cmake CI job from `Debug` to `RelWithDebInfo` so the binary the snapshot suite
tests is built the way the shipped one is. That surfaced one `-DNDEBUG` casualty, fixed in the
same pull request.

**Upstream value: borderline.** Upstream ships debug builds deliberately. The `genrules`
pathology is worth reporting to them regardless of which default anyone prefers.

### `6f033dd` — `ATLANTIS_SEED`

`game.h`, `game.cpp`, `main.cpp`. A public `Game::set_deterministic_seed(int)` assigning the
existing private `init_random_seed` hook — the same extension point the unit tests use — driven
by an environment variable. Affects `new` only; `run` restores the seed from `game.in`.

**Upstream value: worth offering.** Reproducible world generation helps anyone debugging map
generation. Opt-in and inert when unset. The debatable part is the delivery mechanism; see
[ADR 0005](../decisions/0005-environment-variables-for-fork-hooks.md).

### `2b1d369` — `ATLANTIS_SIM_MODE`, `ATLANTIS_NO_GM_REPORT`

`main.cpp`. `SIM_MODE` narrows `REPORT_FORMAT` to JSON; `NO_GM_REPORT` clears `GM_REPORT`, worth
roughly 70% of a turn on a snapshot-sized world. Two switches, not one, because a *recorded*
simulation still wants the GM report as ground truth.

**Fork-local.** An environment variable overriding a ruleset global is a policy decision
upstream would reasonably reject. The right upstream shape is a `--no-gm-report` flag.

### `#32` — `ATLANTIS_FORCE_GM_REPORT`

`main.cpp`, one block plus a `usage()` line. The mirror of `ATLANTIS_NO_GM_REPORT`: sets
`GM_REPORT` rather than clearing it, so a ruleset that ships with the world-wide report off
writes it every turn. That is `basic` alone among the seven.

Read *after* `ATLANTIS_NO_GM_REPORT`, so setting both is defined rather than diagnosable: force
wins. Inert when unset, like the other three.

**Fork-local**, for the same reason as `2b1d369`: an environment variable overriding a ruleset
global is a policy decision upstream would reasonably reject, and the right upstream shape is a
flag. It is also less obviously *wanted* upstream than the others — `GM_REPORT = 0` in `basic`
is presumably deliberate, and this fork's reason for overriding it is that a hosted game keeps
a world history the ruleset's author never needed.

**Measured, not read off the source.** `engine_contract_check.py` asserts on every CI run that
`basic` writes no `report.1.json` on turn two. With the variable set, that assertion goes red on
`basic` and stays green on the other five — which is the evidence that the switch does exactly
one thing.

### `e89a198` — JSON report for `havilah`

`havilah/rules.cpp`, one flag. `neworigins` already carries `REPORT_FORMAT_JSON` upstream.
Purely additive: the text report and template are unchanged.

**Upstream value: borderline, low risk.** Consistency with what upstream already does elsewhere.

### `#30` — JSON report from every ruleset

`standard/rules.cpp`, `basic/rules.cpp`, `fracas/rules.cpp`, `kingdoms/rules.cpp`: the same flag
`e89a198` set for `havilah`, so `report.<n>.json` exists whichever ruleset a consumer builds.
Includes the 28 re-recorded `standard` turn fixtures — the re-record produced 28 new files and
**zero modified** ones, which is the evidence that building the JSON document draws no random
numbers and moves nothing downstream of it.

Two unrelated repairs ride along: the explicit `obj/genrules.o` recipe from `5b44837` became a
target-specific `CFLAGS += -O0`, because an explicit recipe overrode the static pattern rule and
make said so twice per sub-make; and `.idea/` joined the ignored IDE directories.

**Fork-local, low upstream value.** The same reasoning as the `havilah` flag, four rulesets
wider. Upstream has no consumer that reads the JSON report.

### `0cf80ff` — `neworigins8`, the fork's own ruleset

`rules.cpp` is a copy of `neworigins/rules.cpp` with
`UPKEEP_FOOD_VALUE` 30 → 50; the other four files are `#include` shims. `RULESET_NAME` and
`RULESET_VERSION` stay identical to `neworigins` because `Game::OpenGame` refuses a `game.in`
whose name differs. See [ADR 0006](../decisions/0006-neworigins8-as-its-own-ruleset.md).

**Fork-local by definition.**

### Infrastructure

`2906bc6`, `426eb8d`, `001a5f4`, `dfe8cb0` — `.github/**`, `CLAUDE.md`, `CONTRIBUTING.md`,
`README.md`, `docs/fork/`, `docs/decisions/`: the CI pipeline, branch protection, Dependabot,
the vendored-header refresh workflow, the pull request template, and this documentation.

**Fork-local, permanently.** The `Upstream Hygiene` CI job fails any `upstream/*` branch that
touches these paths, so they cannot leak into a contribution by accident.

`c1656a7` (map viewer type check, Dependabot) is infrastructure but does add
`map_viewer/tsconfig.json` and two `package.json` scripts, which are upstream-friendly on their
own.

### Developer documentation

`1aeef70`, `68a9559`, `a9ba283`, `69eeff6` — `docs/architecture.md`, `docs/rulesets.md`,
`docs/build-and-test.md`, `docs/snapshot-tests.md` and `docs/interface/**`, plus the three
fork-local documents and the decision records.

Upstream ships no developer documentation by choice; the README says so. The engine reference
and the interface specification are **upstream-friendly in content** — each carries a
`Provenance:` header saying so — but offering documentation upstream is a conversation about
their project's shape, not a bug fix, and is not proposed here.

### `#35` — the fork's own `CHANGELOG` section

`CHANGELOG` had stood untouched since 2019, ending at *5.1.0 -> 5.2.0*, so none of the entries in
this register had ever reached it. `#35` opens *Changes in 5.2.5, in this fork* and fills it from
this file, which is why every line there maps onto an entry here.

Deliberately **not** titled *5.2.0 -> 5.2.5*: upstream set that constant themselves in `c38361d`,
and our work sits on top of it without moving it. Upstream's own 5.2.0 -> 5.2.5 — 167 commits
across four years — stays unrecorded, and the section's preamble says so rather than letting the
gap pass for completeness.

**Fork-local, and a standing cost.** `CHANGELOG` is not on the `Upstream Hygiene` list, because
upstream maintains the file too. This section will conflict in every future
`chore/sync-upstream-*`. Keeping the fork's history under `docs/fork/` instead was considered and
not taken.

### `#36` — the `rimefall` ruleset decisions

`docs/decisions/0010-…`, `0011-…`. Two records settling a new ruleset before any of it is written:
one tapering continent from tundra to desert, two monster fronts, start locations across five
latitude bands, neighbouring factions starting mutually allied, and victory by election once both
hordes are put down.

0010's governing constraint is that **nothing outside the ruleset directory changes**, which is why
this entry expects no engine divergence to follow. The classic map generator is used instead of the
natural one for exactly that reason: reshaping `Map::Generate`'s latitude model would have meant
touching `mapgen.cpp`.

0011 exists because planning found two design gaps — no victory condition and no invasion trigger —
plus a self-contradiction in 0010 section 8. Per the append-only rule 0010 was left as accepted and
extended rather than corrected in place.

**Fork-local, permanently.** A new game variant is this fork's own; nothing here is offered
upstream.

### `#37` — a guard against agent writes to upstream

`.claude/hooks/upstream-guard.sh`, `.claude/settings.json`, `CONTRIBUTING.md`,
`.github/workflows/ci.yml`. A `PreToolUse` hook that refuses a GitHub write carrying no `--repo`
inside a clone that can reach upstream, asks a human before anything deliberately aimed there, and
leaves reads alone. It lives in the repository rather than in `~/.claude` so it applies in every
clone on every machine, and it matches its own paths by suffix so the self-protection survives a
different checkout location.

Written after an agent opened a pull request against upstream on 2026-08-14 by omitting `--repo`;
`gh` defaults to a fork's parent, so the command named upstream nowhere. `CONTRIBUTING.md` carried
the same hazard — its documented merge command had no `--repo`, and it claimed a default repository
was already configured locally, which was untrue. Both corrected here.

`Upstream Hygiene` now rejects `.claude/` on `upstream/*` branches, so the guard cannot leak into
an upstream pull request.

**Fork-local, permanently.** Upstream is not a fork and has no parent to default to; the hazard
does not exist there.

### `#38` — the `rimefall` skeleton, and its registration

`rimefall/` (new), `CMakeLists.txt`, `Makefile`, `.gitignore`,
`snapshot-tests/run-snapshots.sh`, `snapshot-tests/update-all-rule-snapshots.sh`,
`snapshot-tests/rules/rimefall.html`. Stage 1 of the ruleset settled in `#36`: a binary that
builds, generates a world and processes a turn, playing as `neworigins` without the underworld.
`rules.cpp` is a copy with four fields zeroed; the other four files are shims.

**0010 said this register would need no entry, and that is wrong for this stage.** Its section 0
constraint — nothing outside `rimefall/` changes — holds for the ruleset's behaviour, but a new
ruleset has to be *registered*, and every registration touchpoint is a file upstream also
maintains. A new directory cannot conflict on an upstream sync; these six shared files can, and
`Makefile` carries the change in six separate places. That is the divergence worth recording, and
it is the reason this entry exists at all.

**Fork-local, permanently.** A new game variant is this fork's own; nothing here is offered
upstream.

### `#39` — the `rimefall` continent

`rimefall/world.cpp`, `rimefall/map.cpp`, `rimefall/rimefall.h` (new), `rimefall/rules.cpp`.
Stage 2: the taper and the climate bands. `world.cpp` and `map.cpp` stop being shims and become
the ruleset's own; `CreateWorld` drops the generator prompt and every underworld, underdeep, abyss
and shaft block; `GetRegType` replaces NewOrigins' equator-symmetric latitude fold with a
monotonic five-band index; `MakeLand` confines continent seeding and growth to the taper.

**Wholly inside `rimefall/`, so an upstream sync cannot conflict with it** — unlike `#38`, which
had to touch the shared registration files. This is the shape 0010 section 0 predicted, and the
rest of the ruleset should look like this rather than like `#38`.

Two things found while building it, recorded because both contradict what was written beforehand:

- **`OCEAN` and the taper are one setting.** `MakeLand` loops until the ocean count falls below
  its target, so a taper too small to hold that much land is an infinite loop. `OCEAN` rose from
  55 to 65 to match, and the function now checks the two against each other up front and throws
  with both numbers, plus a stall counter for the same failure reached from the other side.
- **Adamantium comes from mountain alone.** The plan recorded mountain and desert as the sources
  of mithril, rootstone and adamantium; desert in fact yields no adamantium. That makes mountains
  in every band a harder requirement than stated, and the southern bands are weighted up because
  they have the least land to roll anchors on.

**Fork-local, permanently.** A new game variant is this fork's own; nothing here is offered
upstream.

### `#40` — the decision to add a gateway hook to the engine

`docs/decisions/0012-…`. A record, no code. It **amends 0010 section 0**, which forbade `rimefall`
from changing anything outside its own directory.

Building stage 3 showed that constraint could not be met. A gateway's destination does not survive
the move: `monthorders.cpp` takes only the *terrain* of the region a ruleset chose and rebuilds the
candidate set from the whole map, so gateways cannot be keyed on latitude band. The function that
does it, `get_starting_region_candidates`, is declared in `aregion.h` but defined in `aregion.cpp`
— engine code, unlike the world-generation chain around it — so no ruleset can override it.

0012 permits **one** hook, `filter_gateway_destinations`, called before the occupancy cascade and
defined empty by every ruleset but `rimefall`. The engine change itself is not in this pull
request; it is registered separately when it lands.

**Fork-local for now, and upstream-worthy in principle.** Start-selection policy is ruleset-legal
by the criterion in `docs/rulesets.md`, so the engine hard-coding *scatter by terrain* is arguably
upstream's problem too. Per [0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is
prepared and registered, **not offered**.

### `#41` — band-keyed start selection, and the one engine hook it needed

`game.h`, `monthorders.cpp`, and `extra.cpp` in all seven rulesets plus `rimefall/`. **This is the
first `rimefall` divergence that touches the engine**, permitted and bounded by
[0012](../decisions/0012-a-ruleset-hook-for-gateway-destinations.md). `#38` touched only
registration files and `#39` nothing shared at all.

The engine side is one hook, `Game::filter_gateway_destinations`, called in the gateway branch of
`Game::DoAMoveOrder` after the candidate list is built and before the occupancy cascade runs. Every
ruleset but `rimefall` defines it empty. **Verified as a no-op:** with the hook in place and the
ruleset side not yet written, unit test output and the entire snapshot run were byte-identical to a
baseline recorded beforehand.

Also carries [0013](../decisions/0013-the-gateway-hook-sets-the-candidate-list.md), which corrects
0012's description of the hook. 0012 called it a filter; the ruleset in fact *sets* the list,
because a start slot curated at world creation must stay reachable even after the engine's live
candidacy test would stop listing it. The decision is unchanged, only its contract made exact.

The ruleset side re-keys the gateways from terrain to latitude band. One gateway object is one
start slot, which is also how the registry survives a save cycle — objects are persisted,
`rulesetSpecificData` is not, and `ARegion::Writeout` has no field to add one to without changing
`game.in`.

**Upstream-worthy, not offered.** Start-selection policy is ruleset-legal by the criterion in
`docs/rulesets.md`, so the engine hard-coding *scatter by terrain* is arguably upstream's problem
too. Per [0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) the hook is prepared and
registered here and **nothing has been offered**. The `rimefall/` half is fork-local permanently.

**This is the divergence to watch on an upstream sync.** `monthorders.cpp` and `game.h` are
actively maintained upstream, and the seven near-empty `extra.cpp` definitions will conflict on any
upstream change near `movement_forbidden_by_ruleset`.

### `#42` — the decision that a refused faction must not stop the turn

`docs/decisions/0014-…`. A record, no code. It is the **second** engine change for `rimefall`, and
[0012](../decisions/0012-a-ruleset-hook-for-gateway-destinations.md) required exactly that: one
hook for one purpose, anything further argued again from scratch.

`Game::SetupFaction` returning 0 is how a ruleset refuses a faction, and `#41` made refusal an
ordinary event — a `rimefall` world holds a fixed number of start slots and filling them is normal.
But `ReadPlayers` sets `return_code = false` on refusal, so the whole turn is abandoned: no
`game.out`, no reports, non-zero exit. 0010 section 9 described that path only as *discard and log*.

That matters because this repository is an engine supplier and two Python projects drive the binary
as a subprocess. A non-zero exit is how they learn a turn failed, and raising it for something that
is not a failure trains the operator to ignore the only signal there is.

0014 decides the faction is skipped and the turn runs, and records the trap: the parse loop applies
each subsequent line to the current faction pointer, so continuing naively would write the refused
newcomer's name, address and password over the **previous** player's registration.

**Upstream-worthy, not offered.** The fix is not conditional on any ruleset and helps `neworigins`
first, whose registration close has the same defect. Per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered,
**not offered**.

### `#43` — a refused faction no longer abandons the turn

`game.cpp`, `GAMEMASTER.md`, `docs/interface/file-formats.md`, `docs/rulesets.md`, `CHANGELOG`.
The engine half of [0014](../decisions/0014-a-refused-faction-does-not-abort-the-turn.md), and the
**second and last** engine change 0012's boundary has been opened for.

`Game::ReadPlayers` set `return_code = false` when `AddFaction` returned null, so a faction a
ruleset declined ended the whole run: no `game.out`, no reports, non-zero exit, and every other
player's turn lost until someone edited `players.in` by hand. It now logs the refusal and carries
on.

The change is four lines and one of them is the point: `fac` is left null and `lastWasNew` cleared,
so the refused block's `Name:`, `Email:` and `Password:` fall through the `else if (fac)` guard
instead of being written over the previously read faction's registration. **Verified by filling a
`rimefall` world, adding a twenty-first faction, and confirming the twentieth kept its own name,
address and password and that the newcomer's details appear nowhere in `game.out` or
`players.out`.** Exit code 0, all reports written, snapshots and unit tests unmoved.

**Behaviour change for `neworigins` as well**, deliberately: its annihilation registration close
had the same defect.

**Upstream-worthy, not offered.** Nothing about the fix is conditional on a ruleset and it helps
`neworigins` first. Per [0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is
prepared and registered, **not offered**.

### `#44` — starting alliances by proximity

`rimefall/extra.cpp`, `rimefall/rimefall.h`, `docs/rulesets.md`, `GAMEMASTER.md`, `CHANGELOG`.
Stage 4, and **wholly inside `rimefall/`** — no engine change, unlike `#41` and `#43`.

Neighbours begin at `ALLY`, both sides, to the holders of every start slot within
`RIMEFALL_ALLY_RADIUS`. By proximity rather than band, per 0011 section 5.

The interesting part is how it fires exactly once without storing anything. Testing whether a
faction *looks* unallied cannot distinguish a newcomer from one that has renounced every alliance,
so that faction would be forced back into its bloc every turn — the opposite of 0010 section 8's
"a starting default, not a pact". Instead it reads the **arrival event**: a gateway still named
`Gateway to …` whose land is now held can only mean its holder arrived since the last turn, and the
rename to `Sealed gateway to …` in the same pass makes it unrepeatable. The seal is already
persisted in `game.out`, so no new state exists. Verified: a faction that renounces its only
alliance still has none two turns later.

The radius was chosen by measurement rather than taste — 16, 12, 9, 7, 5 and 3 on one fixed-seed
world filled to all twenty slots, then 7 confirmed across three seeds. The table is in
`rimefall.h` beside the constant.

**Fork-local, permanently.**

### `#45` — the makefile tracks header dependencies

`Makefile`, `docs/build-and-test.md`, `CHANGELOG`. Not a `rimefall` change at all: a plain build
bug that `rimefall` happened to expose.

The makefile compared a `.cpp` against its `.o` and nothing else, so editing **any** header
rebuilt nothing. `game.h` included. The ruleset shims were hit twice over, because a change to
`neworigins/extra.cpp` did not rebuild the `neworigins8` and `rimefall` objects that `#include`
it — the pattern [0006](../decisions/0006-neworigins8-as-its-own-ruleset.md) is built on.

Fixed with `-MMD -MP` and `-include` of the generated lists. `DEPFLAGS` is deliberately separate
from `CFLAGS`, which is also passed to the link steps where it would emit a stray dependency file
named after the executable. The hand-written dependency on `external/boost/ut.hpp` is now generated
like everything else and was removed.

Found the hard way: a tuning constant was changed in a header six times and measured six times,
and every measurement was of the same unchanged binary. CI never showed any of it, because CI
always builds from scratch.

**Upstream-worthy, not offered.** Nothing here is fork-specific. Per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.

### `#46` — the two powers, renamed

`rimefall/extra.cpp`, `docs/rulesets.md`. Six existing monsters reskinned: rimeworm, hoarwyrm,
frostbound, thawless, winterwright for the north, saltdrake for the east. **No entry added to
`gamedata.cpp`**, per 0010 section 4, and no engine change — wholly inside `rimefall/`.

Renaming needs **both** tables and it is easy to get half right. `ModifyItemName` covers `ItemDefs`;
a monster unit is named from `MonDefs`, which `Game::MakeLMon` reads directly. Written with only
the first at first, and a generated world still said "Ice Wurms (158)". No `modify_monster_name()`
helper exists and none was added: `find_monster()` is public in `items.h` and returns a mutable
reference, which is what the other `modify_monster_*` helpers use internally.

Recorded as out of reach: `MakeLMon` names a crypt's stack with the hard-coded literal `"Undead"`,
so that one label survives the rename. Reaching it would be a third engine change under 0012's
boundary, and it is lair furniture rather than either invasion front.

**Fork-local, permanently.**

### `#47` — one production per item and skill (0015)

`docs/decisions/0015-…`, `docs/decisions/README.md`. The record settling where the fix for a
duplicated region production belongs, written before the fix as `#42` was before `#43`.

A region's `products` can hold the same item twice: `ModifyTerrainItems` lets a ruleset write any
item into any slot of a terrain, `fracas` points slot 5 of the desert at mithril where slot 4 of
the shared table already holds it, and each slot rolls separately. Only the first entry is ever
harvestable, because `RunAProduction` deletes the month order of every unit that worked it.

**The record exists because the obvious repair is the wrong one.** Letting production fall through
to the next deposit would have repaired existing worlds without touching their save files — and
would have taken an affected desert from 3 mithril a month to 8. Dropping the repeat and keeping
the first is the only choice that moves no yield anywhere.

**Fork-local, permanently**, like everything under `docs/decisions/`. The fix it governs is
upstream-worthy and registered separately.

### `#48` — a region no longer produces the same item twice

`aregion.cpp`, `aregion.h`, `economy.cpp`, `unittest/produce_test.cpp`, `docs/interface/json-report.md`,
`CHANGELOG`. The engine half of [0015](../decisions/0015-one-production-per-item-and-skill.md).

`ARegion::products` could hold two entries for one item, and the second was permanently
unharvestable, because `RunAProduction` deletes the month order of every unit that worked the
production it was handed. `ModifyTerrainItems` puts mithril in a second slot of the `fracas` desert
where the shared table already has it, both slots roll separately, and the report then promises a
deposit nobody can mine. `remove_duplicate_products()` drops the repeat, from `SetupProds` for a new
world and from `Readin` for one already in play.

**The detail that keeps it cheap:** the repeat is discarded *after* the slot has been rolled and the
`Production` constructed, so the sequence of random draws does not move and neither does any other
region. Measured rather than assumed — a seeded 64×64 `fracas` world regenerates byte-identical
apart from the removed entries, and the same world generated *before* the fix and then loaded *with*
it lands on a byte-identical `game.out`.

**Neither half of this could have been caught by the snapshot suite**, whose two blind spots —
world generation, and `fracas` beyond its rulebook — are exactly where the bug lived. See
[../interface/compatibility.md](../interface/compatibility.md).

`markets` was checked for the same shape and cannot reach it; the scan that established this used a
temporary probe which is deliberately not in the tree.

**Upstream-worthy, not offered.** The engine, the shared terrain table and `fracas` are all
upstream's; nothing here is fork-specific. Per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.

### Maintenance of this register

`#28`, `#29` — corrections to this file itself. `#28` added the SHAs of commits that prose
covered but no key named; `#29` stopped the acceptance check chasing its own tail, by skipping
commits that touched nothing else.

`#31` — replaced the SHA-only key with the rule above and added the `Divergence Register` job to
`ci.yml`. It also retired `#29`'s exemption, which was only ever needed because a commit cannot
name its own SHA. This entry was written while `#31` was open and pushed onto the same branch,
which is the whole point of the change.

`#34` — added the entry for `6e16846`, which came off an `upstream/*` branch and could only be
keyed once the rebase-merge had invented its SHA. The register-only pull request that carries it
names itself here, which is exactly the second push `#31` describes.

**Fork-local, permanently**, like everything under `docs/fork/`.

### `bea2a21` — vendored nlohmann/json 3.11.3 → 3.12.0

Produced by the `Vendored Deps` workflow, which runs `make check-libraries` monthly and opens a
pull request when a header moved. `external/boost/ut.hpp` was already current at 2.3.1.

Registered by SHA after the fact, not by number: the workflow opens its pull requests with
`GITHUB_TOKEN`, so the `Divergence Register` job exempts them.

**Mechanical, and temporary.** It is a divergence only until upstream refreshes their own copy;
there is nothing here to offer them that their own `make check-libraries` would not produce.

Worth knowing about the workflow that creates these: the pull request is opened with
`GITHUB_TOKEN`, and GitHub does not trigger workflow runs from that token. **The pull request
therefore arrives with no `CI` status check and branch protection holds it pending.** Close and
reopen it as a user to start the pipeline — the pull request body says so, and it is not
optional. Re-running the workflow does not help; it reproduces the same condition.

The verification that matters is that the bump builds warning-clean: the header is included
under `-Werror` by every translation unit, so the change invalidates ccache completely and both
CI builds run cold. On this bump both passed, along with 19 unit test suites and 35 snapshot
checks.

---

## Prepared for upstream — documentation

### `09e3b05` — the gamemaster guide's compiling chapter

`GAMEMASTER.md` section 2. It listed object files removed years ago (`astring.o`, `fileio.o`,
`shields.o`, `template.o`), a sample compile without `-std=c++20` or `-Werror`, and step-by-step
instructions for Cygwin, Visual C++ and Dev-C++ 5 beta — including a CVS checkout from
`cvs.dragoncat.net`. CMake, which CI uses, was not mentioned at all. Following it would not
produce a working build.

206 lines out, 67 in, table of contents updated. Deliberately self-contained: it references no
file upstream does not have, so it applies there unchanged.

**Upstream value: high.** The staleness is upstream's, and the replacement was written to apply
to their copy as-is.

---

## Considered and not taken

- **Division by zero in `TownStatistics`** and the **null guard in `skillshows.cpp`** were in the
  original port but upstream has since fixed both independently. Only an explanatory comment
  differed. Not worth a pull request.
- **Turn fixtures for `neworigins8`.** It shares all engine and world-generation code with
  `neworigins`; recording 14 more turns duplicates coverage rather than adding it.
- **A full audit of the other 186 `TerrainDefs[x->type]` accesses.** Its own undertaking;
  `a388a00` fixes the one pattern where the guard is demonstrably misplaced.
- **A null check on `GetUnitId()`'s result in `monthorders.cpp`**, noticed while fixing the
  uninitialised `t` next to it. Pre-existing and separate.
- **Section 1 of `GAMEMASTER.md`**, which still points at a Yahoo Groups tarball for release
  5.1.0. Stale in the same way as section 2, but a separate change.
