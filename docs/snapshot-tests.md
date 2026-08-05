# Snapshot tests

> **Audience:** anyone whose change touched engine behaviour.
> **Provenance:** upstream-friendly.

**Read this when** a snapshot test failed, or your change intentionally alters engine output.

## What they are

`snapshot-tests/` replays recorded turns through a freshly built engine and compares **every
byte of every output file** against what was recorded.

| Suite | Covers |
| --- | --- |
| `run-game-snapshots.sh [game]` | replays `turns/` (`standard`) or `<game>_turns/`; turn data exists for `standard` and `neworigins` only, 14 turns each |
| `run-rules-snapshot.sh [game]` | regenerates the HTML rulebook and diffs it against `rules/<game>.html`, for all six rulesets |
| `run-snapshots.sh` | both, for everything — 34 individual checks |

The comparison covers `game.*`, `players.*`, `orders.*`, `template.*`, `report.*`, `times.*`
and the engine's own stdout. The rules comparison strips the `Last Change:` timestamp line
first; nothing else is normalised.

## Running them

```bash
cd snapshot-tests && ./run-snapshots.sh
```

The runners look for `../<game>/<game>` and fall back to `../build/<game>`, so either build
system works. Build all six rulesets first — the four without turn data are still needed for
the rulebook comparison, and a missing binary is reported as a **failure**, not a skip.

`run-snapshots.sh` returns a non-zero exit code when anything failed. Do not pipe it straight
into `tail` or `grep` and read *that* exit code — you will see the filter's status, not the
suite's.

## When one fails

**Read the diff before doing anything else.** The suite is not flaky; it is exact. A failure
means engine output changed, and there are only three possibilities:

1. **Intended.** Your change was supposed to alter output. Re-record, and say what moved in the
   pull request body.
2. **Unintended.** Your change altered output you did not mean to touch. This is the case the
   suite exists for.
3. **A different number of random draws.** The most confusing case: the logic looks unrelated,
   but a phase now consumes one more or fewer values from the RNG, and everything downstream
   shifts. Look for an added or removed call into `rng.hpp` on any path, including one that
   rarely executes.

**Never re-record to make CI green.** The fixtures are the only automated statement about what
this engine does; overwriting them without reading the diff discards it silently.

## Re-recording

```bash
cd snapshot-tests
./update-all-game-snapshots.sh
./update-all-rule-snapshots.sh
git clean -xfd snapshot-tests
```

The `git clean` is not optional. `update-game-snapshots.sh` renames each old turn directory to
`turn_N.bak`, and those are ignored — so `git status` will not mention them and a plain
`git clean -n` will not list them either.

Record on **Linux**. The fixtures are byte-compared against the output of a Linux binary, and
the Windows C runtime writes CRLF in text mode, which would make every file differ.
`.gitattributes` forces LF for the fixture trees so a mistake here is at least caught rather
than committed.

## Adding a turn

`snapshot-tests/turns/new-turn.sh` creates the next turn directory. Check with
`git status` that the new files are actually staged: the repository's `.gitignore` matches
`game.*`, `players.*`, `report.*` and friends for good reasons elsewhere in the tree, and the
fixture directories are re-included by explicit negations. If you add a fixture tree under a
new path, extend those negations too, or the commit will be silently incomplete and the replay
will fail with `turn N missing`.

## Why Linux only in CI

The runners are bash and use `[[ ]]`, `shopt -s nullglob`, `&>`, `seq`, `diff -ur` and `chmod`.
More fundamentally, the fixtures encode the output of a Linux build. Running them on another
platform tests the platform, not the engine. The `Platforms` workflow therefore compiles on
Windows and macOS but does not replay snapshots.
