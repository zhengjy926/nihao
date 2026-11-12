#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Search source files without tripping over the encryption containers.

`.c`, `.s`, `.uvprojx` and some `.txt` files are transparent-encryption
containers on this workstation: PowerShell's Select-String reports zero matches
on them without an error, so a grep that "finds nothing" may simply have read
ciphertext. This walks the tree and decodes every file through the
workstation's reader, so no match means there really is no match.

The traversal skips the bulk trees by default -- `third_party/`, `config_gen/`,
`build_cfg/` -- because almost every question about *this* project is answered
in `app/`, `services/`, `port/`, `boards/`, `tools/` or `docs/`. Paths given on
the command line are always searched, whatever they are called.

Usage:
    find_in_sources <pattern> [paths...]
    find_in_sources --all --include "*.c" "LOG_BACKEND_UART" .

Exit codes: 0 one or more matches, 1 no match, 3 a file could not be read.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

SKIP_DIRS = {
    ".git",
    "__pycache__",
    "third_party",
    "config_gen",
    "build_cfg",
    "Objects",
    "Listings",
}

# Sources and docs are small; anything bigger is a log, a map or an artifact.
MAX_BYTES = 4 << 20


def load_reader():
    """Import the workstation reader, or None where plain reading is enough."""
    candidates = []
    override = os.environ.get("READSRC_DIR")
    if override:
        candidates.append(Path(override))
    candidates.append(Path.home() / ".dsh" / "tools")
    for directory in candidates:
        if (directory / "readsrc.py").is_file():
            sys.path.insert(0, str(directory))
            import readsrc

            return readsrc
    return None


def configure_output() -> None:
    """Print UTF-8 when captured, so Chinese source lines survive the pipe.

    This workstation's console code page is GBK, so Python picks `gbk` for
    stdout even when the output is being captured; whatever captures it then
    decodes UTF-8. A terminal still gets the console's own code page.
    """
    if not sys.stdout.isatty():
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")


class NotText(Exception):
    """Not a file worth searching: too big, binary, or a container."""


def decode_file(path: Path, reader) -> str:
    data = path.read_bytes()
    if len(data) > MAX_BYTES:
        raise NotText("too big")
    if b"\x00" in data[:4096]:
        raise NotText("binary")
    if reader is None:
        return data.decode("utf-8", errors="replace")
    if data.startswith(reader.MAGIC):
        raise NotText("container is still encrypted for this reader")
    return reader.decode(data, None).text


def iter_files(paths, search_skipped: bool, include: str | None):
    for argument in paths:
        target = Path(argument)
        if target.is_file():
            yield target
            continue
        for root, dirs, names in os.walk(target):
            if not search_skipped:
                dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for name in sorted(names):
                candidate = Path(root) / name
                if include and not candidate.match(include):
                    continue
                yield candidate


def display(path: Path) -> str:
    try:
        return path.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pattern", help="regular expression")
    parser.add_argument("paths", nargs="*", default=["."])
    parser.add_argument("-i", "--ignore-case", action="store_true")
    parser.add_argument("--include", metavar="GLOB",
                        help="only files matching GLOB, e.g. '*.c'")
    parser.add_argument("--all", action="store_true",
                        help="also search third_party/, config_gen/ and build_cfg/")
    parser.add_argument("--max", type=int, default=200, metavar="N",
                        help="stop after N matches (default 200, 0 = no limit)")
    args = parser.parse_args(argv)

    configure_output()
    reader = load_reader()
    if reader is None:
        print("note: workstation reader not found; reading files as plain text",
              file=sys.stderr)

    pattern = re.compile(args.pattern, re.I if args.ignore_case else 0)
    matches = 0
    unreadable = 0
    truncated = False

    roots = []
    for argument in args.paths:
        if Path(argument).exists():
            roots.append(argument)
        else:
            print(f"{argument}: no such file or directory", file=sys.stderr)
            unreadable += 1

    for path in iter_files(roots, args.all, args.include):
        try:
            text = decode_file(path, reader)
        except NotText:
            continue
        except (OSError, ValueError, LookupError) as exc:
            print(f"{display(path)}: {exc}", file=sys.stderr)
            unreadable += 1
            continue

        for number, line in enumerate(text.splitlines(), 1):
            if not pattern.search(line):
                continue
            matches += 1
            print(f"{display(path)}:{number}:{line}")
            if args.max and matches >= args.max:
                truncated = True
                break
        if truncated:
            break

    if truncated:
        print(f"note: stopped at {args.max} matches; raise with --max", file=sys.stderr)
    if matches == 0:
        print("no match", file=sys.stderr)
        return 3 if unreadable else 1
    return 3 if unreadable else 0


if __name__ == "__main__":
    raise SystemExit(main())
