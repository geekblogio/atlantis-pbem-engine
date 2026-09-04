# 0024 — The start count scales with the land, and a thin band hands its slots on

**Status:** proposed, 2026-09-04. Ruleset-internal, no engine change. Amends the density curve in
[0010](0010-climate-banded-single-continent-ruleset.md) section 7, which is written for one map
size and says nothing about any other. Closes
[#82](https://github.com/geekblogio/atlantis-pbem-engine/issues/82).

## Context

`rimefall_starts_per_band` is 3, 5, 8, 3, 1 — twenty start locations, apportioned so the middle
band is roughly twice as crowded per hex of land as either edge. Those numbers were measured
against a 64×64 world and are the balancing axis of the map.

They were applied to every world, whatever its size, and `ARegionList::SetACNeighbors` placed them
with a sampler that drew fifty candidates at random per slot and kept the one furthest from what
was already placed. Two things came out of that, both reported in #82 and both measured before
anything was changed:

**A band with no candidates lost its slots outright.** `get_starting_region_candidates` requires
wood, and the far south rolls jungle — its only wood-producing terrain — at 4 in 64. Over ten
generated 64×64 worlds, four had no forest or jungle at all in the far south; all ten still placed
twenty slots, because the candidate test reaches two hexes and crosses the band edge. The world
that reported this did not: 48 land hexes, no wood, none in reach. It offered **nineteen** start
locations, one faction fewer than the world was supposed to hold, for a reason no player could see.

**Twenty starts on a small world is a different game.** A 64×64 world holds around 590 land hexes,
so twenty slots is one per thirty. The same twenty on a 24×24 world, which holds about ninety, is
one per four and a half:

| Map | land | starts | land per start | slots adjacent to another |
| --- | --- | --- | --- | --- |
| 64×64 | 589 | 20 | 29 | 0 of 20 |
| 32×32 | 155 | 20 | 8 | — |
| 24×24 | 88 | 20 | 4 | **14 of 20** |

The sampler was blamed for the crowding and is largely innocent: at 64×64 no two starts were within
three hexes of each other. On a 24×24 world a perfect farthest-point placement still leaves six
adjacent pairs, because the candidate pool itself is that tight. The crowding is the quota.

## Decision

Three changes, all in `rimefall/map.cpp` and `rimefall.h`.

**1. The count follows the land, and the curve becomes a shape rather than a count.** One start
per `RIMEFALL_LAND_PER_START` (28) surface land hexes, in both directions: a 24×24 world gets
three, a 64×64 world twenty-one, a 64×96 world thirty-three. A bigger world holding more players at
the same density is a better answer than the same twenty factions with twice the room. The total is
apportioned across the bands by largest remainder, so the quotas add up exactly and the rounding
falls where the curve is thinnest.

**Twenty-eight is a first number and is expected to move.** It puts the reference world at the
density the curve was balanced against, with enough margin that an ocean-heavy 64×64 world does not
quietly lose a start — the ten worlds measured for this record held 594 to 638 land hexes. Whether
the crush zone wants a lower number is a question for a real game's data.

**2. A band that cannot fill its quota hands it to the nearest band that still has candidates.**
The band still offers nothing — what moves is the slot, not the shortage — and the log names both
ends of the move. A world keeps the number of start locations its land earns.

**3. Farthest point, not fifty guesses.** Each slot goes to the candidate whose nearest
already-placed start is furthest away. Only the first slot in the world is drawn at random, because
there is nothing for it to be far from yet and without it every world would begin its layout in the
same corner.

## What this produces

Same seed, after all three:

| Map | land hexes | slots | nearest-neighbour distances | villages founded |
| --- | --- | --- | --- | --- |
| 24×24 | 88 | 3 | 4, 4, 6 | 1 |
| 32×32 | 155 | 6 | 3 to 5 | 2 |
| 48×48 | 326 | 12 | — | — |
| 64×64 | 589 | 21 | 3 to 9 | 2 |
| 64×96 | 931 | 33 | — | — |
| 96×96 | 1348 | 48 | — | — |

The 64×64 world is the one that reported the missing slot: the far south still has no candidates,
its slot goes to the southern reaches, and nothing is lost.

**No start location is adjacent to another at any size measured.** The minimum separation at
64×64 is 3 rather than the 5 a farthest-point sweep could reach on the unrestricted pool; the
difference is [0023](0023-a-rimefall-start-carries-a-settlement.md) taking settled candidates
first, which is worth more than two hexes of spacing.

## What was weighed

**A relaxed candidate test for an empty band**, dropping the wood requirement so every band stays
inhabited. Rejected: `get_starting_region_candidates` is engine code and cannot be parameterised
without a second hook, so the ruleset would have to reimplement it — and a probe written to mirror
it for this investigation already counted 1 candidate where the engine counted 12. A duplicated
test that drifts is worse than a band that occasionally offers nothing.

**Raising the far south's jungle share** from 4 in 64, which would fix the cause rather than the
symptom and help everyone living there, not only newcomers. Rejected *here* rather than rejected
outright: it changes the climate of a band for every world, which is a map-design decision and not
a bug fix, and the redistribution above already keeps the world's start count whole.

**Leaving the quota fixed and documenting the crowding.** Rejected: a 24×24 world is what the
snapshot fixture uses and what a game master reaches for when testing, and its starts were four
hexes of land apart. Nothing about that is a small version of the designed game.

## Consequences

- **A small world offers fewer start locations**, and the southern bands are the first to lose
  theirs, because the curve weights them least: a 24×24 world places three, all in the northern
  two-thirds. That is the curve being honoured, not overridden.
- **A large world offers more, and can therefore hold more factions.** `SetupFaction` still refuses
  a newcomer once every slot is held, so the map size now sets the ceiling on how many players a
  game can take. A game master picking a map size is choosing a player count.
- **The recorded `rimefall` world is re-recorded**, and its slot count drops from twenty to three,
  because it is a 24×24 world. The recorded turns replay from their own `game.in` and are untouched.
- The selection consumes one `rng` draw for the whole world instead of up to fifty per slot, so
  every world generated from a given seed changes. Nothing outside world creation is affected.
- `GAMEMASTER.md` and `docs/rulesets.md` no longer state twenty as a property of anything. They
  give the count as a function of the land, with a measured table beside it, and say plainly that
  choosing a map size is choosing how many players the game can take.
