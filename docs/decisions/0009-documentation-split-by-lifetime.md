# 0009 — Split documentation by lifetime, behind one router

**Status:** accepted, 2026-08-05.

## Context

The project needs documentation covering the engine, the rulesets, the build, the tests, the
external interface, the fork's divergence and its decisions. All of it in one file is unusable;
`CLAUDE.md` in particular is loaded on every prompt and has to stay small.

## Decision

`CLAUDE.md` is a **router**: non-negotiables, a repository map, the commands, and an index whose
every row names a *trigger* rather than a topic — "read this when …". Everything else lives in
`docs/`, split by **lifetime**:

| Lifetime | Where |
| --- | --- |
| durable reference | `docs/architecture.md`, `docs/rulesets.md`, `docs/interface/` |
| runbooks | `docs/build-and-test.md`, `docs/snapshot-tests.md`, `docs/fork/upstream-sync.md` |
| append-only history | `docs/fork/patches.md`, `docs/decisions/` |

Every file carries an `Audience:` and `Provenance:` header, so it is obvious at a glance whether
its content may travel upstream.

## Why lifetime rather than topic

Files with the same lifetime are edited in the same situations. A reference document is rewritten
when the code changes; a runbook is corrected when someone follows it and it fails; an
append-only file is never edited at all. Grouping by topic instead produces files that need three
different editing disciplines at once, and they rot unevenly.

Trigger-based index rows matter for the same reason: "read this when a snapshot test failed" gets
read at the moment it helps. "Snapshot tests" does not.

## Consequences

- **Exactly one sub-index**, `docs/interface/README.md`. Two indexes drift, so there is no
  `docs/README.md`.
- Fork-local content is confined to `docs/fork/`, `docs/decisions/`, `.github/` and three named
  root files — which is what makes the `Upstream Hygiene` CI job expressible as a path pattern.
- **No progress log or worklog document.** This repository sees few commits a month; a living log
  would rot. `docs/fork/patches.md` and the hand-maintained `CHANGELOG` cover that ground.
- No Doxygen, no `SECURITY.md`, no issue templates — deliberately, until something needs them.
