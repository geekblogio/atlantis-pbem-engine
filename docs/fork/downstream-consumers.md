# Downstream consumers

> **Audience:** anyone changing something the two Python projects can observe.
> **Provenance:** FORK-LOCAL.

**Read this before** any change that could break the projects that build and run this engine.

## Who consumes this

Two Python projects, today sharing one repository (`atlantis-pbem`) and intended to be split:

| Consumer | What it does with the engine |
| --- | --- |
| **the PBEM orchestrator** | hosts real games: generates `players.in`, stages `orders.<n>`, runs one turn per invocation, distributes the reports |
| **the simulation / bot lab** | runs thousands of turns unattended, reads only `report.<n>.json` |

They build the binary from this source — the orchestrator via `make -C src havilah` in a Docker
image — and drive it as a subprocess. **There is no library boundary, no API, and no version
negotiation.** The interface is the process: its command line, its files, its exit code, its
stdout.

That interface is specified in [../interface/](../interface/README.md). This document is only
about who depends on it and what that obligates.

## What they depend on, concretely

- **`make -C src havilah` keeps working**, and the Makefile path in particular — not just
  CMake. The CI job `Build (make)` exists for this reason and no other.
- **The working-directory model.** One directory per game, fixed file names, one turn per
  process. Changing that is a rewrite on their side, not an adaptation.
- **`report.<n>.json`.** The simulation reads nothing else. A removed or renamed field is a
  silent failure the next time a turn runs.
- **The three environment variables.** `ATLANTIS_SEED`, `ATLANTIS_SIM_MODE` and
  `ATLANTIS_NO_GM_REPORT` are load-bearing for the simulation, by name. That is precisely why
  they were ported as environment variables rather than converted to CLI flags — see
  [ADR 0005](../decisions/0005-environment-variables-for-fork-hooks.md).
- **`neworigins8`** for the live game. Consumers that want NewOrigins 8 build that target
  rather than patching `neworigins` — this replaced a local patch and is a deliberate,
  downstream-visible change.

## Obligations when changing this repository

1. **Assume nothing is unobservable.** If it can be seen from outside the process, it is
   interface. Re-read [../interface/README.md § What counts as a breaking change](../interface/README.md).
2. **A red snapshot is the interface telling you it moved.** Do not re-record to get green; work
   out what changed and whether a consumer cares.
3. **Update `docs/interface/` in the same pull request.** Documentation that lags a release is
   worse than none, because it is trusted.
4. **Say so in the pull request** when a change is consumer-visible, even if it is additive.
   The consumers are not in this repository's CI and nothing here will catch a break.
5. **Verify on the consumer side after merging** anything that touches the interface: run
   `pytest app` in `atlantis-pbem` against the newly built engine, plus one simulation run as a
   smoke test.

## The gap worth knowing about

**Nothing in this repository's CI tests a consumer.** The snapshot suite proves that engine
output did not change; it cannot prove that a consumer still parses it. Until the two Python
projects are split out and pinned against a tagged engine build, that verification is manual
and the responsibility sits with whoever merges.
