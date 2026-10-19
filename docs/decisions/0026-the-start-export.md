# 0026 — The start export: where a gateway lands, from the arrival's own code

**Status:** proposed, 2026-10-18. An engine change: a new `map` export. The turn does not change.

## Context

`geekblogio/atlantis-pbem-web` is building a catalogue of generated worlds: pick a ruleset and its
options, generate one world or many from random seeds, and look up what each holds. The figure it
is built around is **how many factions can enter through a gateway before two of them share a
starting hex** — the first thing a game master needs to know about a seed, and the thing nothing
in the engine's output says.

The answer is not in `game.in` or the world report as such. A faction created by `Faction: new`
starts in the Nexus; the starting hex is chosen on its first `MOVE <gateway> IN`, from a candidate
list (`get_starting_region_candidates`, then the ruleset's `filter_gateway_destinations`) by a
five-level cascade over who already stands where. Rebuilding that on the web side would be a
second copy of the rule, silently wrong the day either changes; the web project's own rule is
that no game logic lives there.

## Decision

**`map starts <file>` writes, per Nexus gateway, the candidate list the arrival would choose from
and the number of arrivals that land alone** ([../interface/file-formats.md](../interface/file-formats.md#map-starts)).

- **The arrival is split, not copied.** `Game::Do1MoveOrder`'s gateway branch becomes three
  functions both it and the export call: `gateway_destinations` (candidates plus the ruleset's
  filter, `unit` optional), `gateway_occupancy` and `gateway_landing` (the cascade, returning its
  level). The random pick among the landing level stays in `Do1MoveOrder`, so the draws are
  unchanged — every recorded turn replays identically.
- **The capacity plays arrivals forward rather than stating a formula.** `gateway_start_capacity`
  runs the same cascade with a set of claimed hexes standing in for factions that have arrived,
  and counts until an arrival lands at a level that shares (2 or 4). Any hex of a level is as good
  as another for the count, so the random pick does not matter to it.
- **Rimefall needs nothing of its own.** Its filter hands back the slot a gateway names; the
  claimed overlay is not a unit, so the second arrival finds the same slot taken and the cascade
  shares — capacity 1 per gateway, which is what one slot per gateway means. That substitutes
  arrivals elsewhere in a real turn is a property of the whole pool, which the consumer adds up.
- **Recorded in the worldgen snapshots.** `run-worldgen-snapshot.sh` exports the generated world
  and diffs `starts.json` with the rest; the recorded worlds' other files did not move.

## Consequences

- One rule, one implementation: a change to the candidate test, the cascade or a ruleset's filter
  changes the export with it, and the worldgen snapshot shows the difference.
- The export describes `game.in` as it is. Read right after `new` it answers "how many can
  start"; read later in a game it answers "how many more".
- `neworigins8` lives on the `neworigins-stable` branch with its own arrival code (a scan of
  similar terrain, no filter hook); it gets the same export there, split from that code the same
  way.
