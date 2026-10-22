#!/usr/bin/env python3
"""Pre-commit check for C/C++ and C# files added in the staged commit.

Existing files are left alone; only newly added files must:
  - .h and .cs: contain the license header from license-header-checker/license.in
    (same rule as the CI check in .github/workflows/check_headers.yml)
  - .h, except *.custom.h fragments: contain `#pragma once`
  - C/C++: be UTF-8 without a BOM
"""

import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
LICENSE_DIR = REPO_ROOT / "shared" / "tools" / "license-header-checker"
LICENSE_FILE = LICENSE_DIR / "license.in"


def license_ignored(path):
    # Same semantics as the CI license check: an entry matches a sequence of path segments
    parts = Path(path).parts
    for line in (LICENSE_DIR / "ignore-filter.txt").read_text().splitlines():
        entry = Path(line.strip()).parts
        if entry and any(parts[i:i + len(entry)] == entry for i in range(len(parts) - len(entry) + 1)):
            return True
    return False


def staged_added_files():
    out = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=A", "-z"],
        capture_output=True, text=True, check=True, cwd=REPO_ROOT,
    ).stdout
    return set(filter(None, out.split("\0")))


def check(path, license_text):
    errors = []
    raw = (REPO_ROOT / path).read_bytes()
    if not path.endswith(".cs") and raw.startswith(b"\xef\xbb\xbf"):
        errors.append("starts with a UTF-8 BOM")
    text = raw.decode("utf-8-sig", errors="replace").replace("\r\n", "\n")
    # CI passes a file whose text contains the license anywhere, not necessarily at the top
    if path.endswith((".h", ".cs")) and not license_ignored(path) and license_text not in text:
        errors.append(f"missing license header (copy {LICENSE_FILE.relative_to(REPO_ROOT)})")
    if path.endswith(".h"):
        # *.custom.h are fragments that RTGen pastes into generated smart-pointer headers
        if not path.endswith(".custom.h") and "#pragma once" not in text:
            errors.append("missing `#pragma once`")
    return errors


def main(argv):
    added = staged_added_files()
    license_text = LICENSE_FILE.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
    failed = False
    for path in argv:
        if path not in added:
            continue
        for error in check(path, license_text):
            print(f"{path}: {error}")
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
