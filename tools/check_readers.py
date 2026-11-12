#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Report whether this workstation transparently encrypts files, per reader.

A transparent-encryption client can store an extension as a
`%TSD-Header-###%` container and decrypt it for allow-listed processes only. The
same path then yields plaintext to `python` and ciphertext to PowerShell and to
the editor host that backs an agent's own read/search tools, so "zero matches"
and "mojibake" describe the *reader*, not the file.

This compares the two views of the same 16 bytes:

    trusted     this script's open(); `python` is allow-listed on this
                workstation (docs/agents/environment.md)
    untrusted   pwsh `Get-Content -AsByteStream`; PowerShell is not allow-listed,
                and neither is the host process behind an agent's file tools

Usage:
    python tools\\check_readers.py                  # probe one file per extension
    python tools\\check_readers.py --ext .c --ext .txt
    python tools\\check_readers.py --no-shell       # python view only, no pwsh
    python tools\\check_readers.py --show app/app.c # print trusted plaintext

Exit codes:
    0  plaintext everywhere: every reader sees the same bytes
    4  containers detected: the untrusted view is ciphertext, so its "no match"
       is void; read and search through python or `cmd /c type`
    5  `python` itself sees containers: the client's allow-list is broken, so no
       reader in this repo can be trusted -- fix the client first
    2  inconclusive: the untrusted view could not be taken (no pwsh, or it
       failed), and python alone cannot show containers because it is allowed
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

CONTAINER_MAGIC = b"%TSD-Header-"
PROBE_BYTES = 16

DEFAULT_EXTS = (".c", ".h", ".s", ".dts", ".uvprojx", ".md", ".py")

SKIP_DIRS = {
    ".git",
    "__pycache__",
    "third_party",
    "config_gen",
    "build_cfg",
    "Objects",
    "Listings",
}

READER = Path.home() / ".dsh" / "tools" / "readsrc.py"


def configure_output() -> None:
    """Print UTF-8 when captured; the console here is GBK."""
    if not sys.stdout.isatty():
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")


def trusted_head(path: Path) -> bytes:
    with open(path, "rb") as handle:
        return handle.read(PROBE_BYTES)


def sample_files(exts, per_ext: int):
    """One small, committed file per extension: the least ambiguous probe."""
    found: dict[str, list[Path]] = {ext: [] for ext in exts}
    for root, dirs, names in os.walk("."):
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
        for name in sorted(names):
            path = Path(root) / name
            ext = path.suffix.lower()
            if ext in found and len(found[ext]) < per_ext:
                try:
                    if path.stat().st_size:
                        found[ext].append(path)
                except OSError:
                    pass
    return [path for paths in found.values() for path in paths]


def untrusted_heads(paths, pwsh: str) -> dict[str, bytes]:
    """One pwsh process for all probes: `path|hex` per line."""
    quoted = ",".join("'" + str(p).replace("'", "''") + "'" for p in paths)
    script = (
        "$ErrorActionPreference='Stop';"
        f"foreach ($f in @({quoted})) {{"
        " try { $b = Get-Content -LiteralPath $f -AsByteStream"
        f" -TotalCount {PROBE_BYTES} -ErrorAction Stop }}"
        " catch { Write-Output \"$f|ERR\"; continue };"
        " $hex = ($b | ForEach-Object { $_.ToString('x2') }) -join '';"
        " Write-Output \"$f|$hex\" }"
    )
    completed = subprocess.run(
        [pwsh, "-NoProfile", "-NonInteractive", "-Command", script],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if completed.returncode != 0:
        raise RuntimeError((completed.stderr or "pwsh failed").strip())

    heads: dict[str, bytes] = {}
    for line in completed.stdout.splitlines():
        name, _, hex_digits = line.rpartition("|")
        if not name:
            continue
        if hex_digits == "ERR":
            raise RuntimeError(f"{name}: pwsh could not read it")
        heads[name] = bytes.fromhex(hex_digits) if hex_digits else b""
    return heads


def classify(trusted: bytes, untrusted: bytes | None) -> str:
    if trusted.startswith(CONTAINER_MAGIC):
        return "container(python)"
    if untrusted is None:
        return "plaintext"
    if untrusted.startswith(CONTAINER_MAGIC):
        return "container(pwsh)"
    width = min(len(trusted), len(untrusted))
    if untrusted[:width] != trusted[:width]:
        return "views differ"
    return "plaintext"


def show(path: Path) -> int:
    """Ground truth for comparing against a reader under suspicion.

    Newlines are normalised so the output lines up with the reader under test;
    the bytes themselves are what the trusted process sees.
    """
    data = path.read_bytes()
    if data.startswith(CONTAINER_MAGIC):
        print(f"{path}: encrypted container; python is not allow-listed here",
              file=sys.stderr)
        return 3
    text = data.decode("utf-8", errors="replace").replace("\r\n", "\n")
    sys.stdout.write(text.replace("\r", "\n"))
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ext", action="append", metavar="SUFFIX",
                        help="extension to probe (repeatable; default: "
                             + " ".join(DEFAULT_EXTS) + ")")
    parser.add_argument("--sample", type=int, default=1, metavar="N",
                        help="files per extension (default 1)")
    parser.add_argument("--no-shell", action="store_true",
                        help="skip the untrusted pwsh view")
    parser.add_argument("--show", metavar="PATH",
                        help="print one file through the trusted python view")
    args = parser.parse_args(argv)

    configure_output()
    if args.show:
        return show(Path(args.show))

    exts = tuple(dict.fromkeys(e.lower() for e in (args.ext or DEFAULT_EXTS)))
    paths = sample_files(exts, args.sample)
    if not paths:
        print("no files to probe; run from the repository root", file=sys.stderr)
        return 2

    heads = None
    if not args.no_shell:
        pwsh = os.environ.get("PWSH") or "pwsh"
        try:
            heads = untrusted_heads(paths, pwsh)
        except (OSError, RuntimeError) as exc:
            print(f"untrusted view unavailable: {exc}", file=sys.stderr)
            print("inconclusive: python is allow-listed, so its view cannot show "
                  "containers on its own.\nRun this where pwsh exists (or set "
                  "PWSH=<path>) rather than treating the result as a pass.",
                  file=sys.stderr)
            return 2

    def view(data: bytes | None) -> str:
        if data is None:
            return "-"
        if data.startswith(CONTAINER_MAGIC):
            return "ciphertext"
        if data and all(32 <= byte < 127 or byte in b"\t\r\n" for byte in data):
            return "plaintext"
        return "binary?"

    print(f"{'extension':10} {'python':11} {'pwsh':11} {'verdict':17} file")
    verdicts = []
    for path in paths:
        trusted = trusted_head(path)
        untrusted = None if heads is None else heads.get(str(path))
        verdict = classify(trusted, untrusted)
        verdicts.append(verdict)
        print(f"{path.suffix.lower():10} {view(trusted):11} {view(untrusted):11} "
              f"{verdict:17} {path.as_posix()}")

    if "container(python)" in verdicts:
        print("\npython itself sees a container, so it cannot read file content. "
              "If `cmd /c type <file>` shows plaintext the client exists but\n"
              "does not allow-list python; if that also shows the header the "
              "file is a bare container copied from another machine.\nNo repo "
              "reader can be trusted until this is resolved.", file=sys.stderr)
        return 5
    if any(v.startswith("container") or v == "views differ" for v in verdicts):
        print("\nencryption detected: the pwsh/editor view is ciphertext.")
        print("Read and search through python (or `cmd /c type`, `git show`); a "
              "match-free result from any other reader is void.")
        print(f"workstation reader: {'found' if READER.is_file() else 'MISSING'}"
              f" ({READER})")
        return 4

    if heads is None:
        print("\ninconclusive: only the python view was taken. python is "
              "allow-listed, so it sees plaintext even where other processes\n"
              "get containers -- a one-view run cannot rule encryption out. "
              "Re-run where pwsh exists (or set PWSH=<path>).",
              file=sys.stderr)
        return 2

    print("\nplaintext everywhere: every reader sees the same bytes, so a "
          "match-free result is a fact.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
