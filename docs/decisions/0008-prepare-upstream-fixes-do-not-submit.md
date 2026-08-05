# 0008 — Prepare and register upstream fixes; do not submit them

**Status:** accepted, 2026-08-05.

## Context

Several fixes carried by this fork are plainly upstream's bugs: dangling pointers in the data
tables, an unguarded `optional`, a use-after-free, an out-of-bounds read, gateways named
"Dummy". Offering them is the obviously right thing eventually.

## Decision

Prepare each one so it *could* be offered — self-contained, on an `upstream/*` branch, written in
upstream's voice, registered in [../fork/patches.md](../fork/patches.md) — and **stop there**.
Submitting is a separate, explicit decision per fix.

## Why

Opening a pull request at another project is an outward-facing act with a maintainer's attention
attached to it, and it is not reversible by deleting a branch. The fork owner decides when and in
what order that happens. Nothing about the mechanics of preparing a fix requires deciding it
early.

Preparing without submitting costs nothing and keeps every option open: the branches are ready
the moment the decision is made.

## Consequences

- Branch naming, commit voice and the `Upstream Hygiene` CI job all exist to make the preparation
  real rather than aspirational. A fix that has accumulated fork-local references cannot be
  offered later without rewriting it.
- `patches.md` carries a status per divergence. It is *Prepared* for everything upstream-worthy.
  **Nothing has been offered.**
- The submission step, when it comes, goes through a personal fork of upstream and cherry-picks
  the commits, rather than pushing a branch from here.
