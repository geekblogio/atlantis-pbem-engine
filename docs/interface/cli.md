# Command line

> **Audience:** anyone invoking the engine or changing how it is invoked.
> **Provenance:** upstream-friendly, except the environment variables marked fork-local.

**Read this when** you are calling the engine from another program, or you are about to change
a subcommand, an exit code or an environment variable.

## The working-directory model

There is **no configuration file and no path argument for game data**. Every subcommand reads
and writes fixed file names relative to the process's current working directory:

```bash
cd /games/havilah-42 && /usr/local/bin/havilah run
```

Two consequences the caller owns:

- **One directory per game.** Two engines run concurrently in the same directory will overwrite
  each other's output. Nothing detects this.
- **The caller stages the inputs and harvests the outputs.** The engine never cleans up and
  never checks whether it is about to overwrite something.

The only paths passed as arguments are for `map`, `check` and `genrules`, and those are
ordinary relative or absolute paths.

## Subcommands

Each ruleset compiles to its own executable — `standard`, `basic`, `fracas`, `havilah`,
`kingdoms`, `neworigins`, `neworigins8`, `rimefall`. The subcommand set is identical for all of
them; the ruleset determines the game rules, not the interface.

| Invocation | Reads | Writes |
| --- | --- | --- |
| `<game> new` | *(interactive, see below)* | `game.out`, `players.out`, and for `havilah` also `names.out` |
| `<game> run` | `game.in`, `players.in`, `orders.<n>` | `game.out`, `players.out`, `report.<n>`, `report.<n>.json`, `template.<n>`, `times.<random>` |
| `<game> edit` | `game.in` | `game.out` *(interactive)* |
| `<game> map <geo\|wmon\|lair\|gate\|hex> <mapfile>` | `game.in` | `<mapfile>` |
| `<game> mapunits` | `game.in` | stdout |
| `<game> check <orderfile> <checkfile>` | `<orderfile>` | `<checkfile>` |
| `<game> genrules <introfile> <cssfile> <rules-outputfile>` | `<introfile>` | `<rules-outputfile>` |

Which report formats are written depends on the ruleset's `REPORT_FORMAT`; see
[file-formats.md](file-formats.md).

### `new` and `edit` are interactive

**`new` prompts on stdin** — nexus size (if the ruleset has a multi-hex nexus), map width, map
height — and loops forever on invalid input. Run non-interactively it will spin against EOF
rather than fail. Feed it:

```bash
printf '2\n8\n8\n' | havilah new
```

The accepted values are ruleset-specific: width and height must be multiples of 8, and a
multi-hex nexus width must be a multiple of 2.

`edit` is a full interactive menu and is not usable from a script at all.

**`run` is not interactive** and is the only subcommand a turn processor needs.

### The turn cycle

`new` and `run` write `game.out`/`players.out`; `run` reads `game.in`/`players.in`. The caller
renames between turns:

```bash
mv game.out game.in
mv players.out players.in
```

Nothing in the engine does this. Forgetting it makes `run` fail with
`Couldn't open the game file!`.

## Exit codes

Only two values are produced:

| Code | Meaning |
| --- | --- |
| `0` | success — **or** the binary was invoked with no arguments at all, in which case it prints usage and exits 0 |
| `1` | anything else: unknown subcommand, wrong argument count, unreadable game file, a failed run |

**There is no distinct code per failure mode.** A caller that needs to tell "no such game file"
from "the turn crashed" has to read stdout. Treat any non-zero as fatal and capture the output.

## stdout

Progress narration, one line per phase, plus a final `done`. It always begins with two version
lines:

```
Atlantis Engine Version: 5.2.5 (beta)
NewOrigins, Version: 3.0.0 (beta)
```

**stdout is part of the tested contract**: the snapshot suite captures it into
`engine-output.txt` and compares it byte for byte, so any new log line moves the fixtures. That
is deliberate — it means a stray debug print cannot reach a release unnoticed.

Errors go to stdout as well, not stderr.

## Environment variables

All four are **opt-in**: unset, the engine behaves exactly as it did before they existed, and
logs nothing about them.

| Variable | Effect | Provenance |
| --- | --- | --- |
| `ATLANTIS_SEED=<integer>` | fixes the world-generation seed used by `new`, making a world reproducible. No effect on `run`, which restores the seed from `game.in`. A non-integer value is rejected with a message and the run continues. | upstream candidate |
| `ATLANTIS_SIM_MODE` | narrows `REPORT_FORMAT` to JSON: no text report, no order template. **Ignored, with a message, on a ruleset that does not enable the JSON report** — all seven do today, so the guard is a safety net for a future one that does not. | fork-local |
| `ATLANTIS_NO_GM_REPORT` | clears `GM_REPORT`, so the world-wide report for the NPC faction is not built. Worth roughly 70% of a turn's wall time; the share grows with the map. | fork-local |
| `ATLANTIS_FORCE_GM_REPORT` | sets `GM_REPORT`, so the world-wide report is built by a ruleset that ships with it off. Affects `basic` alone today; the other six already have it on and the variable is a no-op there. | fork-local |

`ATLANTIS_SIM_MODE` and `ATLANTIS_NO_GM_REPORT` are deliberately independent: a recorded
simulation still wants the GM report as ground truth, only throwaway runs do not.

Setting both `ATLANTIS_NO_GM_REPORT` and `ATLANTIS_FORCE_GM_REPORT` is **not an error**. Force
is read second and wins, because switching a report on is the more specific request and because
the alternative — refusing to start over two variables the caller may not both control — is
worse in the environment these are used in.

### What `ATLANTIS_FORCE_GM_REPORT` is for

`Faction::gets_gm_report` is `is_npc && num == 1 && (GM_REPORT || (month == 0 && year == 1))`.
A ruleset with `GM_REPORT = 0` therefore writes `report.1.json` on its **first turn only**, via
the year-1 fallback, and never again — so a game played under it has no world-wide record past
turn one. `basic` is the one shipped ruleset in that state.

Nothing in the simulation depends on the report being absent: it is written after the turn is
resolved and it draws no random numbers, which is what the `#30` re-record demonstrated. The
cost of forcing it on is wall time, the same 70 % the `NO` variant saves.

## Things that are *not* configurable

Worth knowing before searching for a flag that does not exist:

- the input and output file names
- the number of turns per invocation (always exactly one)
- the ruleset — it is compiled in, one binary per ruleset
- log verbosity
- where reports are written
