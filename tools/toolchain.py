# SPDX-License-Identifier: Apache-2.0
"""Locate host tools with a machine-independent search order.

Every tool is resolved as:

1. an explicit argument (``--cmake``, ``--uv4``, ...)
2. an environment variable (``CMAKE``, ``UV4``, ``ARMCLANG_TOOLCHAIN_PATH``)
3. ``shutil.which`` on ``PATH``
4. a list of known install locations

This keeps one checkout usable on machines whose vendors installed the
toolchain in different directories (for example ``D:\\DevTools\\CMake`` at
home and ``D:\\Program Files\\CMake`` at the office).
"""

from __future__ import annotations

import os
import shutil
from pathlib import Path


def find_tool(explicit: str | None, name: str, env_var: str,
              candidates: tuple[str, ...] = ()) -> str:
    """Return the first usable path for a host tool, or "" if none is found."""
    if explicit:
        return explicit
    if env_var:
        value = os.environ.get(env_var)
        if value:
            return value
    if name:
        found = shutil.which(name)
        if found:
            return found
    for candidate in candidates:
        if Path(candidate).exists():
            return candidate
    return ""
