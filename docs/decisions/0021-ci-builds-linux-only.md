# 0021 — CI builds Linux only

**Status:** accepted, 2026-08-29.

## Context

`Platforms` built `make all` on `ubuntu-latest`, `windows-latest` and `macos-latest`, on every
push to `master` and weekly. It was never part of the merge gate; the reasoning recorded in the
workflow was that a portability regression found within a day is cheap, while seven minutes of
Windows compile on every pull request is not.

That reasoning weighed the wrong cost. Actions minutes on the private repository are billed with
a multiplier per runner class — Linux ×1, Windows ×2, macOS ×10 — and the workflow ran often
enough for the multiplier to dominate everything else in the account. Measured over
2026-08-05 to 2026-08-24, 69 runs, each job rounded up to the whole minute the way GitHub bills
it:

| Runner | Wall clock | Multiplier | Billed |
| --- | --- | --- | --- |
| `macos-latest` | 378 min | ×10 | **3 780 min** |
| `windows-latest` | 667 min | ×2 | 1 334 min |
| `ubuntu-latest` | 472 min | ×1 | 472 min |

**5 586 billed minutes in 19 days, 92% of it for the two platforms nothing ships on.** macOS
alone was 68% while being the *fastest* of the three in wall clock. For scale, the entire merge
gate over the same period — 186 runs, 1 284 jobs, all on Linux — cost 984 billed minutes:
`Platforms` was **5.7× everything the pull request path spent**. The account ran out of minutes.

## Decision

CI builds Linux only. The `windows-latest` and `macos-latest` matrix entries are removed, and
with a single platform left the workflow is renamed from `Platforms` to `Toolchain Drift`,
which is the job it actually still does: build unpinned, so a new runner image's compiler is
found before the pinned gate has to meet it.

The remaining Linux job also loses its `push: [master]` trigger and runs on the weekly cron and
`workflow_dispatch` only. Every ruleset is already compiled from `CMakeLists.txt` by the gate's
`Build & Test (cmake)` job, and from the `Makefile` by `Build (make)`, on every pull request
that touches code. Per-push runs of the same compile added nothing except a second opinion on
the image pin — which is exactly what the weekly run is for. That is a further ~740 billed
minutes a month.

## Why this is not a loss worth paying for

- **Nothing ships on Windows or macOS.** The two downstream Python projects build the engine
  with `make -C src havilah` inside a Debian image. There is no macOS or Windows artefact, no
  user running one, and no plan for either.
- **macOS is covered better than CI ever covered it.** Development happens on a macOS/arm64
  workstation: `make all`, the unit tests and the full snapshot suite are run there by hand
  before a pull request is opened, and since [0019](0019-the-engine-owns-its-random-distributions.md)
  and [0020](0020-the-engine-owns-its-binomial-draw.md) that machine replays all recorded turns
  byte for byte. The CI job only compiled.
- **Windows was compile-only too, and MSVC was already not a gate.** `/WX` is off there and
  `4244`, `4267` and `4700` are suppressed, so the Windows job could never be the authority on
  warnings; `docs/build-and-test.md` says as much. What it did catch is code that GCC accepts
  and MSVC rejects — a real class of finding, and the accepted cost of this decision.

## Consequences

- **A break in the MinGW or MSVC build will not be noticed here.** It surfaces when someone
  builds on Windows, or in an upstream pull request. Given [0008](0008-prepare-upstream-fixes-do-not-submit.md)
  — fixes are prepared and registered, not submitted — nothing is submitted upstream without a
  deliberate act anyway, and that act is the place to check the platform if it matters.
- The workflow file is renamed, so the run history stays under the old `Platforms` entry in the
  Actions UI. It is not a required status check and never was, so branch protection is
  untouched ([0002](0002-single-aggregating-status-check.md)).
- Reversing this is one matrix line. If Windows coverage is wanted again, `windows-latest` at
  ×2 on the weekly cron only is roughly 80 billed minutes a month — the per-push trigger, not
  the platform, was what made it expensive.
