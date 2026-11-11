#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Regenerate the devicetree/Kconfig header snapshot for the f429igt6 board.

Requires a local Zephyr 4.5.0 tree plus CMake, Ninja and the Zephyr SDK.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

HEADERS = (
    "autoconf.h",
    "devicetree_generated.h",
    "version.h",
    "offsets.h",
    "syscall_list.h",
    "kobj-types-enum.h",
    "heap_constants.h",
    "otype-to-size.h",
    "otype-to-str.h",
    "syscall_dispatch.c",
    "syscall_exports_llext.c",
)


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd) if cwd else None, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--zephyr-base", default=os.environ.get("ZEPHYR_BASE", ""))
    parser.add_argument("--keil-root", default=r"D:\Keil_v5\ARM\ARMCLANG")
    parser.add_argument("--python", default=None,
                        help="Python interpreter used by Zephyr (defaults to the "
                             "venv next to the Zephyr tree)")
    parser.add_argument("--cmake", default=r"D:\Program Files\CMake\bin\cmake.exe")
    parser.add_argument("--board", default="f429igt6")
    args = parser.parse_args()

    repo = args.repo.resolve()
    zephyr_base = args.zephyr_base
    if not zephyr_base:
        # Convenience fallback for this workstation; override via env or arg.
        candidate = Path(r"D:\Zephyr\zephyrproject\zephyr")
        if (candidate / "VERSION").exists():
            zephyr_base = str(candidate)
    zephyr = Path(zephyr_base) if zephyr_base else None
    if not zephyr or not (zephyr / "VERSION").exists():
        print("Zephyr base not found; set ZEPHYR_BASE or pass --zephyr-base", file=sys.stderr)
        return 3

    build = repo / "build_cfg"
    out_dir = repo / "config_gen" / "4.5.0" / "zephyr"
    patch = repo / "tools" / "patches" / "0001-armclang-dts-preprocessor.patch"
    cmake = Path(args.cmake)

    python = args.python
    if not python:
        venv_python = zephyr.parent / ".venv" / (
            "Scripts/python.exe" if os.name == "nt" else "bin/python")
        python = str(venv_python) if venv_python.exists() else sys.executable

    env = os.environ.copy()
    env["ZEPHYR_BASE"] = str(zephyr)
    env["ARMCLANG_TOOLCHAIN_PATH"] = args.keil_root

    applied = False
    try:
        subprocess.run(["git", "-C", str(zephyr), "apply", "--check", str(patch)],
                       check=True, env=env)
        subprocess.run(["git", "-C", str(zephyr), "apply", str(patch)],
                       check=True, env=env)
        applied = True

        if build.exists():
            shutil.rmtree(build)

        run([
            str(cmake),
            "-B", build.as_posix(),
            "-S", (repo / "tools" / "config_gen_app").as_posix(),
            "-GNinja",
            f"-DBOARD={args.board}",
            f"-DBOARD_ROOT={repo.as_posix()}",
            f"-DPython3_EXECUTABLE={Path(python).as_posix()}",
            "-DZEPHYR_TOOLCHAIN_VARIANT=armclang",
            f"-DARMCLANG_TOOLCHAIN_PATH={Path(args.keil_root).as_posix()}",
        ])

        run([str(cmake), "--build", build.as_posix(), "--target",
             "version_h", "offsets_h", "syscall_list_h_target", "kobj_types_h_target",
             "heap_constants_h"])
    except subprocess.CalledProcessError as exc:
        print(f"configuration generation failed (exit {exc.returncode})", file=sys.stderr)
        return exc.returncode or 3
    finally:
        if applied:
            subprocess.run(["git", "-C", str(zephyr), "checkout", "--",
                            "cmake/compiler/armclang/generic.cmake"], check=False)

    generated = build / "zephyr" / "include" / "generated" / "zephyr"
    out_dir.mkdir(parents=True, exist_ok=True)
    for name in HEADERS:
        src = generated / name
        if src.exists():
            shutil.copyfile(src, out_dir / name)

    syscalls_src = generated / "syscalls"
    syscalls_dst = out_dir / "syscalls"
    if syscalls_src.is_dir():
        if syscalls_dst.exists():
            shutil.rmtree(syscalls_dst)
        shutil.copytree(syscalls_src, syscalls_dst)

    print(f"Snapshot written to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
