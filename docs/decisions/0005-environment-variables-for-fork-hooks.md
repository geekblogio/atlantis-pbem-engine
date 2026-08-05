# 0005 — Environment variables, not CLI flags, for the fork hooks

**Status:** accepted, 2026-08-05.

## Context

Three switches were ported from the consumers' local engine patch: `ATLANTIS_SEED`,
`ATLANTIS_SIM_MODE` and `ATLANTIS_NO_GM_REPORT`. All three follow the same shape — an
environment variable read in `main.cpp` that overrides a `Globals` field.

CLI flags would be the cleaner design. They are discoverable in `usage()`, they cannot be
inherited accidentally by a child process, and they do not depend on the caller's environment
being clean.

## Decision

Port them as environment variables, unchanged, including the names.

## Why

Both Python consumers already set these variables by name. Converting to flags would be a
breaking change for them in exchange for aesthetics — in the same step that is supposed to make
the engine easier for them to consume.

There is also a practical argument in this specific case: the engine runs as a subprocess inside
a Docker image whose command line the orchestrator does not always control, while the
environment it does.

## Consequences

- `usage()` lists the variables explicitly, since they cannot be discovered any other way.
- Each one is **inert when unset** — no behaviour change, no log line. That is what makes them
  safe to carry.
- `ATLANTIS_SEED` is offered upstream as-is; the other two are fork-local. If upstream wants the
  GM-report switch, the right shape there is `--no-gm-report`, and this fork can gain the flag
  *in addition to* the variable without breaking anyone.

Recorded in [../fork/downstream-consumers.md](../fork/downstream-consumers.md) as a consumer
dependency.
