# Syncing with upstream

> **Audience:** whoever brings upstream changes into this fork.
> **Provenance:** FORK-LOCAL.

**Read this when** you are pulling `Atlantis-PBEM/Atlantis` into this fork.

Going the *other* way — preparing a fix for upstream — is covered in
[CONTRIBUTING.md § Contributing a fix to upstream](../../CONTRIBUTING.md#contributing-a-fix-to-upstream).
This document is only about the inbound direction.

## The two remotes

```
origin     https://github.com/geekblogio/atlantis-pbem-engine.git
upstream   https://github.com/Atlantis-PBEM/Atlantis.git
```

`gh` defaults to the **parent** repository on a fork. Every `gh pr` invocation in this project
passes `--repo geekblogio/atlantis-pbem-engine` explicitly, and

```bash
gh repo set-default geekblogio/atlantis-pbem-engine
```

is worth running once per clone. Without it, `gh pr create` will happily open a pull request
against upstream.

## Merge, do not rebase

`upstream/master` must remain an ancestor of `master`:

```bash
git merge-base --is-ancestor upstream/master master && echo ok
```

A merge commit is what keeps that true. Rebasing our commits on top of upstream destroys the
merge base, and every subsequent sync then re-presents changes we already carry as conflicts.

This is also why branch protection has **`required_linear_history: false`** — see
[ADR 0004](../decisions/0004-no-linear-history.md). It is not an oversight.

## The procedure

```bash
git fetch upstream
git switch -c chore/sync-upstream-$(date +%Y-%m-%d) master
git merge upstream/master
```

Then, in order:

1. **Resolve conflicts in favour of upstream wherever the change is theirs.** Our divergences
   are catalogued in [patches.md](patches.md); check each conflicting hunk against it. A
   conflict in a file with no entry there means we are carrying something unregistered — stop
   and find out what.
2. **Look for fixes that make our patches redundant.** Upstream sometimes solves the same
   problem independently. When that happens, drop our version and mark the entry
   *Landed upstream* in `patches.md`. Two entries have already gone this way.
3. **Check `GameDefs`.** A new field added upstream must be added to `neworigins8/rules.cpp`
   too. The initializer is positional and a missing field compiles — it just reads the wrong
   values from that point on. See [../rulesets.md](../rulesets.md).
4. **Check the shims.** `neworigins8/{extra,map,monsters,world}.cpp` `#include` the
   `neworigins` originals, so they follow upstream automatically. That is the point of the
   design, but it also means an upstream change to `neworigins` silently changes `neworigins8`.
5. **Build and run everything**, then read the snapshot diff rather than re-recording it:

   ```bash
   make all-clean && make all
   ./unittest/unittest
   cd snapshot-tests && ./run-snapshots.sh
   ```

6. **Re-record deliberately, if at all.** A snapshot moved by an upstream behaviour change is
   expected; classify it first, note it in the pull request, and only then re-record. See
   [../snapshot-tests.md](../snapshot-tests.md).
7. Open a pull request as usual. The sync goes through the same gate as everything else.

## Verifying afterwards

```bash
git log --oneline upstream/master..master
```

Every line must correspond to an entry in [patches.md](patches.md), named either by its pull
request number or by its short SHA. That is the acceptance check for a sync, and for the fork as
a whole; run the loop under
[*How to use this file*](patches.md#how-to-use-this-file) rather than reading the list.

## What upstream must never receive by accident

The `Upstream Hygiene` CI job fails any `upstream/*` branch whose diff touches:

```
CLAUDE.md  CONTRIBUTING.md  README.md  docs/fork/  docs/decisions/  .github/
```

A checklist does not survive a late evening; the job does. If you add a fork-local path, add it
to that pattern in `.github/workflows/ci.yml` at the same time.
