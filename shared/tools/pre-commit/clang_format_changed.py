#!/usr/bin/env python3
"""Pre-commit hook: run git-clang-format on the staged lines of the given files.

Files whose committed version is indented mostly with tabs keep tabs
(UseTab: AlignWithSpaces), so a change never mixes tabs and spaces in one file.
Everything else, including new files, uses the spaces from .clang-format.
"""

import subprocess
import sys

TAB_STYLE = "{BasedOnStyle: InheritParentConfig, UseTab: AlignWithSpaces}"
EXTENSIONS = "c,cc,cpp,cxx,h,hpp"


def uses_tabs(path):
    committed = subprocess.run(["git", "show", f"HEAD:{path}"], capture_output=True, text=True, errors="replace")
    if committed.returncode != 0:  # new file
        return False
    lines = committed.stdout.splitlines()
    tabs = sum(1 for line in lines if line.startswith("\t"))
    spaces = sum(1 for line in lines if line.startswith("    "))
    return tabs > spaces


def git_clang_format(files, style=None):
    if not files:
        return 0
    cmd = ["git-clang-format", "--binary", "clang-format", "--extensions", EXTENSIONS]
    if style:
        cmd += ["--style", style]
    return subprocess.run(cmd + ["HEAD", "--"] + files).returncode


def main(files):
    tab_files = [f for f in files if uses_tabs(f)]
    space_files = [f for f in files if f not in tab_files]
    results = [git_clang_format(space_files), git_clang_format(tab_files, TAB_STYLE)]
    return max(results)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
