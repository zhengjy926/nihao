# Environment facts

Things about this workstation that are invisible in the code but decide whether
tooling works. Read this before changing anything under `tools/`.

## Transparent file encryption

An endpoint encryption client protects some file types. Files are stored as
containers whose first bytes are `%TSD-Header-###%`; the client decrypts them
transparently for allow-listed processes only.

Observed reader matrix (same file, same user):

| Reader | Result |
| --- | --- |
| `python.exe` (also `tools/read_source.py`) | plaintext |
| `cmd.exe` (`type`) | plaintext |
| `ripgrep.exe` | plaintext |
| `git` (including `hash-object`, `diff --no-index`) | plaintext |
| Keil toolchain: `UV4.exe`, `armclang`, `armasm`, `armlink`, `fromelf` | plaintext |
| `pwsh.exe` / .NET: `Get-Content`, `Select-String`, `[IO.File]::ReadAllText` | `%TSD-Header-###%` |
| VS Code extension host (the agent's `view` tool) | `%TSD-Header-###%` |

Observed per extension (repo scan with an untrusted reader):

- encrypted on disk: `.c`, `.uvprojx`, some `.txt` written by trusted tools
- plaintext: `.h`, `.s`, `.dts`, `.sct`, `.ps1`/`.py`, `.md`

Writer side: files written by a trusted process (`python`, `armclang`, `uVision`)
land encrypted when their extension is protected; files written by an untrusted
process (PowerShell, the VS Code host) land as plaintext. Either way **git reads
plaintext**, so committed blobs and `git diff` are portable.

### Rules

1. Read file content with Python (`tools/read_source.py`), never with
   PowerShell. `Select-String` on a container returns zero matches without
   error, which silently produces wrong conclusions.
2. `tools/source_io.py` rejects `%TSD-Header-###%` input with an explicit error; use
   `read_text()` for any content read in new scripts.
3. Do not "fix" a container by rewriting it byte-for-byte; let the owning tool
   write it.

## Arm Compiler 6.23 and devicetree preprocessing

`armclang -E -o <file>` produces a `%TSD-Header-###%` container, so
python-devicetree cannot parse the preprocessed DTS. `tools/gen_config.py`
applies `tools/patches/0001-armclang-dts-preprocessor.patch` (Zephyr SDK GCC as
the DTS preprocessor), generates the snapshot, then reverts the patch in a
`finally` block. The patch never affects the Keil build.

## Configuration must be generated with the armclang variant

`-DZEPHYR_TOOLCHAIN_VARIANT=armclang` selects Zephyr's `armstdc` libc glue. An
`autoconf.h` generated for GCC/picolibc makes the drivers fail on `errno`.
`tools/gen_config.py` always uses the armclang variant.

## Python interpreter

Zephyr's CMake needs the venv next to the Zephyr tree
(`<zephyr>/../.venv/Scripts/python.exe`). The system Python lacks Zephyr's
dependencies and configuration fails. `gen_config.py` picks the venv
automatically; override with `--python`.

## uVision batch builds

`UV4.exe -b project.uvprojx -j0 -o build.log` can return before the log is
written when another uVision instance is running. `tools/build.py` deletes the
log first, waits for it to be refreshed (default 120 s), prints it, and returns
uVision's exit code. Exit code 1 means warnings, 2 means errors.

## Target board

ALIENTEK Explorer, STM32F429IGT6, 25 MHz HSE, 180 MHz SYSCLK. **BOOT0 must be
0**: with BOOT0 high the chip runs the ST ROM bootloader and the application
never starts, even though the image is present at `0x08000000`.

## Compiler flags

Keil optimisation must stay at `-O1` or higher. At `-O0`, armclang emits
out-of-line copies of unused `static inline` functions whose `extern`
declarations come from the generated `syscalls/*.h` headers; those copies
reference kernel symbols that this bare-metal port does not provide
(`z_timeout_expires`, ...).
