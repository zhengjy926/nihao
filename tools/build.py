#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build mdk/nihao.uvprojx with uVision in batch mode and print the log."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from toolchain import find_tool  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--uv4", default=None,
                        help="uVision UV4 executable (default: auto-detect)")
    parser.add_argument("--project", type=Path, default=None)
    parser.add_argument("--log", type=Path, default=None)
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    mdk = args.repo / "mdk"
    project = args.project or (mdk / "nihao.uvprojx")
    log = args.log or (mdk / "build.log")
    uv4 = Path(find_tool(
        args.uv4, "UV4", "UV4",
        (r"D:\Keil_v5\UV4\UV4.exe", r"C:\Keil_v5\UV4\UV4.exe")))

    if not uv4.exists():
        print(f"uVision not found: {uv4}", file=sys.stderr)
        return 3
    if not project.exists():
        print(f"project not found: {project} (run tools/gen_project.py)", file=sys.stderr)
        return 3

    if log.exists():
        log.unlink()

    started = time.time()
    proc = subprocess.run(
        [str(uv4), "-b", str(project), "-j0", "-o", str(log)],
        cwd=str(mdk),
        capture_output=True,
        text=True,
        errors="replace",
    )

    # uVision can forward a batch request to an already running instance and
    # return early; wait until the log it was asked to write actually appears.
    deadline = started + args.timeout
    while time.time() < deadline:
        if log.exists() and log.stat().st_mtime >= started - 1:
            break
        time.sleep(0.5)

    if not log.exists():
        print("uVision produced no log; close the running uVision instance and retry.",
              file=sys.stderr)
        if proc.stdout:
            print(proc.stdout, file=sys.stderr)
        if proc.stderr:
            print(proc.stderr, file=sys.stderr)
        return 3

    print(log.read_text(encoding="utf-8", errors="replace"))
    print(f"uVision exit code: {proc.returncode}")
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())
