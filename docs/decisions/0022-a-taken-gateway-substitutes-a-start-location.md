# 0022 — A taken gateway substitutes a start location, it does not refuse the move

**Status:** accepted, 2026-09-03. Extends
[0012](0012-a-ruleset-hook-for-gateway-destinations.md) and
[0013](0013-the-gateway-hook-sets-the-candidate-list.md): the hook they permitted keeps its single
purpose and gains one parameter. Closes
[#78](https://github.com/geekblogio/atlantis-pbem-engine/issues/78).

## Context

`rimefall` ties one gateway to one start slot. `Game::filter_gateway_destinations` answered with
that slot when it was free and with **nothing** when it was taken, and 0013 recorded the empty
answer as meaning "this band is full". `ARegion::movement_forbidden_by_ruleset` then turned the
engine's fallback to the nominal region into a refusal a player could read. The two were designed
as a pair and worked exactly as designed.

An eight-faction game on turn 2 showed what the design costs. Two factions ordered `MOVE 10 IN`,
the same gateway, in the same month. The first went through. The second read

```
MOVE: That start location is already held prevents movement in that direction.
```

and finished the turn standing in the Nexus with its month spent. Its first turn in the game
produced nothing.

**Nobody could have avoided that collision.** The gateways are offered as a menu, every faction
chooses before any of them has run, and none of them can see what the others chose. There is no
information and no skill in the outcome: the loser is whoever `Game::RunOrders` happened to process
second. When the game is hosted over the web, which is what this engine is a supplier for, that is
the ordinary case rather than a corner one.

The code argued against substitution, and the argument was real:

> The first cut of this pooled every free slot in the band instead, and it read badly in testing: a
> faction entering "Gateway to the middle lands, desert" was put on a plain, because the pool had
> other slots in it. Naming a place and delivering a different one is worse than offering less
> choice.

That holds while a player picks from a list and is the only one moving. It does not survive
simultaneous, blind choice, where the alternative to a different place is no place at all.

## Decision

**When the slot a gateway names can no longer take the unit, the ruleset substitutes the closest
one that can.** The order is narrowest circle first:

1. a free slot in the same band **with the same terrain**;
2. a free slot in the same band;
3. a free slot with the same terrain, anywhere;
4. any free slot.

An overrun slot — `Lost gateway to …` — is not offered at any level, and is itself substituted for
rather than refused, because the rename happens once a turn in `CheckVictory` and a player may well
have written the order while the gateway still read as open.

The objection above is answered rather than overruled. Level 1 delivers the same *kind* of place
under the same name, differing in nothing a player named. Levels 2 to 4 are worse than that and
better than the alternative they replace, which is no start at all.

**An empty answer keeps its meaning as the ruleset's business, and that meaning narrows.** It no
longer says "this band is full"; it says no start location anywhere in the world can take this
unit. `movement_forbidden_by_ruleset` says so in those words.

**The hook gains a `Unit *`, and nothing else.** The engine chooses the arrival hex from the list
the ruleset returns, so the ruleset is the only party that knows a substitution happened, and
without the unit it has nowhere to say it. A player who is set down somewhere they did not name
must be told why, or the report reads as a bug. Every ruleset but `rimefall` still defines the hook
empty and ignores every parameter.

The message names the **band and not the hex**, because the hex is not chosen yet when the message
is written: the occupancy cascade in `Game::DoAMoveOrder` picks from the candidates afterwards.

## What is not decided here

**The bias towards the lower faction number stays.** Whoever the engine processes first still gets
the place they named. Removing it would mean collecting every gateway move of the turn before
resolving any of them, and drawing among the factions that collided — a second engine change of a
different shape, with its own effect on the `rng` draw sequence. The fix here reduces the cost of
losing that race from a lost month to a different hex.

**Reserving the slot at parse time was the alternative**, and would prevent the collision instead
of surviving it: the second faction would be told while writing orders that the gateway is gone.
`Game::ParseOrders` has no occupancy state and runs before any movement, so it would need a
reservation table that does not exist and would have to be discarded and rebuilt every turn.
Rejected as more machinery for a strictly smaller improvement — it converts a bad outcome into an
error message, where substitution converts it into a game.

## Consequences

- `game.h`, `monthorders.cpp` and the empty definition in all seven other rulesets carry one more
  parameter. No other engine behaviour changes, and the cascade and `rng` draw sequence are
  untouched for every ruleset that leaves the candidate list alone.
- A collision now costs a `rng` draw where it previously cost none, because the candidate list is
  no longer empty. Only games where a collision actually happens are affected; recorded snapshots
  taken before this change replay identically.
- `rimefall_free_slot_count` counts what can actually take a newcomer. It previously counted
  overrun slots as free, so a faction could be admitted into a world with nothing but lost ground
  left and be stranded in the Nexus — the exact failure
  [0011](0011-rimefall-invasion-triggers-and-victory.md) section 6 asked to be deliberate. The
  predicate is now shared with the substitution and the refusal, so the three cannot disagree.
- The player-facing promise changes and is restated in `rimefall_intro.html`: a gateway still names
  one location, and no longer promises to be the only way anyone reaches it.
