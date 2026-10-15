#!/usr/bin/env python3
"""Fails on C that 816-tcc is known to miscompile (make check).

`a || !b`: when `a` is true, 816-tcc jumps as if the whole condition were
false. Every `||` whose right operand starts with `!` is wrong on the SNES
(in an `if`, a ternary, or nested), while the host tests, built with a
normal compiler, pass. Write `b == 0`, or split the test.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OR_NOT = re.compile(r"\|\|\s*!(?!=)")


def strip_comments(text):
    """Comments blanked out, line breaks kept (for line numbers)."""
    return re.sub(r"/\*.*?\*/|//[^\n]*",
                  lambda m: re.sub(r"[^\n]", " ", m.group()), text, flags=re.S)


def main():
    problems = []
    for path in sorted((ROOT / "src").glob("*.c")):
        text = path.read_text(encoding="utf-8")
        code = strip_comments(text)
        for match in OR_NOT.finditer(code):
            line = code.count("\n", 0, match.start()) + 1
            problems.append(f"{path.relative_to(ROOT)}:{line}: "
                            f"{text.splitlines()[line - 1].strip()}")
    if problems:
        print("816-tcc miscompiles `|| !` (write `x == 0` or split the test):")
        print("\n".join("  " + p for p in problems))
        sys.exit(1)
    print("816-tcc pitfalls: none")


if __name__ == "__main__":
    main()
