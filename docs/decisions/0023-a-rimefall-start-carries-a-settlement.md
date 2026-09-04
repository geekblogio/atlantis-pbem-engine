# 0023 — A `rimefall` start carries a settlement

**Status:** proposed, 2026-09-04. Ruleset-internal: no engine change, and therefore nothing to
argue against the boundary [0012](0012-a-ruleset-hook-for-gateway-destinations.md) drew. Closes
[#80](https://github.com/geekblogio/atlantis-pbem-engine/issues/80).

## Context

`rimefall` picks its start locations at world creation, in `ARegionList::SetACNeighbors`, from
`ARegionArray::get_starting_region_candidates`. That function weighs **resources** — wood, iron,
stone, grain or livestock, and horses or camels within two hexes, on a landmass of at least ten —
and says nothing about whether the hex has a town.

Every other ruleset gets a town anyway, because the engine's occupancy cascade in
`Game::DoAMoveOrder` tries three town-only match levels before it will take an empty hex, and it is
handed the whole terrain-matched pool to look through. `rimefall` hands it one hex, so the cascade
has nothing to choose between and falls straight through to "completely empty hexes". The
preference is not overruled; it is bypassed.

Measured on a real eight-faction game (#80): **six of eight factions began in a hex with no
settlement.** What that costs is exactly the market. Recruiting belongs to the region rather than
the town ([0016](0016-a-town-owns-its-markets-the-region-owns-recruiting.md)), so a townless start
still raises men — but it has no wanted list at all, so nothing it produces can be sold where it
stands, and the opening every Atlantis player knows is closed to it.

## Decision

**A start location carries a settlement.** Two steps, in this order:

1. `SetACNeighbors` draws from candidates that already have a town, and only falls back to
   town-less ones once a band runs out. Same policy the band quota already uses when candidates are
   short: take what there is, log it, do not fail.
2. `Game::CreateWorld` then founds a **village** on every start slot that still has none.

Both are ruleset code. Step 2 lives in `rimefall/world.cpp` rather than beside step 1 because
`ARegion::add_town` is private and `Game` is a friend of `ARegion` while `ARegionList` is not —
which is what keeps this from needing a second engine hook.

A village, never a town or a city: the smallest size that carries a market, so a slot that had to
be helped never outranks one the map settled on its own.

## Why the preference alone is not enough

It was the obvious answer, it was built first, and it was measured. Same seed, three map sizes,
counting start slots that carry a settlement:

| Map | land hexes | settlements | before | preference only | preference + founding |
| --- | --- | --- | --- | --- | --- |
| 24×24 | 88 | 9 | 3 of 20 | 8 of 20 | **20 of 20** |
| 32×32 | 155 | 16 | 1 of 20 | 7 of 20 | **20 of 20** |
| 64×64 | 589 | 55 | 2 of 19 | 17 of 19 | **19 of 19** |

On a large world the preference nearly does the job on its own. It cannot do it on a small one, and
the reason is arithmetic rather than policy: settlements are roughly a tenth of the land, so a
24×24 world holds about nine of them while the density curve in `rimefall.h` asks for twenty
starts. There is nothing to prefer.

The founding is what the largest map pays least for: **two villages at 64×64**, against twelve at
24×24. A world too small to settle twenty starts is being asked for something it does not have, and
this is where that shows.

## What was weighed

**Preference only, and accept the gap.** Rejected on the numbers above: it leaves the reported
defect in place at the sizes below 64×64, and even at 64×64 leaves two starts marketless in the
thin bands.

**Widen the candidate test so towns qualify without the resource check.** Rejected: the resource
guarantee is the other half of what makes a start playable, and trading one silent shortfall for
another is not an improvement.

**Found a *city*, as `MakeStartingCity` does.** Rejected: `rimefall` has no start cities by design
(`START_CITIES_EXIST` is 0, which is what selects the curated gateway path in the first place), and
a city start would be worth more than the natural villages the map hands everyone else.

## Consequences

- **Existing worlds keep their start locations.** This runs at world creation only. A game already
  in progress — including the one that reported this — keeps the slots it was built with. A game
  master who wants to repair one can add a town by hand with `<game> edit`: find the region, then
  `town`. Documented in `GAMEMASTER.md` 3.3.
- **The recorded `rimefall` world changes**, and its `worldgen` fixture is re-recorded with it.
  `add_town` draws from the shared `rng` stream, so everything downstream of the founding pass
  moves. The recorded turns replay from their own `game.in` and are untouched.
- **The engine log states the outcome**, per band and per founded village, and the worldgen fixture
  records those lines — which is what makes this testable at all without a new harness.
- The substitution from
  [0022](0022-a-taken-gateway-substitutes-a-start-location.md) inherits the guarantee for free: a
  substitute is another start slot, and start slots now carry settlements.
