# 0013 — The gateway hook sets the candidate list, it does not only narrow it

**Status:** accepted, 2026-08-15. **Corrects the wording of
[0012](0012-a-ruleset-hook-for-gateway-destinations.md)**, which described the hook as narrowing.
0012 stays accepted; the decision it made is unchanged and this only makes its contract exact.

## Context

0012 permitted one engine hook, `Game::filter_gateway_destinations`, and described it as a filter:
the engine builds the candidate destinations, the ruleset *narrows* them, the occupancy cascade
picks from what is left. That was written before the ruleset side existed.

Building it showed narrowing is not quite enough. `rimefall` ties one gateway to one start slot, so
in the ordinary case its answer is a single element drawn from the list the engine offered — a
narrowing, exactly as 0012 said. But the engine's list comes from
`ARegionArray::get_starting_region_candidates`, which re-tests candidacy **live**: a hex qualifies
only while the resource cluster is still within two hexes of it and its landmass is still at least
ten hexes.

Those inputs move. Production changes with population and development over a long game. A slot
curated at world creation can therefore stop being something the engine would list, years later,
without anything having happened to it that a player would recognise. Under a strict narrowing that
gateway would go quietly dead: the list would come back empty, the cascade would fall through to
the nominal region, and `movement_forbidden_by_ruleset` would refuse it — a start location lost with
no event and no explanation.

## Decision

**The hook's contract is that the ruleset determines the candidate set.** It may narrow the list,
replace it, or leave it alone. `rimefall` sets it to its own slot when that slot is free and to
nothing when it is taken.

An empty result stays meaningful and stays the ruleset's business: the engine falls back to the
gateway's nominal region, and `ARegion::movement_forbidden_by_ruleset` turns that into a refusal a
player can read. The two are designed as a pair.

Nothing else about 0012 changes. It is still **one hook for one purpose**, still defined empty by
every ruleset but `rimefall`, still verified to leave every other ruleset's candidate list, cascade
and `rng` draw sequence untouched. The boundary it drew — a second engine change needs its own
record — stands exactly as written.

## Why not keep the stricter contract

Two alternatives were considered.

**Narrow strictly, and accept the drift.** Honest to 0012 as written, and simpler to reason about.
Rejected because the failure is silent and delayed: a start location disappears with no event, in a
game that may be years old, and the symptom — one gateway refusing everyone — looks like a bug
rather than a rule.

**Narrow strictly, and re-curate slots when they lapse.** Keeps the contract and repairs the drift
by moving the slot. Rejected as worse than the disease: it makes start locations wander during a
live game, and a "Gateway to the middle lands, plain" could name a different hex from one turn to
the next with nothing telling anyone.

## Consequences

- The engine-side comment in `game.h` says *narrow or replace*, and means it. A future ruleset may
  legitimately hand back a set the engine never proposed, so nothing downstream of the hook may
  assume the list is a subset of what went in.
- The responsibility for producing a *sensible* destination moves fully to the ruleset. The engine
  no longer guarantees the arrival hex is one it would have called a valid start.
- **Still open, and not decided here: a refused faction aborts the whole turn.** When
  `SetupFaction` returns 0, `ReadPlayers` sets `return_code = false` and the run ends with
  `Couldn't run the game!` and a non-zero exit, writing no `game.out` and no reports. 0010 section
  9 described that path only as *discard and log*, which understates it considerably. It is engine
  behaviour rather than anything `rimefall` introduced — `neworigins` reaches it at its own end
  game — but `rimefall` is the ruleset where a full world is an ordinary state rather than a final
  one, and the two Python projects that drive this binary would read that exit as a failed turn.
  Changing it would be a second engine change and needs its own record. Documented as it stands in
  [GAMEMASTER.md](../../GAMEMASTER.md) section 3.3.
