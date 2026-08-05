# The engine interface

> **Audience:** anyone integrating the engine, or changing something visible from outside it.
> **Provenance:** upstream-friendly, except where marked fork-local.

**Read this when** you are about to change anything observable from outside the binary, or you
are wiring the engine into another program.

This repository is an **engine supplier**. Two Python projects build the binary and drive it as
a subprocess. Everything they can see — the command line, the file names, the file contents,
the JSON report shape — is a published interface, not an implementation detail. Changing it
breaks them silently, at the next turn, in production.

This is the only sub-index in `docs/`. Four documents:

| Document | Read when |
| --- | --- |
| [cli.md](cli.md) | you are invoking the engine, or changing a subcommand, exit code or environment variable |
| [file-formats.md](file-formats.md) | you are producing or consuming `game.*`, `players.*`, `orders.*`, `report.*`, `template.*` or `times.*` |
| [json-report.md](json-report.md) | you are reading the machine-readable report, or adding a field to it |
| [compatibility.md](compatibility.md) | you are changing a version constant, or need to know what a consumer may assume |

## The short version

```
cd <game-directory> && <ruleset-binary> run
```

The engine has **no configuration file and no path arguments for game data**. It reads and
writes fixed file names in the current working directory. One process, one turn, then exit.
Isolation between games is the caller's job: give each game its own directory.

## What counts as a breaking change

Anything in this list needs a version bump and a note to the consumers, not just a green CI run:

- renaming, adding or removing a subcommand, or changing how many arguments one takes
- changing an exit code
- renaming an input or output file, or changing when one is written
- removing or renaming a JSON report field, or changing its type
- changing the meaning of a `players.in` directive
- changing `CURRENT_ATL_VER`, `JSON_REPORT_VERSION` or a ruleset's `RULESET_VERSION`

Adding an *optional* JSON field or a *new* subcommand is additive and safe. See
[compatibility.md](compatibility.md).
