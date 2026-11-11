#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Read or search source files through an allow-listed process.

Examples:
    python tools/read_source.py third_party/zephyr/kernel/device.c --head 40
    python tools/read_source.py third_party/zephyr/include/zephyr/kernel.h --grep "z_timeout"
    python tools/read_source.py src/main.c -n
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from source_io import EncryptedContentError, read_text  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--head", type=int, default=None, metavar="N")
    parser.add_argument("--tail", type=int, default=None, metavar="N")
    parser.add_argument("--grep", default=None, metavar="PATTERN")
    parser.add_argument("-i", "--ignore-case", action="store_true")
    parser.add_argument("-n", "--line-numbers", action="store_true")
    args = parser.parse_args()

    flags = re.IGNORECASE if args.ignore_case else 0
    pattern = re.compile(args.grep, flags) if args.grep else None
    multiple = len(args.paths) > 1
    status = 0

    for path in args.paths:
        try:
            text = read_text(path)
        except EncryptedContentError as exc:
            print(str(exc), file=sys.stderr)
            status = 3
            continue
        except OSError as exc:
            print(f"{path}: {exc}", file=sys.stderr)
            status = 3
            continue

        lines = text.splitlines()
        if multiple:
            print(f"=== {path}")

        if pattern:
            for number, line in enumerate(lines, 1):
                if pattern.search(line):
                    print(f"{path}:{number}:{line}" if multiple else f"{number}:{line}")
            continue

        if args.head is not None:
            lines = lines[:args.head]
        elif args.tail is not None:
            lines = lines[-args.tail:]

        for number, line in enumerate(lines, 1):
            if args.line_numbers:
                print(f"{number:5}: {line}")
            else:
                print(line)

    return status


if __name__ == "__main__":
    raise SystemExit(main())
