<!--
Describe what changed and why. Write for someone who has the diff but not the context:
what was wrong, why this is the right fix, what you measured or ruled out.
-->

## Checklist

<!-- Delete the lines that do not apply. Leaving one unticked is fine; silently
     dropping it is not. -->

- [ ] Build is warning-clean (`-Werror` on GCC/Clang) and unit tests pass
- [ ] Snapshot tests pass, **or** fixtures were re-recorded deliberately (fill in the section
      below) and `git clean -xfd snapshot-tests` removed the `turn_*.bak` directories
- [ ] Documentation invalidated by this change is updated **in this pull request**
- [ ] Visible outside the binary (CLI, file names, report shape)? → `docs/interface/` updated and
      `JSON_REPORT_VERSION` / `CURRENT_ATL_VER` in `game.h` considered
- [ ] Breaks a downstream consumer? → `docs/fork/downstream-consumers.md` updated and an issue
      opened in the consumer repository
- [ ] New source file? → registered in **both** `Makefile` and `CMakeLists.txt`
- [ ] Registered in `docs/fork/patches.md`, in an entry naming this pull request's `#number` —
      the `Divergence Register` job fails until it is
- [ ] On an `upstream/*` branch? → branched from `upstream/master`, no fork-local paths touched,
      commit written in upstream's voice

## Snapshot diff summary

<!-- Required only if fixtures changed. Which files moved, what the difference is, and why it
     is the intended consequence of this change rather than a side effect. -->

_Not applicable._
