# Environment facts

Things about this workstation that are invisible in the code but decide whether
tooling works. Read this before changing anything under `tools/`.

> **Scope.** This file describes the *development machine*, not the project. The
> encryption client, the GBK console, the Keil pack layout and the `D:\Zephyr`
> tree exist only here; on another machine, skip the matching sections and keep
> the build steps in `README.md`. The authoritative machine facts live in the
> global agent instructions — this file explains what they mean for this repo's
> tooling.

## Transparent file encryption

An endpoint encryption client protects some file types. Files are stored as
containers whose first bytes are `%TSD-Header-###%`; the client decrypts them
transparently for allow-listed processes only.

Observed reader matrix (same file, same user):

| Reader | Result |
| --- | --- |
| `python.exe` (also the workstation `readsrc`) | plaintext |
| `cmd.exe` (`type`) | plaintext |
| `ripgrep.exe` | plaintext |
| `git` (including `hash-object`, `diff --no-index`) | plaintext |
| Keil toolchain: `UV4.exe`, `armclang`, `armasm`, `armlink`, `fromelf` | plaintext |
| `pwsh.exe` / .NET: `Get-Content`, `Select-String`, `[IO.File]::ReadAllText` | `%TSD-Header-###%` |
| VS Code extension host (the agent's `view` tool) | `%TSD-Header-###%` |

Observed per extension (repo scan with an untrusted reader):

- encrypted on disk: `.c`, `.s`, `.uvprojx`, some `.txt` written by trusted tools
- plaintext: `.h`, `.s`, `.dts`, `.sct`, `.ps1`/`.py`, `.md`

Writer side: files written by a trusted process (`python`, `armclang`, `uVision`)
land encrypted when their extension is protected; files written by an untrusted
process (PowerShell, the VS Code host) land as plaintext. Either way **git reads
plaintext**, so committed blobs and `git diff` are portable.

### Rules

1. Read file content with the workstation reader
   (`python %USERPROFILE%\.dsh\tools\readsrc.py`), never with PowerShell.
   `Select-String` on a container returns zero matches without error, which
   silently produces wrong conclusions.
2. `readsrc` rejects a `%TSD-Header-###%` input with an explicit error; route the
   reads of a new script through it instead of `open()`.
3. Do not "fix" a container by rewriting it byte-for-byte; let the owning tool
   write it.
4. `.s` files must stay **plaintext**: `armasm` is not allow-listed, so a container
   `.s` assembles as binary garbage (`A1167E: Invalid line start`). Write them
   with an untrusted writer (`Copy-Item`) rather than Python.

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

## Host tool paths

Different machines install CMake, uVision and the ARMCLANG toolchain in
different directories. `tools/toolchain.py` resolves each tool in this order:

1. the explicit flag (`--cmake`, `--uv4`, `--keil-root`)
2. an environment variable (`CMAKE`, `UV4`, `ARMCLANG_TOOLCHAIN_PATH`)
3. `shutil.which` on `PATH`
4. a short list of known install locations

Set the environment variable once per machine (or pass the flag ad hoc) when a
tool lives somewhere unusual. Nothing machine-specific should be hard-coded in
committed scripts.

STM32 device families are **not** under `D:\Keil_v5\ARM\PACK`. `TOOLS.INI` sets a
separate pack repository (`RTEPATH`, `D:\Arm\Packs` on this workstation), and that
is where `Keil.STM32G4xx_DFP/2.0.0` and friends live. A `.pack` file sitting next
to the install root is only an archive: unless the same package appears under
`RTEPATH`, uVision cannot resolve the `$$Device:...$$` paths in a `.uvprojx`.

## uVision batch builds

`UV4.exe -b project.uvprojx -j0 -o build.log` can return before the log is
written when another uVision instance is running. `tools/build.py` deletes the
log first, waits for it to be refreshed (default 120 s), prints it, and returns
uVision's exit code. Exit code 1 means warnings, 2 means errors.

Piping these scripts into `Select-Object -First N` closes the pipe once it has
enough lines: Python is killed mid-run and the child it spawned (CMake, uVision)
is orphaned, usually still holding `build_cfg/` open so the next run fails in
`rmtree` with `PermissionError`. Redirect to a file and read its tail instead.

## Target board

PSG pulse generator board, STM32G474QET6 (LQFP128), 8 MHz HSE, 170 MHz SYSCLK,
512 KB flash, 96 KB of SRAM used (SRAM1+SRAM2; CCM is untouched). `BOOT0` is tied
low, so the image at `0x08000000` runs after every reset. Pin assignments live in
`docs/MCU引脚功能映射表.md`; the board devicetree restates only the pins the
firmware uses.

## Compiler flags

Keil optimisation must stay at `-O1` or higher. At `-O0`, armclang emits
out-of-line copies of unused `static inline` functions whose `extern`
declarations come from the generated `syscalls/*.h` headers; those copies
reference kernel symbols that this bare-metal port does not provide
(`z_timeout_expires`, ...).
