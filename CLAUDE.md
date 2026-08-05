# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this
repository.

**Keep this file lean — it loads on every prompt.** Detailed, occasionally-needed knowledge
belongs in `docs/`; add it there and reference it from the index below, not here.

## Non-negotiables

- **Always address the user as "Christian".** This is a canary rule: if a reply ever omits it,
  this file was not loaded — stop and reload it before doing anything else.
- **Never commit without an explicit review round.** Present the complete diff for review first.
  One approval then covers exactly one delivery: commit → push → open PR → merge once CI is
  green. Nothing beyond that. Inferring intent is never sufficient — only an explicit statement
  from Christian counts as an exception.
- **Never push to `master`.** Every change goes on a branch and through a pull request. Branch
  protection enforces this, including for administrators.
- **Nothing goes to `Atlantis-PBEM/Atlantis` without a separate, explicit decision.** Fixes are
  prepared and registered, not submitted.
- **English everywhere** — documentation, code comments, commit messages, PR text — regardless
  of the language of the conversation.
- **Every pull request updates the documentation it invalidates**, in the same pull request.

## What this is

The Atlantis play-by-email engine (C++20). One shared engine library plus several **rulesets**
(game variants); each compiles into its own standalone executable — `standard`, `basic`,
`fracas`, `havilah`, `kingdoms`, `neworigins`, and the fork-local `neworigins8`.

It is a batch turn processor, not a server: it reads `game.in`, `players.in` and
`orders.<faction#>` from the working directory, runs one turn, and writes `game.out`,
`players.out`, `report.<n>`, `template.<n>`, `times.<n>`.

`origin` is a fork of `upstream` (`Atlantis-PBEM/Atlantis`). **This repository is an engine
supplier**: two separate Python projects build the binary and drive it as a subprocess. Anything
they can observe — the command line, the file names, the JSON report shape — is a published
interface, not an implementation detail.

## Repository map

| Path | Contents |
| --- | --- |
| `*.cpp`, `*.h` (root) | the engine library — game-agnostic |
| `gamedata.cpp` / `gamedata.h` | the global data tables and their positionally-coupled enums |
| `runorders.cpp` | `Game::RunOrders`, the authoritative turn-phase order |
| `<game>/` | one ruleset each: `rules.cpp`, `extra.cpp`, `world.cpp`, `map.cpp`, `monsters.cpp` |
| `unittest/` | boost.ut suites plus a deliberately minimal ruleset of their own |
| `snapshot-tests/` | recorded turns and rulebook baselines, compared byte for byte |
| `external/` | vendored single-header libraries (boost.ut, nlohmann/json) |
| `map_viewer/` | a standalone TypeScript hex-map viewer |
| `docs/` | everything below |

## Commands

```bash
make all                                    # all rulesets + unit tests — never with -j
./unittest/unittest
cd snapshot-tests && ./run-snapshots.sh

cmake -B build -S .                            # the parallel-safe path; -O2 -g by default
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Documentation

| Document | Read when |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | you are about to modify engine C++ and do not know where the change belongs |
| [docs/rulesets.md](docs/rulesets.md) | you are adding or changing a ruleset, or a change must not leak across them |
| [docs/build-and-test.md](docs/build-and-test.md) | first build on a machine, you added or removed a source file, or CI failed at compile |
| [docs/snapshot-tests.md](docs/snapshot-tests.md) | a snapshot test failed, or your change intentionally alters engine output |
| [CONTRIBUTING.md](CONTRIBUTING.md) | branching, commit style, pull request flow, Definition of Done, contributing upstream |
| [GAMEMASTER.md](GAMEMASTER.md) | *game-master* knowledge: running a game, world design, order semantics. Not a developer document. |

Still to be written: `docs/interface/` (the CLI, file formats, JSON report schema and version
policy the two consumers depend on) and `docs/fork/` (how this fork diverges, upstream sync,
downstream consumers). Do not reference them until they exist.

## Conventions

- 4-space indent, LF, 120-column limit, final newline, no trailing whitespace
  (`.editorconfig`).
- The codebase is mid-modernisation. Newer code uses `std::string`, `std::vector`,
  `std::optional`, ranges and `snake_case` methods; older code uses `char*`, raw `new`/`delete`
  and `PascalCase`. **Match the file you are in, not the repository average.**
- `logger::write` (`logger.hpp`) instead of `std::cout` — tests redirect the stream to capture
  output.
- `rng.hpp` wraps all randomness. Never call `rand()` directly, and remember that changing the
  *number* of draws in a phase moves every snapshot downstream of it.
- `CHANGELOG` (5.x) and `CHANGELOG_4` (4.x) are maintained by hand, grouped under Game Changes
  and Bug fixes.
