# 0006 — NewOrigins 8 as its own ruleset, sharing name and version

**Status:** accepted, 2026-08-05.

## Context

The live game runs NewOrigins 8. Its only published rule change against 7 is that meals cost 50
silver instead of 30 — one value in `rules.cpp`. Until now that was a local patch to
`neworigins/rules.cpp`, which conflicts with every upstream sync.

## Decision

A separate ruleset `neworigins8/`. `rules.cpp` is a copy with the one value changed; the other
four sources are one-line `#include "../neworigins/<file>"` shims. `RULESET_NAME` and
`RULESET_VERSION` stay **identical** to `neworigins`.

## Why a separate ruleset

Upstream's `neworigins` stays byte-identical, its 28 fixtures are untouched, and every sync
stays conflict-free. Only the file that actually differs is duplicated; `map.cpp` (96 KB) and
`extra.cpp` (63 KB) follow upstream automatically through the shims.

## Why the same name and version

`Game::OpenGame` refuses a `game.in` whose stored ruleset name does not equal
`Globals->RULESET_NAME`, and a *higher* `RULESET_VERSION` triggers the `upgrade_*_version` path.
Renaming or bumping either would mean the binary could not open the very game files it exists to
process.

## Consequences

- **Nothing in a report or a game file distinguishes the two.** The rulebook is the only visible
  difference; the generated baselines differ in exactly four lines.
- The two binaries are interchangeable on the same game — a feature for migration, a hazard for
  operations. Which binary ran a turn has to be tracked outside the game files.
- An upstream change to `neworigins` silently changes `neworigins8`. That is the point of the
  shims, and a thing to check on every sync.
- Consumers that want v8 now build `neworigins8` instead of patching `neworigins`.
