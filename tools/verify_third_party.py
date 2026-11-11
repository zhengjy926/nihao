#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify that every file under third_party/ is byte-identical to upstream.

Checks both directions:
  * content: sha256(vendored) == sha256(upstream) for every file the manifest lists
  * membership: no missing files, and no extra files inside the copied directories

Must run under a process the endpoint-encryption client allows to read plaintext
(python.exe is one). Exits 0 when everything matches, 1 otherwise.
"""

from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

from sync_upstream import MODULE_DIRS, MODULE_FILES, ZEPHYR_DIRS, ZEPHYR_FILES


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def resolve_upstream() -> tuple[Path, Path]:
    zephyr_base = os.environ.get("ZEPHYR_BASE", "")
    if not zephyr_base:
        candidate = Path(r"D:\Zephyr\zephyrproject\zephyr")
        if (candidate / "VERSION").exists():
            zephyr_base = str(candidate)
    if not zephyr_base or not (Path(zephyr_base) / "VERSION").exists():
        raise SystemExit("ZEPHYR_BASE is not set or is not a Zephyr tree")
    zephyr = Path(zephyr_base)
    modules = Path(os.environ.get("ZEPHYR_MODULES", "") or zephyr.parent / "modules")
    return zephyr, modules


def rel_files(root: Path) -> set[str]:
    return {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()}


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    tp = repo / "third_party"
    zephyr, modules = resolve_upstream()

    problems: list[str] = []
    checked: set[Path] = set()

    def compare(up: Path, local: Path) -> None:
        if not up.is_file():
            problems.append(f"missing upstream: {up}")
            return
        if not local.is_file():
            problems.append(f"missing vendored: {local}")
            return
        checked.add(local)
        if sha256(up) != sha256(local):
            problems.append(f"content differs: {local.relative_to(tp).as_posix()}")

    for rel in ZEPHYR_DIRS:
        src, dst = zephyr / rel, tp / "zephyr" / rel
        if not src.is_dir() or not dst.is_dir():
            problems.append(f"missing directory: {rel}")
            continue
        up_set, local_set = rel_files(src), rel_files(dst)
        for name in sorted(up_set & local_set):
            compare(src / name, dst / name)
        for name in sorted(up_set - local_set):
            problems.append(f"missing vendored file: zephyr/{rel}/{name}")
        for name in sorted(local_set - up_set):
            problems.append(f"extra file: zephyr/{rel}/{name}")

    for rel in ZEPHYR_FILES:
        compare(zephyr / rel, tp / "zephyr" / rel)

    for rel in MODULE_DIRS:
        src, dst = modules / rel, tp / "modules" / rel
        if not src.is_dir() or not dst.is_dir():
            problems.append(f"missing module directory: {rel}")
            continue
        up_set, local_set = rel_files(src), rel_files(dst)
        for name in sorted(up_set & local_set):
            compare(src / name, dst / name)
        for name in sorted(up_set - local_set):
            problems.append(f"missing vendored file: modules/{rel}/{name}")
        for name in sorted(local_set - up_set):
            problems.append(f"extra file: modules/{rel}/{name}")

    for rel in MODULE_FILES:
        compare(modules / rel, tp / "modules" / rel)

    for item in problems:
        print(item)
    print(f"checked {len(checked)} files, {len(problems)} problem(s)")
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
