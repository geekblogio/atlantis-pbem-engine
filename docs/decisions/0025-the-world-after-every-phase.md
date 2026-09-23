# 0025 — The world after every phase, for the Python port

**Status:** proposed, 2026-09-23. An engine change, opt-in, in the shape
[0005](0005-environment-variables-for-fork-hooks.md) set for the fork's switches. The turn itself
does not change.

## Context

`geekblogio/atlantis-pbem-ng` ports this engine to Python and pins this repository as a submodule,
the behavioural oracle it is checked against. It compares the two engines by the state they save:
both write `game.out`, one converter turns each into the same named JSON, and the harness diffs
them.

That works while the port only loads, saves and reports. The next stage ports the turn, the
thirty-odd phases of `Game::RunOrders`, and a turn compared by its `game.out` alone can only say
*that* the two worlds differ at the end, not *where* they started to. A unit with one silver too
many after the turn could have it from TAX, SELL, GIVE, a month order or maintenance.

The port's roadmap planned for this from the start: a state dump after every phase, emitted by the
reference, so that a deviation shows up after the phase that causes it.

## Decision

**With `ATLANTIS_PHASE_DUMPS` set, `run` writes the world state 32 times: once after the orders
are read, and once after each phase of `RunOrders`.** Each file is `phase.<nn>-<name>.out` in the
working directory, in the `game.out` format;
[../interface/file-formats.md](../interface/file-formats.md#phasenn-nameout) lists the 32.

- **`SaveGame` is split, not copied.** Its body becomes `Game::write_game(std::ostream&, int seed)`;
  `SaveGame` opens `game.out`, draws the seed and calls it, `dump_phase` opens the phase file and
  calls it with `0`. One writer, so the phase files cannot drift from `game.out`.
- **A phase file draws nothing.** `SaveGame` draws the next turn's seed from the RNG. A draw per
  phase file would shift every draw after it, and the turn recorded would no longer be the turn
  played. The seed line of a phase file is therefore a placeholder.
- **The phases are the `logger::write` lines of `RunOrders`**, each with the calls that follow it
  up to the next one; the three calls after movement without a line of their own belong to
  movement. That makes 31 phases and 32 files with `00-orders`.
- **The set of files is fixed.** A phase a ruleset skips (`WITHDRAW`, `SACRIFICE`, transport,
  annihilation, migration) still writes its file, so a consumer never has to know the ruleset's
  switches to know which files exist.
- **Opt-in and inert when unset**, read in `main.cpp`, listed in `usage()`, as 0005 decided for
  every switch of this kind.

## Why not the alternatives

- **Draw the seed, then restore the generator.** The file would carry a real seed, but only
  through a save-and-restore of `rng.hpp`'s internal state that nothing else needs, and the seed
  of a phase file means nothing anyway: nothing reads a phase file back as a game.
- **A dedicated JSON dump.** It could carry what `game.out` does not (order slots, the battles,
  events), but it would be a second serialisation of the world to keep in step with the first.
  The port already has a converter for the `game.out` format; the gap is recorded below instead.
- **Coarser phases**, one per group of the port's roadmap. Fewer files, but a deviation would
  still need narrowing down inside the group, which is the problem this solves.
- **A probe in the port's own repository** that replays `RunOrders` call by call. It would copy
  the phase order out of this repository, and drift from it the first time upstream moves a
  phase. The calls belong beside the phases.

## Consequences

- **The recorded turns stay byte-identical, and that is tested.**
  `snapshot-tests/run-phase-dump-snapshot.sh` replays the `standard` turns with the variable set
  and requires every file but the phase files, and every stdout line but the one announcing the
  variable, to match the recording. A unit test checks that `write_game` draws nothing.
- **A new or moved phase needs a `dump_phase` call**, and the table in `file-formats.md` with it.
  The numbers are the consumer's contract; a phase inserted upstream renumbers every file after it.
- **What `game.out` does not hold, the phase files do not show.** Order slots, battle reports,
  events and errors live only until the reports are written. A deviation there shows up in the
  reports at the end of the turn, not in a phase file.
- **About 32 times the size of `game.out` per turn** on disk, 178 KB each for a recorded
  `standard` turn. Only a consumer that sets the variable pays it.
- **A third consumer.** The port reads these files and nothing else of the fork's switches; it is
  added to [../fork/downstream-consumers.md](../fork/downstream-consumers.md).
- **Fork-local.** Upstream has no Python port to serve, and per
  [0008](0008-prepare-upstream-fixes-do-not-submit.md) nothing is offered without a separate
  decision anyway.
