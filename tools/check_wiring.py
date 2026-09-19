#!/usr/bin/env python3
"""jsonTools wiring check — the `wire` stage of tools/ci.sh.

Adding a tool touches several files at once: src/jt_<verb>.cpp (the binary),
CMakeLists.txt (the build list AND the install list), tests/CMakeLists.txt (the
binary the mains test spawns), a test that exercises it, and README.md (the
authoritative verb reference). Forgetting one of them is silent — the build still
succeeds, the tool is simply never installed, never tested, or never documented —
which is exactly the kind of thing that gets noticed months later, by a user.

This script asserts the whole set, so a new tool cannot half-land. It reads the
repo and changes nothing. Exit status: 0 = every tool fully wired.

Usage: tools/check_wiring.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:  # a missing file is a wiring failure, not a crash
        return f"__MISSING__ {exc}"


def cmake_tool_lists(text: str) -> list[list[str]]:
    """Every `foreach(... IN ITEMS A B C)` list in CMakeLists.txt.

    The build list and the install list are separate literals, so a tool added to
    one and not the other builds but never installs.
    """
    lists = []
    for match in re.finditer(r"IN ITEMS\b(.*?)(?:\)|\n\s*\n)", text, re.S):
        chunk = match.group(1).replace("\\", " ")
        lists.append([tok for tok in re.split(r"[\s()]+", chunk) if tok.isidentifier()])
    return lists


def main() -> int:
    cmake = read(ROOT / "CMakeLists.txt")
    tcmake = read(ROOT / "tests" / "CMakeLists.txt")
    readme = read(ROOT / "README.md")
    mains = read(ROOT / "tests" / "test_mains.cpp")

    tools = sorted(p.stem[3:] for p in (ROOT / "src").glob("jt_*.cpp"))

    lists = cmake_tool_lists(cmake)
    build_list = {t.lower() for t in (lists[0] if lists else [])}
    install_list = {t.lower() for t in (lists[-1] if lists else [])}

    dep_block = re.search(r"add_dependencies\(jt_tests\s*(.*?)\)", tcmake, re.S)
    deps = {t[2:].lower() for t in re.split(r"\s+", dep_block.group(1) if dep_block else "") if t.startswith("jt")}

    test_files = {m.group(1) for m in re.finditer(r"^\s+(test_\w+)\.cpp", tcmake, re.M)}

    if not tools:
        print("wire: no src/jt_*.cpp sources found — refusing to report success")
        return 1

    print(f"wire: {len(tools)} tools checked (verb, build list, install list, test deps, README, exercised)")
    gaps: list[str] = []
    for verb in tools:
        binary = f"jt{verb.capitalize()}"  # the binary is jtNew, the source jt_new.cpp
        spawned = re.search(rf"\b{binary}\b", mains) is not None
        covered = f"test_{verb}" in test_files or spawned
        row = {
            "build list": verb in build_list,
            "install list": verb in install_list,
            "test deps": verb in deps,
            "exercised": covered,
            "README": re.search(rf"\b{binary}\b", readme) is not None,
        }
        missing = [name for name, ok in row.items() if not ok]
        marks = "  ".join(f"{name}={'yes' if ok else 'NO'}" for name, ok in row.items())
        print(f"  {binary:<10} {marks}")
        for name in missing:
            gaps.append(f"{binary}: missing from {name}")

    # The other direction: a name in the lists with no source file behind it.
    for stale in sorted((build_list | install_list | deps) - {t.lower() for t in tools}):
        gaps.append(f"jt{stale}: named in CMake/tests but src/jt_{stale}.cpp does not exist")

    if gaps:
        print("\nwire: gaps")
        for gap in gaps:
            print(f"  {gap}")
        return 1

    print("wire: every tool is built, installed, depended on by the tests, exercised, and documented")
    return 0


if __name__ == "__main__":
    sys.exit(main())
