# 0004 — No linear history requirement

**Status:** accepted, 2026-08-05.

## Context

`required_linear_history: true` is a sensible default: it forbids merge commits and keeps the
history readable, which also makes cherry-picking to upstream cleaner.

## Decision

`required_linear_history: false`.

## Why

Syncing from upstream **requires** a merge commit. `upstream/master` has to remain an ancestor
of `master`:

```bash
git merge-base --is-ancestor upstream/master master
```

Rebasing our commits on top of upstream destroys the merge base. Every subsequent sync then
re-presents changes we already carry as conflicts, and the fork becomes progressively harder to
maintain — exactly the outcome the fork exists to avoid.

## Consequences

The history is not linear, and that is intentional rather than neglect. Cherry-pick
friendliness is preserved a different way: `upstream/*` branches are **rebase-merged**, so
their individual commits land on `master` unchanged and stay cherry-pickable. Fork-local work is
squash-merged.
