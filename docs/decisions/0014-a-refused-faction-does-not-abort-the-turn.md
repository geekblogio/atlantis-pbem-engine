# 0014 — A refused faction does not abort the turn

**Status:** proposed, 2026-08-15. Second engine change for `rimefall`, under the boundary
[0012](0012-a-ruleset-hook-for-gateway-destinations.md) drew: one hook for one purpose, and
anything further argued again from scratch. This is that argument.

## Context

`Game::SetupFaction` returning 0 is the engine's way for a ruleset to refuse a faction.
`neworigins` uses it to close registration once its end-game condition is met, and
[0010](0010-climate-banded-single-continent-ruleset.md) section 9 chose it for `rimefall` so that a
full world turns players away cleanly instead of stranding them in a Nexus with no gateway they can
use.

0010 described the consequence as *"`AddFaction` discards the faction and logs it"*. That
understates it. In `Game::ReadPlayers`:

```cpp
fac = AddFaction(noleader, reg);
if (!fac) {
    logger::write("Failed to add a new faction!");
    return_code = false;
    break;
}
```

`return_code = false` aborts the run. The turn is **not processed at all**: no `game.out`, no
reports, no `times`, and the process exits non-zero. `game.in` is untouched, so removing the
`Faction: new` line and running again recovers — but until someone does that by hand, the game is
stopped.

Stage 3 (`#41`) made this reachable in ordinary play. A `rimefall` world holds a fixed number of
start slots, and filling them is a normal thing for a game to do rather than an end state.

**Why this is not merely cosmetic.** This repository is an engine supplier. `docs/interface/`
records that everything observable from outside the binary — the command line, the file names, the
report shape — is a published interface, and two Python projects drive this binary as a subprocess.
A non-zero exit and a missing `game.out` is how those projects learn a turn failed. Reporting a
failure for something that is not a failure trains the operator to ignore the signal, and the
signal is the only one there is.

## Decision

**A faction that a ruleset refuses is skipped. The turn runs.**

`ReadPlayers` logs the refusal and carries on to the next faction block instead of setting
`return_code = false`. Everything else about the refusal is unchanged: the faction is not created,
`AddFaction` still deletes it, and the ruleset still decides.

### The trap this has to avoid

The parse loop applies every subsequent line to whatever `fac` currently points at:

```cpp
} else if (fac) {
    return_code = ReadPlayersLine(token, parser, fac, lastWasNew);
```

Continuing naively would leave `fac` pointing at the **previously read faction**, and the refused
faction's `Name:`, `Email:` and `Password:` lines would be applied to it — quietly overwriting a
real player's registration with the rejected newcomer's details. That is far worse than the abort
being fixed.

`AddFaction` already returns `nullptr` on refusal, so the fix is to leave `fac` null and clear
`lastWasNew`, letting the refused faction's remaining lines fall through the `else if (fac)` guard
and be discarded. **No new state, and the failure mode is the safe one.**

## Consequences

- **This changes `neworigins` too**, and deliberately. Once its annihilation end-game closes
  registration, a leftover `Faction: new` line currently stops every turn until a human intervenes;
  afterwards the turn runs and the newcomer is skipped. That is what *closing registration* should
  have meant all along.
- **The recorded turn snapshots must stay byte-identical.** None of them contains a refused
  faction, so nothing should move; that is the evidence, and it must be checked rather than
  assumed.
- **A refusal becomes quiet.** Today it is impossible to miss, because everything stops. Afterwards
  it is a line in the engine log. The log message therefore has to say plainly which faction was
  refused and why, and `GAMEMASTER.md` section 3.3 has to be rewritten — it currently documents the
  abort as the expected behaviour.
- **Second engine divergence**, registered in `docs/fork/patches.md` alongside `#41`'s hook.
  `game.cpp` is actively maintained upstream and this will need re-applying on a sync.
- **Upstream-worthy, and not offered.** Aborting a batch run over a refusal the ruleset asked for
  looks like an upstream bug rather than a `rimefall` need — the fix is not conditional on the
  ruleset and helps `neworigins` first. Per [0008](0008-prepare-upstream-fixes-do-not-submit.md) it
  is prepared and registered here; offering it is its own decision.
- **The boundary from 0012 still holds.** This is a second engine change, argued on its own and
  scoped to one function. It is not licence for a third.
