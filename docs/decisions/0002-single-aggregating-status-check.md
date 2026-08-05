# 0002 — One aggregating `CI` status check

**Status:** accepted, 2026-08-05.

## Context

Branch protection requires named status checks. The obvious approach is to require each job:
`Build & Test (cmake)`, `Build (make)`, and so on.

## Decision

Require exactly one context, `CI`, produced by an aggregating `gate` job that runs
`if: always()` and fails when any needed job reports `failure` or `cancelled`. `skipped` passes.

## Why

Two failure modes that are invisible until they bite:

- **A renamed or removed job leaves its context "expected" forever.** Every pull request then
  hangs, unmergeable, with no way to clear it except editing the protection rules.
- **A newly added job is not required.** It can be red and the pull request still merges.

One stable string removes both. Jobs can be added, renamed and split freely.

There is deliberately **no workflow-level `paths:` filter**, for the same reason: a
documentation-only pull request must still produce the `CI` context. Filtering happens inside
the workflow, in the `changes` job and per-job `if:` conditions, so skipped jobs still report a
result.

## Consequences

`gate` must never be renamed. The aggregation checks results explicitly rather than relying on
`if: success()`, which would pass when a needed job was skipped.
