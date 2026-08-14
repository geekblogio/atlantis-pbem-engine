# 0011 — Rimefall: invasion triggers, the front's clock, and how the game is won

**Status:** accepted, 2026-08-14.

## Context

[0010](0010-climate-banded-single-continent-ruleset.md) settled what `rimefall` *is*. Planning its
implementation surfaced six things that record does not cover, gets wrong, or cannot express — two
of them design gaps large enough that the ruleset could not have been built without answering them.
0010 is accepted and this register is append-only, so it is extended here rather than edited.

Three properties of the engine constrain every answer below, and are recorded because they are not
obvious and would otherwise be rediscovered painfully:

- **`CheckVictory` is the only per-turn ruleset hook, and `runorders.cpp` calls it only when
  `OPEN_ENDED == 0`.** Gateway rebuild, both fronts and the election hang off it.
- **`rulesetSpecificData` is not persisted.** `Game::SaveGame` writes year, month, sequences,
  factions, regions and quests, nothing else. That JSON is re-established every run from
  `ModifyTablesPerRuleset`, so it is *configuration*, never state. **There is no accumulator
  available anywhere**; all ruleset state must be derived from `TurnNumber()` or read back out of
  the persisted world.
- **Monsters do not march directionally.** `Unit::DefaultOrders` gives wandering monsters a random
  walk weighted by terrain preference, and ice creatures prefer tundra — they would stay north. No
  `modify_monster_*` helper reaches terrain preference, so changing it would mean `gamedata.cpp`,
  which 0010 section 4 forbids.

## Decision

### 1. The northern front creeps south to the end of the continent

Its spawn band's **position** is a pure function of `TurnNumber()` and advances until it has
consumed the map. Nothing slows it; only destroying its source ends it.

This gives the game a hard clock, which is the point. It also means front speed is the single most
important balance constant in the ruleset, and the first thing to revisit after a real game.

**Why a function of the turn number:** with no accumulator available, a derived position is the only
form that survives a save cycle. The side benefit is that the advance is legible — players can see
winter coming and plan against it.

### 2. Position and strength are separate mechanisms

The creep sets **where** the front is. A separate **threat score** decides **when it attacks and how
hard**, recomputed from scratch every turn from three terms:

| term | read from | meaning |
| --- | --- | --- |
| time | `TurnNumber()` | rises steadily; doubles as the grace period, since it starts near zero |
| prosperity | population, regions and buildings within the front's reach | growing fat in its path draws it onto you |
| discord | player-versus-player battles that turn | the noise of civil war calls them |

Each term is a separately tunable constant. There is deliberately **no separate grace period**: the
time term starting near zero is the grace period.

**Why three terms rather than a schedule:** a pure schedule gives players no agency, and pure
provocation lets a quiet player never be attacked, which would waste the clock in decision 1.

### 3. Only player-versus-player battles count as discord

`Game::battles` is readable inside `CheckVictory` — it is cleared only after `RunOrders`. But
`Battle` does not record its sides usably, and `Faction::battles` also collects mere bystanders. So
a player-versus-player battle is identified **by exclusion: one referenced by neither `monfaction`
nor `guardfaction`.** The test errs conservatively, under-counting rather than over-counting.

**Why exclude monster battles:** counting them creates a feedback loop in which a large wave
produces many battles and therefore a larger wave. The conservative direction of the filter matters
precisely because it cannot drive that loop.

### 4. The dragons wake when the north burns — or when it is saved

The eastern front wakes when the northern spawn band passes a set depth, **or immediately when the
northern source is taken**, whichever comes first. Both conditions are derivable.

**Why the second condition:** without it, destroying the northern source early would stop the front
before it reached the wake depth and the eastern front would never happen — a fast group would skip
half the game. With it, success in the north *brings the second act forward* instead of cancelling
it. One catastrophe ending begins the next.

The eastern source still exists from world creation regardless, so victory always requires taking
both.

### 5. Starting alliances are by proximity, and are applied on arrival

**Proximity, not band.** 0010 section 8 contradicts itself — its heading says "same band", its body
says "clustered by starting location". The body wins: section 7 gives the middle band by far the
most starting locations, so a band-wide alliance would make the most contested zone a single bloc.

**On arrival, not at faction creation.** 0010 places the alliance in `SetupFaction`, which cannot
work: with gateway-based start selection the faction is still in the Nexus at that moment and has no
location yet. The relationship is written when a faction first takes a start region, from the
per-turn hook, on both sides — `set_attitude` is one-directional.

### 6. Victory: kill both hordes, then crown a king

While either source stands, there is no winner. Once **both** are held by players, the election
opens: the winner is the faction holding mutual `ALLY` from at least a configured share of living
factions.

`neworigins` already implements this shape for its monolith ending, including the percentage
threshold, so it is a proven mechanism rather than a new one. Because starting alliances are small
proximity clusters, no bloc can win on its own — a candidate has to win over factions outside it.

If the fronts consume the continent first, the factions die out and the existing `!livingFacs` path
ends the game. The loss condition needs no code.

## Consequences

- **`OPEN_ENDED` must stay `0`**, and `rimefall/rules.cpp` says why at the field. Setting it to `1`
  would silently disable the gateway rebuild, both fronts and the election, leaving a ruleset that
  still builds, still runs, and is no longer the game.
- Voting with `ALLY` means the ballot is not free: `ALLY` also permits `GIVE UNIT` and defeats theft
  and assassination attempts. Crowning someone means opening your gates to them. That is intended,
  and follows 0010 section 8's decision to use the heavy attitude deliberately.
- The threat score has three interacting terms and no accumulator to smooth it, so it will be spiky
  until tuned. All three weights are expected to move after the first game.
- Bands the front has overrun must stop offering start slots, or the game sends newcomers into
  ground that is already lost. A useful side effect: **registration closes itself as the world
  falls**, which should be deliberate rather than accidental.
- Nothing here needs a `GameDefs` field or an engine change. Every value is a constant in
  `rimefall/extra.cpp`, and every input is either `TurnNumber()` or the persisted world.
