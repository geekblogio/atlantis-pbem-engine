# Versions and compatibility

> **Audience:** anyone changing a version constant, or deciding whether a change is breaking.
> **Provenance:** upstream-friendly.

**Read this when** you are about to change a version constant, or you need to know what a
consumer is allowed to assume.

## Three independent versions

| Constant | Where | Covers |
| --- | --- | --- |
| `CURRENT_ATL_VER` | `game.h` | the engine itself, and the `players.in` file format |
| `RULESET_VERSION` | each ruleset's `rules.cpp` | that ruleset's game rules, and its `game.in` layout |
| `JSON_REPORT_VERSION` | `game.h` | the shape of `report.<n>.json` |

They move independently. A ruleset can change without the engine version moving, and the JSON
report can gain a field without either.

## How a version is encoded

`MAKE_ATL_VER(x, y, z)` packs three bytes into an `unsigned int`
(`helper.h`): `(major << 16) + (minor << 8) + patch`. Files store the packed integer, not the
dotted string.

**`ATL_VER_STRING` appends `" (beta)"` when the *minor* number is even.** So `5.2.5` renders as
`5.2.5 (beta)` and `5.3.0` would render as `5.3.0`. This is the odd/even stable convention, not
a mistake and not a per-release decision — an even minor is by definition a beta.

A consumer parsing the `engine` block of the JSON report must therefore **split on whitespace
before parsing the version**, or accept the suffix as part of the value. Do not compare the
whole string for equality across releases.

## What the engine enforces at load

### `players.in` — strict on major and minor

`Game::ReadPlayers` rejects the file unless:

- major equals the engine's, **and**
- minor equals the engine's, **and**
- patch is **less than or equal to** the engine's.

A newer patch level than the binary is refused. The message is *"The players.in file is not
compatible with this version of Atlantis."* and the run stops.

So the orchestrator must write the `Version:` of the engine it is about to invoke — not a
value it remembers from an earlier build.

### `game.in` — name exact, version upgraded forward

`Game::OpenGame` checks two things:

1. **The ruleset name must match exactly.** A mismatch is *"Incompatible rule-set!"* and the
   run stops. There is no override. This is why `neworigins8` keeps the name `NewOrigins` —
   see [../rulesets.md](../rulesets.md).
2. **The stored ruleset version may be older**, in which case `upgrade_major_version`,
   `upgrade_minor_version` and `upgrade_patch_level` run in turn, each able to abort with
   *"Unable to upgrade! Aborting!"*.

A stored version *newer* than the binary is **not** rejected — the comparisons are only
`<`. An old binary will happily open a newer game file and process it with the wrong rules.
Nothing warns. Guarding against that is the caller's job: pin the engine build per game.

## What is a breaking change

Needs a version bump **and** a note to the consumers:

- removing or renaming a JSON report field, or changing its type
- renaming, adding or removing a subcommand, or changing its argument count
- changing an exit code
- renaming an input or output file, or changing the condition under which one is written
- changing the meaning of a `players.in` directive
- any rules change that alters engine output — which is every change the snapshot suite catches

Safe and additive, no bump required:

- adding an optional JSON field (say so in the pull request anyway)
- adding a subcommand
- adding an environment variable that is inert when unset

## The snapshot suite is the tripwire

Every recorded turn is compared byte for byte, including the engine's stdout. Any change to
observable behaviour turns the suite red before it reaches a consumer. That is the point:
**a red snapshot is not a broken test, it is the interface telling you it moved.** See
[../snapshot-tests.md](../snapshot-tests.md).

The two things it cannot catch, because no fixture exercises them:

- world generation (`new`) — the fixtures replay pre-generated worlds
- the `havilah`, `basic`, `fracas` and `kingdoms` rulesets beyond their rulebooks — only
  `standard` and `neworigins` have turn fixtures
