#!/usr/bin/env python3
"""Refuse two RNG draws of different ranges inside one argument list.

The order in which a call's arguments are evaluated is unspecified in C++, and GCC resolves it
per architecture: right to left on x86-64, left to right on aarch64. Two draws in one argument
list therefore run in opposite orders on the two platforms, and because rng::get_random()
re-parametrises its distribution per range, two draws of *different* ranges yield different
values -- not the same values in the other order. That is how one seed produced two different
worlds. See docs/decisions/0017.

Deliberately narrow, so that it can stay quiet enough to be believed:

  * one argument list, direct arguments only -- `f(draw(a), draw(b))` and `f(draw(a) - draw(b))`
    are the shapes that bit us, and both are caught;
  * different ranges only, because two draws of the same range are combined by + or - where
    swapping them cannot change the result;
  * `{ .x = draw(a), .y = draw(b) }` is NOT a call and is not flagged -- a braced initialiser
    list has been sequenced left to right since C++11;
  * `s << draw(a) << draw(b)` is not flagged either: `<<` has been sequenced since C++17.

What it does not see: two draws of different ranges combined outside any call, as in
`x - draw(a) - draw(b)`. Catching those would mean parsing C++ rather than scanning it.
"""
import re
import sys
from pathlib import Path

CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
DRAW = re.compile(r"\brng::[a-z_]+\s*\(")
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "catch"}
SKIP_DIRS = {".git", ".claude", "external", "build", "obj", "node_modules"}


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group().count("\n"), text, flags=re.S)
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)
    return re.sub(r"//[^\n]*", "", text)


def match_paren(text: str, open_idx: int) -> int:
    depth, i = 1, open_idx + 1
    while i < len(text) and depth:
        depth += (text[i] == "(") - (text[i] == ")")
        i += 1
    return i - 1


def draws_at_top_level(text: str, start: int, end: int):
    """Argument text of every rng:: call sitting directly inside (start, end).

    Braces are counted as well as parentheses, so that draws inside a lambda body passed as an
    argument are not mistaken for arguments of the call itself -- they are sequenced by the
    statements around them, not by the argument list.
    """
    found, i, parens, braces = [], start, 0, 0
    while i < end:
        m = DRAW.match(text, i)
        if m and parens == 0 and braces == 0:
            close = match_paren(text, m.end() - 1)
            found.append(re.sub(r"\s+", "", text[m.end():close]))
            i = close + 1
            continue
        parens += (text[i] == "(") - (text[i] == ")")
        braces += (text[i] == "{") - (text[i] == "}")
        i += 1
    return found


def check(path: Path):
    text = strip_comments(path.read_text(errors="replace"))
    out = []
    for m in CALL.finditer(text):
        if m.group(1) in KEYWORDS or DRAW.match(text, m.start()):
            continue
        close = match_paren(text, m.end() - 1)
        args = draws_at_top_level(text, m.end(), close)
        if len(args) > 1 and len(set(args)) > 1:
            out.append((text.count("\n", 0, m.start()) + 1, m.group(1), sorted(set(args))))
    return out


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    bad = 0
    for path in sorted(root.rglob("*.cpp")):
        if SKIP_DIRS & set(path.parts):
            continue
        for line, call, args in check(path):
            bad += 1
            print(f"::error file={path},line={line}::{call}() takes two rng draws of different "
                  f"ranges ({', '.join(args)}) in one argument list. The evaluation order is "
                  f"unspecified and differs by architecture -- draw them into sequenced locals "
                  f"first, x86-64 order (rightmost first). See docs/decisions/0017.")
    if bad:
        print(f"\n{bad} unsequenced draw pair(s).")
        return 1
    print("No unsequenced RNG draw pairs in an argument list.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
