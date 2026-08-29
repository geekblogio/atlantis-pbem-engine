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

### `ad07b87` — a recruited unit could study nothing in `kingdoms`

`unit.cpp`, plus a regression test in `unittest/unit_test.cpp`, fixed in `#71`. `Unit::CanStudy`
enforced the one-skill rule for non-leaders on the *number of skill entries* (`skills.size() > 0`)
rather than on study days. With `REQUIRED_EXPERIENCE` — of ours only `kingdoms` — `Game::DoBuy`
seeds the race's specialized skills with experience and zero days, so any unit that had ever
bought a man already held entries and was refused every skill it tried to begin, its own
specialities included.
Experience is not a level: `GetRealSkill` reads days only, and `Unit::Practice` pays into
experience rather than days whenever the flag is on, so nothing else closed the gap either. Only
the starting leaders could learn anything.

The condition arrived in `2324a2a` (2012) as a *display* fix — "don't display skills as studyable"
— eight years after `07f4abd` (2004) added `REQUIRED_EXPERIENCE`, `kingdoms = 50` and the recruit
seeding in the same commit. `Unit::Study`, `Unit::Practice` and `Unit::AdjustSkills` all measure
this same rule in days; `CanStudy` was the one place that measured it in entries. The generated
rulebook, written by the author of the experience system, still promises a specialized unit level
1 after one month of study.

Fixed by counting a skill as known only when it has days. A non-leader with days in one skill is
still refused a second. The other five rulesets cannot be reached by the change: with
`REQUIRED_EXPERIENCE` off nothing writes experience — `SkillList::SetDays` erases a zero-day entry
unless it carries experience, `SetExp` and `Readin` never create one without — so every entry
there already has days. Verified by replaying the same four-turn recruit-and-study scenario under
both binaries in `standard`, `basic`, `fracas`, `havilah` and `neworigins`: byte-identical.

Reported from the downstream bot project, which carries a second `kingdoms`-only campaign because
of it.

**Upstream value: high.** Upstream ships `kingdoms` with the same two flags and the same rulebook.

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

### `#49` — the two invasion fronts

`rimefall/extra.cpp`, `rimefall/world.cpp`, `rimefall/map.cpp`, `rimefall/rimefall.h`, plus the
documentation. Stage 5, and **wholly inside `rimefall/`** — no engine change.

Both horde sources are placed at world creation and garrisoned from `CheckVictory`. The northern
spawn band creeps south as a function of the turn from its own source's row; a threat score of
time, prosperity and discord decides when it attacks and how hard. The eastern front wakes at a set
depth or the moment the northern source falls. Overrun bands stop offering start slots.

Three things were found by measuring rather than by reading:

- **Prosperity outweighed time eight to one** (747 against 90 over 45 turns), leaving one term
  deciding a score meant to have three. Counted per ten thousand people now, which brings them
  level.
- **The front started at row 0, where there is no land.** Its first twenty-odd turns crossed empty
  polar water doing nothing, and the apparent grace period was geography rather than the time term.
  It creeps from its source's row instead.
- **`O_ICECAVE` has no `CANENTER` flag.** A source nobody can enter can never be taken, and the
  game would have been unwinnable. The north uses `O_ATEMPLE`, whose garrison is the northern
  front's own creatures.

A fourth was found by playing it: re-garrisoning an empty source on the same turn it was cleared
closed the only window in which it could be entered, making both sources permanently untakeable.
The refill now waits until the region is empty of players.

**Not done, deliberately:** the six creatures are renamed (`#46`) but their combat statistics are
untouched. Retuning them without play data would be guessing, and the character difference between
the fronts already lives in the spawn logic rather than in `MonDefs`.

**Fork-local, permanently.**

### `#50` — the region editor frees pointers it keeps

`edit.cpp`, `CHANGELOG`. Two double-frees, found while writing the regression tests for `#48`: the
test helper reached for the same idiom and corrupted the heap with it.

`std::remove_if` leaves the tail **unspecified**, and for a vector of raw pointers that means copies
of pointers still live at the front. Deleting that tail frees objects the container is about to hand
back. Regenerating a region's products keeps the two silver productions and deletes the rest — and
because `SetupProds` runs before `SetupEconomy`, silver sits at the *end* of `products`, so the
compaction moves it forward and the delete takes out the wages entry the region still uses. Flipping
a market between buy and sell has the same shape.

**Reproduced rather than argued.** Editing one desert of a generated `fracas` world and saving
produced a `game.out` whose product list is raw heap bytes — 62 NUL bytes in a text file — and which
segfaults while reading the regions. A gamemaster loses the world by editing one hex.

**No unit test, deliberately.** The editor reads `std::cin` and `UnitTestHelper` has no way to feed
it, which its own header records as future work. The reproduction is a seven-token stdin script and
is in the commit message.

**Upstream-worthy, not offered.** `edit.cpp` is upstream's and nothing here is fork-specific. Per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.

### `#51` — a town owns its markets, the region owns recruiting (0016)

`docs/decisions/0016-…`, `docs/decisions/README.md`. The record settling how a replaced town's
markets are disposed of, written before the fix as `#47` was before `#48`.

**It amends 0015, and by extension the claim in `#48` above that markets cannot reach the same
shape.** That claim rested on `SetupCityMarket` not producing a duplicate within one block, which is
true and was checked. It says nothing about the function being called a second time on a region that
already has a block, which is what `MakeStartingCity` does. The scan offered as evidence covered
`fracas` (which builds no starting city at all), `standard` (which clears its markets) and the two
rulesets where it is rare — not `kingdoms`, where it is common. A clean scan over the wrong four
rulesets reads exactly like a clean scan, and this entry exists partly to say so.

The decision itself is that a replaced town's markets go and the recruiting markets stay, because
the obvious fix — clearing the list — takes recruiting away from every starting city in the game.

**Fork-local, permanently**, like everything under `docs/decisions/`. The fix it governs is
upstream-worthy and registered separately.

### `#52` — a starting city no longer carries two market blocks

`economy.cpp`, `aregion.h`, all seven rulesets' `world.cpp` plus the `unittest` one,
`unittest/starting_city_test.cpp`, `docs/interface/json-report.md`, `CHANGELOG`. The engine half of
[0016](../decisions/0016-a-town-owns-its-markets-the-region-owns-recruiting.md).

`MakeStartingCity` replaces a town, `add_town` appends a market block rather than replacing one, and
the clearing pass sits below the `START_CITIES_EXIST` early return. A region that already had a town
carried two complete blocks, of which only the first was tradeable.

**The fix is one line of intent and one of trap.** Clearing the market list before `add_town` — the
obvious version, and the one first written here — takes the recruiting markets with it, and nothing
on that path rebuilds them: measured on a `kingdoms` 64×64 world, all six start cities came out
unable to buy men or leaders. `remove_town_markets()` keeps `IT_MAN` markets for that reason.

Reachable in `kingdoms`, `havilah`, `neworigins` and `rimefall`. **Not** in `fracas`, whose flag
suggests otherwise: its `SetACNeighbors` call sits behind `if (Globals->NEXUS_EXISTS)`, which is 0,
so it builds no starting city at all.

**Existing worlds are deliberately not repaired**, unlike `#48`. Nothing in a save file records
which markets belonged to which town, so a repair would be a guess, and the stale block is inert.

**Upstream-worthy, not offered.** `MakeStartingCity`, `add_town` and `SetupCityMarket` are all
upstream's and the fix is not conditional on a ruleset. Per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.

### `#53` — the starting-city fix reaches only `kingdoms`

`docs/decisions/0016-…`, `CHANGELOG`. A correction to the entry for `#52` above, and to 0016.

Both name `kingdoms`, `havilah`, `neworigins` and `rimefall` as affected. **Only `kingdoms` is.**
`ARegionList::SetACNeighbors` is per-ruleset like `MakeStartingCity`, and six of the seven copies
wrap their starting-city loop in a third guard on `START_CITIES_EXIST`, laying nexus portals on
terrain types in the `else` branch instead. `kingdoms` has no such guard; `fracas` has none either
but never reaches the function at all.

Measured this time rather than derived: logging every `MakeStartingCity` call, `kingdoms` 64×64
builds six start cities per world, of which 0, 2 and 1 landed on an existing town across three
seeds. `havilah` builds none on three seeds, `neworigins` and `rimefall` none.

**Twice in a row now, a reachability claim in this register has been wrong in the same way** — first
0015's about markets, now 0016's about which rulesets. Both came from reading flags rather than the
call path, and both had a clean measurement standing behind them that could not have found anything.
The one habit worth carrying out of it: when a count comes back zero, measure the step *before* the
one being counted, because a structural zero and a rare zero look identical.

The fix in `#52` is unaffected and stays as it is.

**Fork-local, permanently**, like everything under `docs/decisions/` and this file.

### `#54` — the same seed builds the same world on every processor

`standard`, `basic`, `fracas`, `kingdoms`, `havilah`, `neworigins` and `rimefall` `extra.cpp`,
`runorders.cpp`, `economy.cpp`, `spells.cpp`, `scripts/check-rng-draw-order.py`, `ci.yml`,
`CHANGELOG`. The engine half of
[0017](../decisions/0017-x86-64-is-the-reference-for-draw-order.md).

`GetRegion(rng::get_random(x), rng::get_random(y))` puts two draws in one argument list, where C++
leaves the evaluation order unspecified and GCC picks a different one per architecture. The same
seed therefore built a different world on x86-64 than on aarch64 — measured on three seeded `fracas`
worlds, one of which came out a city on one platform and a village on the other.

**The interesting half is the choice, not the fix.** Both draw orders are equally correct C++, so
what decides is that the servers are x86-64 and every recorded seed reproduces its world under that
order. Pinning x86-64's order — the rightmost draw first, which reads like a slip and is commented
as deliberate at all twelve sites — means nothing moves where games are played, and `aarch64` moves
instead. Verified both ways, including 28 recorded turns replayed identically inside an x86-64
container.

Twelve sites, not the nine first surveyed: `rimefall` did not exist then, and `ARegion::Pillage`
came from the second patch. A **CI job** now refuses the shape, validated in both directions —
twelve findings before, none after — after a first version that reported 24 sites of which 23 were
noise and was rewritten rather than exempted.

**Upstream-worthy, not offered.** Unspecified evaluation order is upstream's bug in upstream's code.
The *choice* of x86-64 as the reference is ours, and upstream may reasonably prefer the other one —
which is a good reason to offer the record alongside the patch if it is ever offered at all. Per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.

### `#55` — kingdoms keeps its lakeless coastline (0018)

`docs/decisions/0018-…`, `docs/decisions/README.md`. **A decision not to diverge**, which is why it
sits here without a patch behind it.

Six of the seven rulesets roll on `Globals->LAKES` in `CleanUpWater` before turning enclosed water
into land; `kingdoms` does not. Its `LAKES` is 20, the highest in the tree, and four generated 64×64
worlds contained no lakes at all against `havilah`'s 11 to 13 at `LAKES = 1`. Both the code and the
value are byte-identical to `upstream/master`.

Left alone deliberately: nothing is dead and nothing is misreported, so it is a discrepancy in the
data rather than misbehaviour, and any repair rewrites every future `kingdoms` world — a game change
rather than maintenance. The record also notes why the obvious repair is wrong: `LAKES` is scaled
differently by the two code paths, so handing `CleanUpWater` the same 20 would make a fifth of all
enclosed water into lakes.

**Fork-local, permanently**, like everything under `docs/decisions/`. **No divergence is created**,
and nothing is offered upstream — a preference about another project's world generation is not a bug
report.

### `#56` — the guard's file half moves to `permissions`

`.claude/settings.json`, `.claude/hooks/upstream-guard.sh`. `#37` protected its own files with a
`PreToolUse` hook matching `Write|Edit|NotebookEdit`. **That branch never ran once.**

Measured on 2026-08-18: in the environment this fork is developed in, `PreToolUse` hooks fire for
`Bash` and for nothing else. An agent's `Edit` of the guard script reached the tool untouched and
failed only on the editor's own string match. Restarting did not change it; splitting the
alternation into three separate matchers did not change it; and a second, independent copy of the
guard registered at user level since 2026-08-14 — with the same branch, written by hand, never
exercised — had been failing silently the whole time. Two correct implementations, neither of
them called.

The file half is carried by `permissions.deny` instead, which does hold. All four paths are now
refused with `denied by your permission settings`: both guard scripts and both `settings.json`.
The hook keeps its branch as a fallback for environments where hooks do fire, and that branch also
gained `notebook_path` — `NotebookEdit` supplies the path under a different key, so the one field
the branch read was empty for that tool even where the hook works.

**The probe belongs in the register, because the failure was silent for four days.** An `Edit`
whose `old_string` cannot occur in the file changes nothing and separates the two outcomes exactly:
`denied by your permission settings` means the guard holds, `String to replace not found` means it
does not. A guard nobody tests is a guard nobody has.

Two scope limits, both deliberate. The deny list names `settings*.json` and `hooks/**` rather than
`.claude/**` at user level, because the agent's own memory directory lives under `~/.claude/` and a
wider rule would sever it. And the user-level copy cannot be registered here at all — it is one
machine's configuration, outside the repository, and it is the reason `#37` put the guard in the
repository in the first place.

**Fork-local, permanently.** For `#37`'s reason: upstream is not a fork and has no parent to
default to.

### `#57` — the region editor's third `remove_if`

`edit.cpp`, `CHANGELOG`. `#50` fixed two sites that deleted the tail `std::remove_if()` hands
back and left explanatory comments on both. **A third site in the same function kept the shape**,
and kept it without the comment — which is how it survived the cleanup that was written to end it.

Deleting a market by item. `remove_if()` moves survivors forward and leaves the tail unspecified;
moving a raw pointer copies it. On `[B(item=Y), A(item=X)]` with `Y` as the target the tail holds
`A`, so the delete frees the market the region keeps and `B`, the one actually removed, leaks.
Written out as a loop like its two neighbours; the preceding `find_if()` goes away because it only
asked what the loop answers anyway.

Found while tracing an unrelated crash, by reading every remaining `remove_if()` in the tree after
`#50` named the shape. The other two survivors — `economy.cpp` and `faction.cpp` — are safe: neither
deletes what it removes.

**Upstream-worthy, not offered**, on `#50`'s reasoning and
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md).

### `#58` — the draw-order check stops scanning worktrees

`scripts/check-rng-draw-order.py`, one line: `.claude` joins `SKIP_DIRS`. Agent sessions keep git
worktrees under `.claude/worktrees/`, each a full copy of the sources at whatever commit it was cut
from, so the checker `#54` added was reading `#54`'s own findings back out of a worktree cut before
the fix and reporting twelve failures against a tree that has none.

**CI never saw it** — a fresh checkout has no worktrees — so the defect appeared only for whoever
ran the script by hand, which is the worst place for it. The value of that check is entirely in
being believed; twelve false alarms spend that faster than any missed draw pair would.

**Fork-local, permanently.** The script is this fork's, added by `#54`.

### `#59` — a failed expectation stops killing the suite

`unittest/testhelper.cpp` and five test files. boost.ut records a failed `expect()` and continues
to the next line, and the next line is usually an index into the container whose size the failed
expectation just asserted. `operator[]`, `.front()` and `.back()` check nothing, so a red run does
not merely report — it reads out of bounds and dies.

32 preconditions become `expect(...) << fatal`, by one rule: an expectation earns it when an
unchecked access to the same container follows immediately. `activate_spell()` gains three null
checks, because it calls the `Run<Spell>` functions directly while their real caller in
`runorders.cpp` reaches them only once it has found an order — `RunTeleport` dereferences
`u->teleportorders` unguarded, and a test whose order text parsed into nothing segfaults inside
the engine.

**`fatal` fires only on a failing expectation, so a green run is untouched** — which is why this
changes nothing in CI and everything on a machine where the suite is red.

Found while chasing a sporadic snapshot crash that turned out not to reproduce. On this fork's
macOS/arm64 workstation the suite exited 139 after a heap-buffer-overflow read past a
`vector<FactionEvent>`; it now exits 1 with ten suites green, ten red and all of them reported.
**Those failures are a separate defect** — the generated world differs on this platform, and `#54`
fixed one cause of that but evidently not every one. A suite that dies at the first failure cannot
be compared against anything, so this is the precondition for that investigation rather than a
part of it.

**Upstream-worthy, not offered.** `unittest/` is upstream's and the fragility is theirs, but per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.

### `#60` — the draw-order job watches its own checker

`.github/workflows/ci.yml`. `#58` changed `scripts/check-rng-draw-order.py` and the `RNG Draw
Order` job was skipped on it: the `code` filter names sources, `Makefile`, `snapshot-tests/` and
`ci.yml`, but never `scripts/`. **The only job that runs the checker fell silent on the one pull
request that altered it.**

A dedicated `scripts` filter, and `draworder` runs on `code` or `scripts`. Kept out of `code`
deliberately — a checker is not the engine, and folding it in would run two full C++ builds to
reach one `python3` invocation.

Noticed while merging `#58`, from its own check list. Worth recording because it is the second
instance in two pull requests of the same failure mode: a check that is quiet precisely where it
matters, whose silence then reads as approval.

**This one could not verify itself** — `ci.yml` is in `code`, so the job ran here through the old
branch of the condition. Both YAML layers were parsed instead, the outer file and the `filters:`
block scalar that `dorny/paths-filter` parses again.

**Fork-local, permanently.** The workflow and the checker are both this fork's.

### `#61` — the engine owns its random distributions (0019)

`rng.hpp`, plus `docs/decisions/0019`, `docs/snapshot-tests.md` and the `CHANGELOG`. `#54` pinned
the draw *order* and did not reach the larger cause: **`std::mt19937` is specified bit for bit, the
distributions are not.** libstdc++ and libc++ map raw draws to values differently and consume the
stream at different rates — ten draws of range 6 cost ten raw draws under one and fifteen under the
other — so the two platforms sit at different points in the stream after the first distribution
call.

`rng.hpp` implements the uniform draw and the shuffle itself, reproducing libstdc++'s numbers, so
no recorded seed and no running game changes. Verified against real libstdc++ in a `linux/amd64`
container under GCC 13.3 and 14.2, values and generator state, over every range to 4096 and every
power of two to 2^30. macOS/arm64 now builds a checksum-identical world and replays all 28 recorded
turns byte for byte; Linux was re-measured in a clean container build and is unchanged.

**This retires the claim in `docs/snapshot-tests.md` that the fixtures are Linux artefacts**, which
is corrected in the same pull request rather than left to rot. Left alone deliberately:
`std::binomial_distribution`, which cannot be frozen the same way because libstdc++'s uses libm —
0019 gives the reasoning and the second reason, that replacing it would change running games.

**Upstream-worthy, not offered.** `rng.hpp` is upstream's and so is the defect, but per
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md) it is prepared and registered.

### `#62` — `rimefall`'s election, rulebook and recorded turns

`rimefall/extra.cpp`, `rimefall/rimefall.h`, `rimefall/rimefall_intro.html`,
`snapshot-tests/rimefall_turns` (new), `snapshot-tests/run-snapshots.sh`,
`snapshot-tests/update-all-game-snapshots.sh`, `.gitignore`, plus the documentation. Stage 6, the
last, and the ruleset is complete.

Once both horde sources are held the throne opens and the crown goes to whoever holds mutual
`ALLY` from at least half the other living factions. Two gates in order — while either source
stands there is no winner at all — and a tie crowns nobody, because breaking it by faction number
would settle the game on registration order.

**The larger part of the change is a removal, and it was not planned.** `rimefall` still carried
NewOrigins' annihilation ending from the stage 1 copy, and carried it *live*: `victory_type` was
set to `"annihilation"` and the altars, monoliths, entity cages, imprisoned entity and the
`ANNIHILATE` skill were all enabled. A second, undesigned way to win — and one that can end a game
while a horde still has a home, which 0011 section 6 rules out explicitly. 107 lines added, 446
removed.

Beyond `rimefall/` this touches the two snapshot scripts and `.gitignore`, which are shared files
upstream also maintains — the same registration surface `#38` had to record, and the reason this
entry exists rather than resting on 0010 section 0.

**The fixtures were recorded on macOS/arm64 and replay byte for byte on `linux/amd64`.** That was
impossible before `#61`, and it is the clearest evidence that fix holds. They were also re-recorded
once: at 64×64 five turns cost 39 MB against 16 MB for `neworigins`' fourteen, so the world is
24×24.

**Fork-local, permanently.** A new game variant is this fork's own.

### `#63` — a market with no item stops reading in front of the item table

`market.h`, `market.cpp`, `economy.cpp`, `CHANGELOG`. The sporadic Bus error in the `neworigins`
snapshot replay, and **not** the use-after-free the bare backtrace suggested: the pointer was
sound, the index was not. AddressSanitizer named it in one run — the region being read is
`ItemDefs` itself, and the read lands 124 bytes *in front of it*.

`lookup_item()` answers -1 for a name the build does not know, and `read_in()` stores that. **A
market with `item == -1` is a supported state**: `write_out()` persists it as `NO_ITEM` on purpose
and `read_in()` restores it, so both ends of the save file have always agreed. Four readers had
not — `post_turn`, `remove_town_markets`, `UpdateEditRegion` and `TownGrowth`. `Market::has_item()`
states the condition once instead of repeating the comparison at each.

**Why it hid for so long:** a read just before a static array almost always lands in other valid
memory. It faults only when that address is unmapped, so the same recorded turn crashed in some
runs and not others — and became reproducible the moment unrelated code shifted the layout, which
is how it was finally caught.

Measured as an A/B on `neworigins` turn 0 with an identical perturbation applied to both builds:
five crashes in six runs before, none in six after; six turns under AddressSanitizer report no
error. All 41 snapshots stay byte-identical, so no running game changes.

**Upstream-worthy, not offered**, on
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md).

### `#64` — a negative seed reaches the generator the same way everywhere

`rng.hpp`, `CHANGELOG`. The third instance of the family `#54` and `#61` belong to, and the reason
ten unit test suites failed on arm64 macOS while passing on x86-64 Linux.

`std::minstd_rand`'s `result_type` is `uint_fast32_t` — **32 bits on arm64, 64 bits on x86-64** —
so a negative `int` arrived truncated on one platform and sign-extended on the other, and the two
streams had nothing in common from the first draw. Seed `0xdeadbeef` becomes `3735928559` on one
and `18446744073150512879` on the other.

**Positive seeds were never affected**, which is exactly why every seeded world and all 41 recorded
snapshots agreed while the unit tests did not: `UnitTestHelper` is the only caller seeding with a
value too large for `int`. The reduction now happens before the engine call, on the sign-extended
64-bit value, reproducing what x86-64 already produced. A negative `ATLANTIS_SEED` is fixed with
it — `std::stoi` rejects anything above `INT_MAX` but accepts negatives.

**No new decision record**, deliberately: [0017](../decisions/0017-x86-64-is-the-reference-for-draw-order.md)
settled which platform is the reference when two behaviours are equally correct, and this applies
that decision rather than making another. The reasoning sits at the code.

**Both platforms are now fully green** — 20 of 20 unit suites and 41 of 41 snapshots on each,
which has not been true of this repository before. Linux was re-measured in a clean container
build and is unchanged.

**Upstream-worthy, not offered**, on
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md).

### `#65` — a leftover `times.*` file no longer changes the turn

`game.cpp`, `docs/interface/file-formats.md`, `CHANGELOG`. The name of a world-events article is
drawn from the game's own generator, and the collision loop drew again — which put **the contents
of the working directory into the turn**.

`GrowWMons` runs later in the same `PostProcessTurn`, so a shifted stream moves the monsters, and
`game.out` saves the generator state, so every following turn drifts with it. A turn replayed in a
directory still holding files from an earlier or crashed run did not reproduce. Measured on turn 0
of the `neworigins` fixtures, with one empty `times.6157` placed beforehand: both article names
moved, and `game.out` and `report.1` with them — a different monster population.

A collision now counts up from the drawn number, which costs no draws. **The stream is untouched**,
so a clean directory writes exactly the file it wrote before and running games are unaffected —
the same promise `#61` and `#64` kept. A generator of its own for the filename would read better
but would remove a draw; sequential `times.001` names would need a discarded draw to keep the
stream, and would change a published filename format. Both were declined as cosmetics.

41 of 41 snapshots identical, unit tests green, and the dirty run's six output files byte-identical
to the clean reference — it now differs by the extra file alone.

The documented warning was corrected with it: `file-formats.md` said leftovers made the retries
"drift" and the files accumulate, which reads as a tidiness note rather than the determinism
condition it was.

**Cleared while investigating:** the `times.<n>` names an AddressSanitizer build once wrote differed
from the `-O2` build's, which looked like a second instance of
[0017](../decisions/0017-x86-64-is-the-reference-for-draw-order.md). It is not. Rebuilt at `-O0` and
at `-O1 -fsanitize=address` on a clean directory, both produce byte-identical output and the same
filenames; the old observation was this bug, fed by leftovers from the very runs ASan was there to
diagnose.

**Upstream-worthy, not offered**, on
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md).

### `#66` — the same seed builds the same `kingdoms` world on every standard library

`rng.hpp`, `aregion.cpp`, `unittest/rng_test.cpp`, `snapshot-tests/`, `docs/`, `CHANGELOG`,
`ci.yml`. The fourth instance of the family `#54`, `#61` and `#64` belong to, and the one that had
been running unnoticed the longest.

`std::discrete_distribution` diverges between libstdc++ and libc++ at **exactly one point: a single
weight.** libstdc++'s `param_type` clears the probabilities below two entries and `operator()` then
returns 0 *without touching the generator*; libc++ draws twice. From two weights up they agreed in
every case measured — which is precisely why
[0019](../decisions/0019-the-engine-owns-its-random-distributions.md) recorded them as agreeing.
**That measurement was taken where the function does not run.**

It runs in one place: `MakeManUnit()`, reached only when `LEADERS_EXIST` is false, which is true of
`kingdoms` alone. Half the weighted picks during a `kingdoms` world creation offer one candidate.
Seed 12345 on a 24×24 world gave `be7627a9…` on macOS and `25581781…` on Linux before, and
`25581781…` on both after — Linux byte for byte unchanged, per
[0017](../decisions/0017-x86-64-is-the-reference-for-draw-order.md). Attribution was measured
rather than assumed: with the same portable replacement compiled on both sides the worlds already
matched, so `kingdoms` world generation holds no second platform-dependent site.

`rng.hpp` now reproduces libstdc++'s algorithm. **Nothing in it calls libm**, which is what
separates it from `std::binomial_distribution` — still called, still 0019's deliberate residue,
because libstdc++'s uses `log` and `exp` and a faithful port could diverge on a last bit and *look*
fixed. Verified against real libstdc++ under GCC 13.3 and 14.2, 1520 cases each, comparing the
chosen index **and** the next raw draw: no divergence. Against libc++ the engine now differs in 160
cases, all at a single weight, which is the point.

**Two guards, because neither path had one.** The turn snapshots replay `run` against an existing
world, so world creation — terrain, towns, starting locations, city guards — was exercised by
nothing at all; that is how a defect this old survived 41 green fixtures.
`snapshot-tests/run-worldgen-snapshot.sh` regenerates a world from a fixed seed and compares it
byte for byte (`kingdoms`: 92 KB, one second), and `unittest/rng_test.cpp` pins the values while
asserting *behaviour* for the case that bit — a one-weight pick must leave the following draw
untouched.

**Also fixed, because it blocked the comparison:** `ResourcesStatistics()` iterated three
`std::unordered_map` directly, so a new world printed the same numbers in a different order on a
different standard library. Sorted by item index. No draw is taken there, so no world changes.

0019 is corrected in place rather than rewritten — the wrong paragraph is struck through and the
reason it was wrong is stated, since the failure mode was a measurement that could not have seen
what it claimed.

**Upstream-worthy, not offered**, on
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md).

### `#67` — a recorded world for every ruleset

`snapshot-tests/worldgen/`, `run-worldgen-snapshot.sh`, `update-worldgen-snapshot.sh`,
`update-all-worldgen-snapshots.sh`, `run-snapshots.sh`, `docs/snapshot-tests.md`, `ci.yml`. No
engine change.

`#66` closed the world creation gap for `kingdoms`, because that is where the defect was. **The
gap was never kingdoms-specific**: the turn suites replay `run` against a world that already
exists, so terrain, towns, starting locations and city guards were watched by nothing in seven
other rulesets. Six more worlds now cover them — seven in all, 24×24 where the ruleset lets you
choose, one seed, **1.1 MB and under a second** to replay. The suite goes from 42 checks to 48.

`neworigins8` is left out for the reason its turn fixtures are: it shares all world generation code
with `neworigins` and produces the same bytes.

**Measured before recording:** all seven worlds are already byte-identical between macOS/arm64 and
linux/amd64, so `#66` has no sibling hiding in another ruleset. Recorded on Linux and replayed on
macOS byte for byte; re-recording `kingdoms` in a fresh session reproduced the bytes `#66` had
already committed, which is the determinism check that makes the rest worth trusting.

**Both runners bound each run at two minutes.** The engine's world size questions sit in loops that
never check for end of input, so an answers file one line short does not fail — it repeats the
question for ever at full speed, which in CI is a job only the six hour limit can stop. The bound
caught precisely that while this change was being written, from a background job in a
non-interactive shell reading `/dev/null` instead of the answers file. `timeout` is absent on
macOS, hence the hand-rolled loop.

**The input loop itself is a defect and is deliberately not fixed here.** Both Python consumers
drive the engine as a subprocess, where a short or closed stdin hangs instead of returning an
error. Fixing it touches six rulesets and changes engine behaviour, which does not belong in a
test recording.

### `#68` — the end of the input is not a reason to ask again

`game.h`, `game.cpp`, `edit.cpp`, every ruleset's `world.cpp`, `unittest/input_test.cpp`,
`docs/interface/cli.md`, `CHANGELOG`. Found while recording the world generation snapshots of
`#67`, where a short answers file would have meant a CI job only the six hour limit could stop.

Every prompt sits in a loop that asks again when it cannot use the answer, and **none looked at the
state of the stream**. Once stdin is closed the read fails immediately and for ever, so the loop
repeats the same question at full speed. `<game> new` with an answers file one line short was a
hung job rather than an error — and the engine is normally driven as a subprocess, which is exactly
where a hang is worst. `<game> edit` reprinted its whole menu each time round.

`read_input_line()` reports the end of input; `CreateWorld()` returns `bool` and gives up
(nineteen prompts across eight rulesets and the test ruleset); the editor quits **without saving**,
leaving the game as it was found rather than committing a decision nobody made. Measured: `new`
with empty or short input now exits 1 writing no `game.out`, `standard new` still succeeds because
it asks nothing, and `standard edit` quits in seventeen lines.

**Consumer-visible**, and called out as such. `docs/interface/cli.md` described the hang as a
property of the tool — *"Run non-interactively it will spin against EOF rather than fail"* — and now
gives the exit code and the message. For the two Python projects a process that never returns
becomes a failure they can see.

The unit tests pin one distinction worth keeping: the parser's `operator>>` begins with
`>> std::ws`, so a blank line is stepped over rather than returned. At a terminal that is what a
person expects, and a stream of nothing but whitespace has genuinely ended. **One of those tests
began by asserting the opposite and failed**, which is how the distinction came to be written down
instead of assumed.

**Upstream-worthy, not offered**, on
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md).

### `#69` — a decision record is accepted once its pull request merges

`docs/decisions/`. Housekeeping, no engine change.

Six records — `0014` through `0019` — still said `proposed` long after their changes had shipped,
the oldest for a week, so the field carried no information at all. They are marked accepted, and
`docs/decisions/README.md` now defines the two values instead of leaving them to be inferred:
`proposed` while the pull request is open, `accepted` once it merges, because under
[0003](../decisions/0003-no-required-reviews.md) merging **is** the acceptance — review happens
before the commit and no separate approval step exists to wait for.

Only the status moved. The reasoning in every record is untouched, which is what the register
demands of itself; the status is the one field about a record's lifecycle rather than its content.

### `#70` — item decay reproduces on every standard library (0020)

`rng.hpp`, `unittest/rng_test.cpp`, `docs/decisions/0020`, `0019`, `CHANGELOG`. The last
unspecified distribution the engine still called, and the fifth and final member of the family
`#54`, `#61`, `#64` and `#66` belong to.

`std::binomial_distribution` sits in `rng::calculate_losses`, the monthly decay of summoned undead.
libstdc++ and libc++ neither return the same values nor consume the stream at the same rate, so a
turn in which a player held undead replayed differently on a Mac.

**[0019](../decisions/0019-the-engine-owns-its-random-distributions.md) gave two reasons for leaving
it alone, and the second was wrong.** It described *replacing* the distribution with n Bernoulli
draws — the same rule stated differently, but consuming different randomness, so running games
would change. Freezing is not replacing: a copy of libstdc++'s algorithm calls the same libm with
the same arguments in the same order on the machine the games run on, so Linux is unchanged **by
construction**. Proposing a game change in the name of not changing games was backwards, and
[0020](../decisions/0020-the-engine-owns-its-binomial-draw.md) says so plainly.

The first reason survives as a limit on what may be promised — the algorithm calls `log`, `exp`,
`sqrt` and `lgamma` — so this one is measured rather than argued:

| | result |
| --- | --- |
| against real `std::binomial_distribution`, GCC 13.3 and 14.2 | 5 984 cases each, **0 divergences** |
| `calculate_losses` at its own call site, on Linux | 11 583 cases, **0 divergences** |
| macOS/arm64 vs linux/amd64, dense sweep | **263 736 draws, identical hash** |

Values and generator state both; the sweep covers every whole percentage 1..99 and item counts
1..200 and 250..5000 by fifties, so both branches of the algorithm many times over.

`normal_source` comes with it: the rejection loop draws from `std::normal_distribution`, whose
polar method produces two values per pair and **keeps the second**. That cache is part of the
sequence rather than an optimisation — a copy recomputing both would consume twice the stream.

0019 is corrected in place rather than rewritten, which is what the register asks of itself: a
decision reversed for a stated reason is worth more than one quietly deleted.

**Upstream-worthy, not offered**, on
[0008](../decisions/0008-prepare-upstream-fixes-do-not-submit.md).

### `#73` — `rimefall` writes its turn as numbers

`rimefall/extra.cpp`, `snapshot-tests/`, `docs/interface/`, `docs/rulesets.md`, `CHANGELOG`.
**No engine change**, which is the point of its shape.

Tuning this ruleset means running several games and reading the result, and everything needed for
that existed only as prose: one line per turn in the engine log, times articles for the events. A
sum cannot say whether the front is driven by time, by prosperity or by players fighting each other
— the first question anyone tuning it asks. `rimefall.json` now carries the front's row, the three
threat terms **separately**, the wave allowed against the wave actually placed on each front,
whether the dragons are awake, both sources' state and, once the ballot opens, the leader, the
electorate and the percentage.

**The ruleset writes it, not the engine, and into a file of its own.** Adding fields to
`report.<n>.json` would touch what the two Python projects read, and an engine hook would need its
own record: [0012](../decisions/0012-a-ruleset-hook-for-gateway-destinations.md) permits exactly
one, for one purpose, and states that it is not a general licence. A ruleset writing a ruleset's
file needs neither.

The write sits in a local object's **destructor**, because `CheckVictory` leaves through six
different returns once the ballot is open and the file must appear on every one of them.

The re-recorded fixtures are the evidence that it costs nothing: across five turns the only change
is the new file. Both snapshot runners move and compare it now, or it would have sat unchecked
beside the fixtures meant to guard it.

**Nothing to offer upstream.** `rimefall` is fork-local in its entirety, and so is this.

### `#74` — CI builds Linux only

`.github/workflows/platforms.yml` renamed to `toolchain-drift.yml`, plus
[0021](../decisions/0021-ci-builds-linux-only.md), `CONTRIBUTING.md`, `docs/build-and-test.md`
and `docs/snapshot-tests.md`. **No engine change.**

The workflow built `make all` on `ubuntu-latest`, `windows-latest` and `macos-latest`, on every
push to `master` and weekly. Actions bills macOS at ×10 and Windows at ×2, and measured from the
API over 2026-08-05 to 2026-08-24 — 69 runs, each job rounded up to the whole minute the way
GitHub bills it — that was **5 586 billed minutes: 3 780 macOS, 1 334 Windows, 472 Linux**. The
entire merge gate over the same period, 186 runs and 1 284 jobs, cost 984. The workflow that
gates nothing was 5.7× the one that gates every pull request, and the account ran out of minutes.

Both removed platforms only ever **compiled**. macOS is covered better without the runner:
development happens on macOS/arm64, where `make all`, the unit tests and the full snapshot suite
run by hand before a pull request exists, and since `#61` and `#70` that machine replays every
recorded turn byte for byte. MSVC was already not an authority on warnings — `/WX` is off and
`4244`, `4267`, `4700` are suppressed — so what Windows caught was code GCC accepts and MSVC
rejects. That class of finding is the accepted cost, and 0021 says so rather than pretending
otherwise.

The surviving Linux job also **drops its `push: [master]` trigger**. Every ruleset is already
built from `CMakeLists.txt` by `Build & Test (cmake)` and from the `Makefile` by `Build (make)`
on every code-touching pull request; a per-push run added only a second opinion on the image
pin, which is what the weekly run is for. With one platform left, `Platforms` was the wrong name
for the one job still being done — building unpinned so a new image's compiler is found before
the pinned gate meets it — hence `Toolchain Drift`. It is not a required status check, so
branch protection is untouched ([0002](../decisions/0002-single-aggregating-status-check.md)).

**Nothing to offer upstream.** Upstream's CI is its own, and its cost is not ours.

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

`#72` — the same again for `ad07b87`, the `Unit::CanStudy` fix from `#71`. Second `upstream/*`
branch, second register-only pull request, second self-naming push.

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
