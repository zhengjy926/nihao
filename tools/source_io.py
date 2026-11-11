# SPDX-License-Identifier: Apache-2.0
"""File access helpers that fail loudly on encrypted containers.

The workstation's endpoint encryption client stores some file types as
`%TSD-Header-###%` containers and only decrypts them for allow-listed
processes. Python is allow-listed, so reading through this module returns
plaintext; if a container still comes back (for example a preprocessed file
written by armclang), the error below says what to do instead of raising a
cryptic UnicodeDecodeError.
"""

from __future__ import annotations

from pathlib import Path
from typing import Union

MAGIC = b"%TSD-Header-###%"

HINT = (
    "The file content is a transparent-encryption container.\n"
    "  Read it with an allow-listed tool: python (tools/read_source.py), "
    "cmd /c type, ripgrep, git, or the Keil toolchain.\n"
    "  Do not use PowerShell Get-Content/Select-String; they return ciphertext "
    "(and Select-String matches nothing without reporting an error).\n"
    "  See docs/agents/environment.md."
)


class EncryptedContentError(RuntimeError):
    """Raised when a read returns an encryption container instead of text."""


def read_bytes(path: Union[str, Path]) -> bytes:
    data = Path(path).read_bytes()
    if data.startswith(MAGIC):
        raise EncryptedContentError(f"{path}: {HINT}")
    return data


def read_text(path: Union[str, Path], encoding: str = "utf-8",
              errors: str = "replace") -> str:
    return read_bytes(path).decode(encoding, errors)
