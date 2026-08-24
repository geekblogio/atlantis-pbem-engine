# Snapshot tests

> **Audience:** anyone whose change touched engine behaviour.
> **Provenance:** upstream-friendly.

**Read this when** a snapshot test failed, or your change intentionally alters engine output.

## What they are

`snapshot-tests/` replays recorded turns through a freshly built engine and compares **every
byte of every output file** against what was recorded.

| Suite | Covers |
| --- | --- |
| `run-game-snapshots.sh [game]` | replays `turns/` (`standard`) or `<game>_turns/`; turn data exists for `standard`, `neworigins` (14 turns each) and `rimefall` (5) |
| `run-rules-snapshot.sh [game]` | regenerates the HTML rulebook and diffs it against `rules/<game>.html`, for every ruleset |
| `run-worldgen-snapshot.sh <game>` | regenerates a world from a fixed seed and diffs it against `worldgen/<game>/output`; recorded for every ruleset except `neworigins8` |
| `run-snapshots.sh` | all three, for everything |

The comparison covers `game.*`, `players.*`, `orders.*`, `template.*`, `report.*`, `times.*`,
`rimefall.json` where that ruleset writes one, and the engine's own stdout. The rules comparison strips the `Last Change:` timestamp line
first; nothing else is normalised.

## Running them

```bash
cd snapshot-tests && ./run-snapshots.sh
```

The runners look for `../<game>/<game>` and fall back to `../build/<game>`, so either build
system works. Build every ruleset first — the ones without turn data are still needed for
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

## World generation

The turn suites replay `<game> run` against a world that already exists, so terrain, towns,
starting locations and city guards used to be exercised by nothing at all. That gap was not
theoretical: `#66` fixed a defect that only ever showed there, and only in `kingdoms`.

`run-worldgen-snapshot.sh` closes it. `worldgen/<game>/` holds three things:

| | |
| --- | --- |
| `answers` | what that ruleset's `new` asks for on stdin, one answer per line — empty for `standard`, which asks nothing |
| `seed` | the `ATLANTIS_SEED` to build with |
| `output/` | everything the run wrote: `game.out`, `players.out`, `names.out` where the ruleset writes one, and the engine's stdout |

Seven worlds, all 24×24 where the ruleset lets you choose, **1.1 MB in total and under a second
to replay all of them**. `neworigins8` is deliberately absent: it shares all world generation code
with `neworigins` and produces the same bytes.

**Adding a ruleset** means creating `worldgen/<game>/{answers,seed}` and running
`./update-worldgen-snapshot.sh <game>`. Get the answers by running `<game> new` by hand once and
writing down what it asks — and get them *complete*: the engine's input loops never check for end
of input, so a file one line short leaves it asking the same question for ever. The runner bounds
each run at two minutes for that reason and says so when it trips.

## Re-recording

```bash
cd snapshot-tests
./update-all-game-snapshots.sh
./update-all-rule-snapshots.sh
./update-all-worldgen-snapshots.sh
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
The `Platforms` workflow therefore compiles on Windows and macOS but does not replay snapshots.

**This page used to give a second and stronger reason: that the fixtures encode the output of a
Linux build, so replaying them elsewhere tested the platform rather than the engine. That is no
longer true.** It was true, and the cause was the standard library rather than the engine — the
distributions in `<random>` are implementation-defined, and libstdc++ and libc++ consumed the
generator stream at different rates. Since [0019](decisions/0019-the-engine-owns-its-random-distributions.md)
the engine implements those itself, and a macOS/arm64 build replays all 28 recorded turns byte for
byte, exactly as an x86-64 Linux build does.

So a failing replay on another platform is now a real finding, not an artefact. The reason CI
stays on Linux is the shell scripting above and the fact that the product ships as a Linux binary,
not a limit on where the fixtures mean anything.
