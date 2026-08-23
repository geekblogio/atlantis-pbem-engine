# Decision records

> **Audience:** anyone about to reopen a decision that has already been made.
> **Provenance:** FORK-LOCAL.

**Read this when** something here looks wrong and you want to change it. The record will tell
you what was weighed, so you can argue against the actual reasoning rather than a guess at it.

**Append-only.** A decision that is reversed gets a *new* record superseding the old one; the
old one stays, marked as superseded. Nothing is edited into agreement with the present.

**Status.** A record is `proposed` while its pull request is open and `accepted` once that pull
request merges — merging it *is* the acceptance, since review happens before the commit
([0003](0003-no-required-reviews.md)). The status is the only field that changes after the fact;
the reasoning never does. Six records sat at `proposed` long after their changes had shipped, which
is what prompted writing this down.

| # | Decision |
| --- | --- |
| [0001](0001-keep-master-as-default-branch.md) | Keep `master` as the default branch |
| [0002](0002-single-aggregating-status-check.md) | One aggregating `CI` status check, not per-job contexts |
| [0003](0003-no-required-reviews.md) | No required GitHub reviews; review happens before the commit |
| [0004](0004-no-linear-history.md) | No linear history requirement |
| [0005](0005-environment-variables-for-fork-hooks.md) | Environment variables, not CLI flags, for the fork hooks |
| [0006](0006-neworigins8-as-its-own-ruleset.md) | NewOrigins 8 as its own ruleset, sharing name and version |
| [0007](0007-build-at-o2.md) | Build at `-O2`, with `genrules.cpp` exempt |
| [0008](0008-prepare-upstream-fixes-do-not-submit.md) | Prepare and register upstream fixes; do not submit them |
| [0009](0009-documentation-split-by-lifetime.md) | Split documentation by lifetime, behind one router |
| [0010](0010-climate-banded-single-continent-ruleset.md) | A climate-banded single-continent ruleset |
| [0011](0011-rimefall-invasion-triggers-and-victory.md) | Rimefall: invasion triggers, the front's clock, and victory |
| [0012](0012-a-ruleset-hook-for-gateway-destinations.md) | A ruleset hook for gateway destinations, amending 0010 |
| [0013](0013-the-gateway-hook-sets-the-candidate-list.md) | The gateway hook sets the candidate list |
| [0014](0014-a-refused-faction-does-not-abort-the-turn.md) | A refused faction does not abort the turn |
| [0015](0015-one-production-per-item-and-skill.md) | One production per item and skill, in a region |
| [0016](0016-a-town-owns-its-markets-the-region-owns-recruiting.md) | A town owns its markets; the region owns recruiting, amending 0015 |
| [0017](0017-x86-64-is-the-reference-for-draw-order.md) | x86-64 is the reference architecture for RNG draw order |
| [0018](0018-kingdoms-keeps-its-lakeless-coastline.md) | `kingdoms` keeps its lakeless coastline |
| [0019](0019-the-engine-owns-its-random-distributions.md) | The engine owns its random distributions, extending 0017 |
| [0020](0020-the-engine-owns-its-binomial-draw.md) | The engine owns its binomial draw too, completing 0019 |
