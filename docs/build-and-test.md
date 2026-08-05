# Building and testing

> **Audience:** anyone building the engine or diagnosing a failed build.
> **Provenance:** upstream-friendly.

**Read this when** it is your first build on a machine, you added or removed a source file, or
CI failed at the compile step.

## Prerequisites

A C++20 toolchain with **complete ranges support**. `CompilerSupport.cmake` probes for it with
a `std::views::split` test compile and hard-fails the configure step otherwise. GCC 12 and
later, and recent Clang, are fine.

Nothing else is required. The two third-party libraries are vendored as single headers in
`external/`.

## Two build systems

Both are supported and both are exercised by CI. They produce binaries in **different places**,
which matters because the snapshot runners look in both.

### Makefile — output `<game>/<game>`

```bash
make all             # all six rulesets plus the unit test binary
make GAME=standard   # one ruleset
make unittest        # the unit test binary only
make all-clean
```

**Never pass `-j`.** All six sub-makes write the same `obj/*.o`, and the `objdir` target races
the compiles. The build will fail or, worse, link stale objects. Parallelism belongs to the
CMake path.

### CMake — output `build/<game>` and `build/unittest`

```bash
cmake -B build -S .                       # defaults to RelWithDebInfo
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

CMake generates correct dependencies, so `-j` is safe here. The shorthand scripts do the same
thing without the parallelism: `./s/configure`, `./s/build`, `./s/runtests`.

Pass `-DCMAKE_BUILD_TYPE=Debug` for an unoptimised build. Without any build type CMake would
pass no optimisation flag at all, so the project sets `RelWithDebInfo` to match the Makefile.

CMake builds the engine once as a static library and links it into all seven executables. It
also runs `genrules` for every ruleset as part of the default target, which is a free check
that the rulebook generator still works.

### Both, always

Adding a source file means registering it in **`CMakeLists.txt` and `Makefile`**. Nothing
detects a mismatch except CI, which builds both paths for exactly this reason.

## Optimisation, and the one file exempt from it

Both build systems compile at **`-O2 -g`** by default. The engine is CPU-bound during a turn
and the downstream simulation runs thousands of them, so this is not a marginal setting.

**`genrules.cpp` is compiled at `-O0` regardless.** It is a single enormous function that GCC's
optimiser scales badly on — measured on GCC 13.3.0:

| Translation unit | `-O0` | `-O2` |
| --- | --- | --- |
| `genrules.cpp` | 6.6 s, 0.76 GB | **119.9 s, 1.81 GB** |
| `aregion.cpp`, for comparison | 4.8 s, 0.57 GB | 8.8 s, 0.69 GB |

GCC says so itself: *variable tracking size limit exceeded*. The rulebook generator runs once
per ruleset release, so its own speed is irrelevant.

The Makefile **appends** `-O0` to `CFLAGS` for that one object rather than removing `-O2`, so
the exemption survives a `CFLAGS=` override on the command line. CMake does the same through
`set_source_files_properties(... COMPILE_OPTIONS)`, which lands after the build type's flags.

## Warnings are errors

GCC and Clang build with `-Wextra -Wall -Werror -pedantic`. Any new diagnostic is a hard
failure.

**The two build systems do not warn identically.** CMake's `RelWithDebInfo` adds `-DNDEBUG`,
which the Makefile does not, so `assert()` disappears and anything that existed only to feed one
becomes an unused variable. A clean `make all` is therefore not proof that the cmake job will
pass. The cmake CI job is the authority.

**MSVC is materially laxer**: `/WX` is disabled and `4244`, `4267` and `4700` are suppressed.
Code that is clean under MSVC can still fail CI. If you develop on Windows, build once under
GCC before opening a pull request.

The runner images used by the merge gate are pinned, so a new compiler cannot break the gate on
its own. The `Platforms` workflow deliberately runs unpinned, weekly, to find that drift early.

## Unit tests

boost.ut suites in `unittest/*_test.cpp`, registered by glob (CMake) and wildcard (Makefile) —
a new `foo_test.cpp` needs no build-file change.

```bash
make unittest && ./unittest/unittest
```

They link against a **dedicated minimal ruleset** in `unittest/` (`rules.cpp`, `world.cpp`,
`map.cpp`, `extra.cpp`, `monsters.cpp`): a 2×4 surface plus a tiny underworld, not the standard
world. This keeps the tests fast and their expectations stable.

`UnitTestHelper` (`unittest/testhelper.hpp`) is declared a `friend class` of `Game`, which is
how tests reach private turn phases. It also captures `logger` output and seeds the RNG
deterministically. **Prefer adding a helper method there over widening `Game`'s public API.**

### Selecting a single test

You cannot, on GCC or Clang. `unittest/main.cpp` calls `ut::cfg<>.run()` without forwarding
`argv`, and boost.ut only picks up arguments implicitly on MSVC via `__argc`. To narrow a run,
comment out suites temporarily.

## Vendored libraries

`external/boost/ut.hpp` and `external/nlohmann/json.hpp` are committed single-header libraries
with no manifest. `make check-libraries` refreshes them from the latest upstream release —
despite the name it **overwrites in place** rather than reporting.

Because they carry no manifest, Dependabot cannot see them. The `Vendored Deps` workflow runs
that target monthly and opens a pull request when something moved.

## When the build fails

| Symptom | Cause |
| --- | --- |
| `Compiler lacks full support for C++20 ranges` | toolchain too old; `CompilerSupport.cmake` refused |
| undefined reference to a member defined in a `.cpp` | an `inline` on an out-of-line definition — legal only while nothing optimises it away |
| link succeeds locally, fails in CI | a source file registered in only one of the two build files |
| `unused variable` under CMake but not under `make` | `RelWithDebInfo` adds `-DNDEBUG`, which compiles `assert()` away; a variable that exists only for an assertion needs `[[maybe_unused]]` |
| a warning you did not introduce | a newer compiler; check whether `Platforms` is already red |
| stale or half-linked objects after an interrupted `make` | `make all-clean`; and check you did not use `-j` |
