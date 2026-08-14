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
