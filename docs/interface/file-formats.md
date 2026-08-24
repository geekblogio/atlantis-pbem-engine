# File formats

> **Audience:** anyone producing or consuming the engine's files.
> **Provenance:** upstream-friendly.

**Read this when** you are writing `players.in`, staging a turn, or parsing what came back.

All of these live in the process's working directory under fixed names. See
[cli.md](cli.md) for the working-directory model.

## What a turn touches

| File | Direction | Notes |
| --- | --- | --- |
| `game.in` | in | the world; opaque, engine-written |
| `players.in` | in | faction administration **and the game-master directives** |
| `orders.<n>` | in | one per faction that submitted orders; `<n>` is the faction number |
| `game.out` | out | rename to `game.in` for the next turn |
| `players.out` | out | rename to `players.in` for the next turn |
| `report.<n>` | out | the human-readable turn report, if `REPORT_FORMAT_TEXT` |
| `report.<n>.json` | out | the machine-readable report, if `REPORT_FORMAT_JSON` |
| `template.<n>` | out | order template, only with `REPORT_FORMAT_TEXT` and only for player factions |
| `times.<random>` | out | world-events articles — see the warning below |
| `rimefall.json` | out | **`rimefall` only** — that turn's front, sources and election as numbers; see below |
| `names.out` | out | `havilah` only, written by `new`: every generated region name, for a GM to scan |

A missing `orders.<n>` is not an error: that faction simply issued no orders.

### `times.<random>` does not mean what it looks like

The suffix is **not a faction number**. Each world-events article is written to
`times.<0..9999>`, with the number drawn from the engine's RNG; if that name is taken the engine
counts up from it without drawing again. So:

- collect them with a glob, never by faction number;
- **delete them between turns**, or the file set accumulates and later turns are increasingly
  likely to land on a name that is already there;
- the count is the number of articles, not the number of factions.

Up to `#65` the collision was resolved by drawing again, which put the contents of the working
directory into the turn: one leftover `times.*` file shifted the shared random stream, and with
it the monsters that spawned later in the same turn and the generator state saved in `game.out`
— so every following turn drifted too. A turn replayed in a dirty directory did not reproduce.
That is fixed; **an uncleared directory now costs nothing but clutter.**

### `rimefall.json`

Written by the `rimefall` ruleset alone, once per `run`, overwritten each turn. It carries what the
ruleset's own balance depends on, as numbers rather than prose:

```json
{
  "turn": 34,
  "front": {
    "row": 12, "ran": true, "dragons_awake": true,
    "population_in_reach": 21500, "battles": 1,
    "threat": { "total": 78, "time": 68, "prosperity": 8, "discord": 10, "threshold": 60 },
    "stacks_allowed": 1, "stacks_placed": { "north": 1, "east": 0 }
  },
  "sources": { "north": { "name": "The Rimewell", "held": false },
               "east":  { "name": "The Saltspire", "held": false } },
  "election": { "open": false }
}
```

**The three threat terms are separate on purpose.** Only their sum is visible anywhere else, and a
sum cannot say whether a front is being driven by time, by prosperity or by the players fighting
each other — which is the first question anyone tuning the ruleset asks. `election` grows the
leader, the electorate and the percentage once both sources are held.

It is a **ruleset's file, not the engine's**: no other ruleset writes it, and nothing else reads
it. A consumer that does not know about it can ignore it; `report.<n>.json` is unchanged.

## `players.in`

A plain-text file the orchestrator generates. This is the normative description — the engine
parses it in `Game::ReadPlayers` / `Game::ReadPlayersLine`.

### Header

```
AtlantisPlayerStatus
Version: <CURRENT_ATL_VER as an integer>
TurnNumber: <ignored on read>
GameStatus: New|Running|Finished
```

The first line must match exactly. The version check is **strict on major and minor and
tolerant downward on patch**: a file whose major or minor differs, or whose patch is *higher*
than the engine's, is rejected with *"The players.in file is not compatible with this version
of Atlantis."* A lower patch level is accepted. `TurnNumber:` is read and discarded.

Any other `GameStatus:` value aborts the read.

### Faction blocks

Then one block per faction, each opening with `Faction: <number>`. `players.out` writes exactly
these keys back:

```
Faction: 3
Name: Example Faction (3)
Email: player@example.com
Password: secret
LastOrders: 4
FirstTurn: 1
SendTimes: 1
Template: long
Battle: na
```

`Faction: new` opens a block for a faction that does not exist yet, optionally followed by
`noleader` and a starting `x y z`.

**A ruleset may refuse a new faction, and refusal is not an error.** `rimefall` refuses one once
every starting location in its world is held; `neworigins` refuses one after its end-game closes
registration. When that happens the whole block is discarded — including its `Name:`, `Email:` and
`Password:` lines, which do **not** attach themselves to the faction listed before it — and the
engine reports it on stdout:

```
A new faction was refused by the ruleset and has been skipped. The turn continues without it.
```

The turn then runs normally for every other faction and the exit code is `0`, the same treatment a
malformed game-master directive gets. **A caller that submits `Faction: new` must therefore scan
stdout to learn whether the faction was created**; nothing else reports it, and the refused player
receives no report file. Older builds abandoned the entire run instead, writing no `game.out` and
exiting non-zero; see `docs/decisions/0014`.

### Directives

Every line the reader understands. The ones past `Battle:` are **game-master directives**: they
mutate the game when the turn runs and are not echoed back to `players.out`.

| Directive | Effect |
| --- | --- |
| `Name:` | faction name. For a new player the faction number is appended in parentheses. |
| `Email:` | contact address; appears in the report and the JSON `administrative` block |
| `Password:` | empty value stores the literal `none` |
| `Battle:` | accepted and ignored (`players.out` always writes `na`) |
| `Template:` | order-template format; the `TemplateStrs` names, e.g. `off`, `short`, `long`, `map` |
| `SendTimes:` | non-zero means the faction receives the world-events articles |
| `LastOrders:` | turn number of the faction's last submitted orders — drives inactivity deletion |
| `FirstTurn:` | the turn the faction joined |
| `RewardTimes` | flag, no value |
| `Reward: <amount>` | grants silver and records a `reward` event |
| `Loc: <x> <y> <z>` | sets the working location for the directives below; all three coordinates are required |
| `NewUnit: <alias>` | creates a unit at the current `Loc:`. **Requires a preceding `Loc:`** — without one the line is refused with a message. |
| `Item: <unit> <count> <item>` | gives items to a unit of this faction |
| `Skill: <unit> <skill> <days>` | grants skill days; the value is multiplied by the unit's man count |

`<unit>` is either a unit number or a `NewUnit:` alias. A directive naming a unit that belongs
to another faction is refused with a message.

**A malformed directive is reported on stdout and skipped — the turn still runs.** There is no
exit code for "your `players.in` had a bad line". A caller that generates this file
programmatically should scan stdout for these messages.

## `game.in` / `game.out`

The complete world state: regions, objects, units, factions, the RNG seed, the turn counter.

**Treat it as opaque.** It is a positional text format written and read by the same code, with
no schema and no forward-compatibility guarantees; the layout changes whenever a field is added
to a serialised class.

Two fields do matter to a caller, both checked when the file is opened:

- the **ruleset name** must equal the binary's `RULESET_NAME`, otherwise `OpenGame` refuses the
  file. This is why `neworigins8` deliberately keeps the name `NewOrigins`.
- the **ruleset version**, which triggers the `upgrade_*_version` path when the binary's version
  is higher.

See [compatibility.md](compatibility.md).

## `orders.<n>`

The faction's order file, as the player wrote it. Opens with

```
#atlantis <faction-number> "<password>"
```

then `unit <number>` blocks. Order grammar and semantics are a game-master topic and live in
[GAMEMASTER.md](../../GAMEMASTER.md); only the framing is an interface concern.

`<game> check <orderfile> <checkfile>` parses an order file without running a turn and writes
the diagnostics to `<checkfile>` — the intended pre-submission validation hook.

## `report.<n>` and `template.<n>`

Human-readable, line-wrapped text. **Do not parse them.** They are rendered *from* the JSON
report by `text_report_generator.cpp`, so anything in the text is in the JSON, and the JSON is
stable in a way the prose layout is not. See [json-report.md](json-report.md).

`template.<n>` is written only for player factions and only when `REPORT_FORMAT_TEXT` is on —
a faction that receives a GM report never gets a template.

## Line endings

Every fixture in `snapshot-tests/` is compared byte for byte against the output of a **Linux**
binary. `.gitattributes` forces `eol=lf` on those trees for that reason. A Windows build writes
CRLF in text mode and will differ in every single file; that is expected, and why the snapshot
suite is Linux-only.
