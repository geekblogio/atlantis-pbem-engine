# 0007 — Build at `-O2`, with `genrules.cpp` exempt

**Status:** accepted, 2026-08-05.

## Context

Upstream ships unoptimised builds — `CFLAGS = -g …`, and CMake with no build type, which passes
no optimisation flag at all. The simulation consumer runs thousands of turns and is
engine-bound.

## Decision

`-O2 -g` in both build systems. `genrules.cpp` is compiled at `-O0` regardless. CMake defaults
to `RelWithDebInfo` when no build type is given, and the cmake CI job configures
`RelWithDebInfo` rather than `Debug`.

## Why

Measured on gcc 13.3.0, replaying `neworigins` `turn_10`, mean of three runs:

| | per turn |
| --- | --- |
| `-O0` | 0.217 s |
| `-O2` | 0.041 s |

**5.3×.** Not a marginal setting for the workload this engine actually carries.

## Why `genrules.cpp` is exempt

It is one enormous function that gcc's optimiser scales badly on, and gcc says so —
*variable tracking size limit exceeded*:

| | `-O0` | `-O2` |
| --- | --- | --- |
| `genrules.cpp` | 6.6 s, 0.76 GB | 119.9 s, 1.81 GB |
| `aregion.cpp`, for scale | 4.8 s, 0.57 GB | 8.8 s, 0.69 GB |

The rules generator runs once per ruleset release. Its own speed is irrelevant; two extra
minutes on every clean build is not.

`-O0` is **appended** rather than `-O2` removed, so the exemption survives a `CFLAGS=` override
from the command line.

## Consequences

- Snapshot output is unchanged — verified byte for byte, which is the evidence that the
  optimiser does not alter engine behaviour.
- **`RelWithDebInfo` adds `-DNDEBUG`, which the Makefile does not.** `assert()` disappears, and a
  variable that existed only to feed one becomes unused under `-Werror`. A clean `make all` is
  therefore no longer proof that the cmake job passes. Documented in
  [../build-and-test.md](../build-and-test.md); it cost one red pipeline to learn.
- Cold CI builds are slower. Warm ccache builds are not.
- Upstream may not want this default. The `genrules` pathology is worth reporting to them
  regardless of which default anyone prefers.
