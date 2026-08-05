# 0001 — Keep `master` as the default branch

**Status:** accepted, 2026-08-05.

## Context

The upstream repository uses `master`. Renaming ours to `main` is the modern default and would
have been free at the point this fork had zero local commits.

## Decision

Keep `master`.

## Why

The fork syncs from `Atlantis-PBEM/Atlantis` indefinitely and prepares contributions back to it.
A different default branch name adds a permanent translation step to every sync, every
`upstream/*` branch and every cherry-pick, in exchange for a naming preference. The cost is
small but recurring; the benefit is zero.

## Consequences

Documentation, CI and branch protection all name `master`. If the fork ever stops tracking
upstream, this is worth revisiting.
