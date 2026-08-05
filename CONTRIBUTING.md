# Contributing

This repository is a fork of [Atlantis-PBEM/Atlantis](https://github.com/Atlantis-PBEM/Atlantis).
It serves two purposes at once, and most of the rules below exist to keep them from
interfering with each other:

1. It is an **engine supplier**. Two separate projects build the binary from this source and
   drive it as a subprocess. Anything they can observe — the command line, the file names, the
   JSON report shape — is a published interface, not an implementation detail.
2. It is a place to **prepare fixes for upstream**. Fixes that belong to everyone should be
   able to travel back without dragging this fork's local decisions with them.

## Ground rules

- **English everywhere** — documentation, code comments, commit messages, pull request text.
  This holds regardless of the language a discussion happens in, because anything here may end
  up in front of upstream.
- **Never push to `master`.** Every change goes on a branch and through a pull request. Branch
  protection enforces this for everyone, including administrators.
- **Every pull request updates the documentation it invalidates**, in the same pull request.
- **Never re-record a snapshot fixture to make CI green.** Read the diff first and convince
  yourself the change is the one you intended. See [Snapshot tests](#snapshot-tests).

## Branches

| Prefix | Use |
| --- | --- |
| `feat/` | new behaviour |
| `fix/` | bug fix that is specific to this fork |
| `perf/` | performance work |
| `ci/` | workflows, repository configuration |
| `docs/` | documentation only |
| `chore/` | maintenance, dependency bumps |
| `upstream/` | **a change intended for Atlantis-PBEM/Atlantis** — see below |
| `chore/sync-upstream-<date>` | merging upstream into this fork |

Never name a local branch after a branch that exists on the `upstream` remote (`master`,
`stable`, `neworigins-v8-stable`, …). Git would then have to choose between your branch and the
remote-tracking ref of the same name, and reference lookups become ambiguous.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/), with scopes drawn from this
repository: `engine`, `ruleset`, `report`, `snapshots`, `ci`, `docs`, `map_viewer`.

```
fix(engine): stop dereferencing an empty special in combat
```

Write the body for someone who has the diff but not the context: what was wrong, why the fix is
the right one, what you measured or ruled out. One logical change per commit.

## Pull requests

```bash
git switch -c fix/some-thing master
# work, then have the diff reviewed before committing
git push -u origin fix/some-thing
gh pr create --repo geekblogio/atlantis-pbem-engine --base master
gh pr merge --auto --squash --delete-branch
```

`gh` defaults to a fork's parent repository. `gh repo set-default geekblogio/atlantis-pbem-engine`
is already configured locally; pass `--repo` explicitly if you work from a fresh clone, or you
will open a pull request against upstream by accident.

**Squash-merge** fork-local work. **Rebase-merge** `upstream/*` branches, so the individual
commits land on `master` unchanged and stay cherry-pickable (see below).

## Definition of Done

A pull request is ready when all of these are true:

- [ ] The build is warning-clean. `-Werror` is on for GCC and Clang.
- [ ] Unit tests pass.
- [ ] Snapshot tests pass, or the fixtures were re-recorded deliberately and the pull request
      body says what changed and why.
- [ ] Every document the change invalidated is updated in the same pull request.
- [ ] A change visible outside the binary is reflected in `docs/interface/`, and the version
      constants in `game.h` were considered.
- [ ] A divergence from upstream is registered in `docs/fork/patches.md`.
- [ ] A new source file is registered in **both** `Makefile` and `CMakeLists.txt`.

## What CI covers

`CI` is the only required status check. It is an aggregating job: it passes when every job it
depends on either succeeded or was skipped.

| Job | Covers |
| --- | --- |
| `Build & Test (cmake)` | the CMake build, unit tests, and the full snapshot suite |
| `Build (make)` | the Makefile build of `havilah` plus unit tests — the path the downstream Docker image uses |
| `Upstream Hygiene` | fork-local paths on an `upstream/*` branch |

**Windows and macOS are not on the pull request path.** They run in `Platforms` after a merge
and weekly. A portability regression is found within a day, which is cheaper than seven minutes
on every pull request for a product that ships as a Linux binary.

Do not rename or delete the `gate` job in `ci.yml`. Branch protection requires the context `CI`;
without a job producing it, every pull request stays pending forever.

## Running everything locally

```bash
make all                      # six rulesets plus the unit test binary — do NOT use -j
./unittest/unittest
cd snapshot-tests && ./run-snapshots.sh
```

`make` shares one `obj/` directory across all six rulesets and races on `mkdir obj`, so `-j` is
unsafe. The CMake path is parallel-safe:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

The snapshot runners look for `../<game>/<game>` and fall back to `../build/<game>`, so either
build works.

### Snapshot tests

Any change to engine output fails them, including a change to the *number* of random draws in a
phase. That is the point. When one fails:

1. Read the diff. Decide whether it is the change you intended.
2. If it is, re-record: `./update-all-game-snapshots.sh && ./update-all-rule-snapshots.sh`.
3. `git clean -xfd snapshot-tests` — the `turn_*.bak` directories the update script leaves
   behind are invisible to `git status`.
4. Summarise the diff in the pull request body.

Record fixtures on Linux. They are byte-compared against the output of a Linux binary.

## Contributing a fix to upstream

Nothing is sent upstream without an explicit decision. The steps below prepare a fix so that
decision stays cheap later.

1. Branch from `upstream/master`, not from our `master`:
   ```bash
   git fetch upstream
   git switch -c upstream/<topic> upstream/master
   ```
   Branching from our `master` drags fork-local commits into the patch's ancestry.
2. Touch only upstream-friendly paths. `CLAUDE.md`, `CONTRIBUTING.md`, `README.md`,
   `docs/fork/`, `docs/decisions/` and `.github/` are fork-local; the `Upstream Hygiene` job
   fails the build if they appear.
3. Write the commit in upstream's voice: no reference to this fork, its consumers or its CI.
   Strip the `Co-Authored-By` trailer from the commit that is actually submitted.
4. Merge into our `master` with **rebase**, so the commit survives unchanged.
5. Register it in `docs/fork/patches.md` as an upstream candidate.
6. When it is time to submit: branch from `upstream/master` in a personal fork of
   Atlantis-PBEM/Atlantis, cherry-pick, push there, and open the pull request from that fork.
   Never open it from a branch of this repository — that history contains fork-local commits.
