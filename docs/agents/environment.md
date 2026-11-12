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

**Check whether it applies before obeying any rule below.** Encryption is a
property of the *reader*, not of the file, so the state cannot be inferred from a
path or remembered from an earlier session:

```powershell
python tools\check_readers.py                   # one verdict per extension
python tools\check_readers.py --show app\app.c  # one file, trusted view
```

| exit | meaning | what to do |
| --- | --- | --- |
| 0 | both views agree on every probed extension | nothing below constrains this checkout: `grep`, `Select-String` and the editor's file tools are all safe |
| 4 | the pwsh/editor view is ciphertext | read through `python`, `cmd /c type` or `git show`; a match-free result from any other reader is void |
| 5 | `python` itself sees a container | the client does not allow-list this interpreter, or the file is a bare container copied from another machine; stop until it is resolved |

`--show` is the referee: it prints one file through the trusted view, so a
suspected reader can be diffed against it line by line. Independently of the
script, `git show HEAD:<path>` returns plaintext for a committed path even on an
encrypted machine, because loose blobs and packfiles are not containers.

The *process* decides membership, not the program name: `rg.exe` on `PATH` is
allow-listed, while ripgrep linked into the editor host is not; `python` is
allow-listed, while a PowerShell pipeline around it is not.

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

1. When the probe exits 4, read file content with the workstation reader
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
5. Search sources with `python tools\find_in_sources.py <regex> [paths...]`. It
   decodes every file through the workstation reader and skips `third_party/`,
   `config_gen/` and `build_cfg/` by default. It exits 3 and names the count when
   it meets a container it cannot decode, so a printed "no match" is a fact only
   when the exit code is 0 or 1.
6. When containers are detected but no workstation reader is installed,
   `tools/check_readers.py` prints `workstation reader: MISSING`. Do not fall back
   to plain `open()`: fix the reader or ask, because every container read then
   yields silent garbage instead of an error.
7. A "no match" is a fact only when the same command also finds a token you know
   is present — `SPDX-License-Identifier` is in every source file and is a cheap
   positive control. Without the control, an empty result says nothing.

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

Python's own stdout here is `gbk` (code page 936) even when the output is piped,
while the harness reads command output as UTF-8: a tool that prints Chinese
source lines must `sys.stdout.reconfigure(encoding="utf-8")` when stdout is not
a tty, or every line arrives as mojibake. `tools/find_in_sources.py` shows the
pattern; `readsrc.py` predates it and still prints Chinese garbled when piped.

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

## RTT log capture

The firmware has no console: log lines leave the MCU over SEGGER RTT (ADR-0006),
so a serial terminal shows nothing. Both steps work without uVision:

```powershell
# flash mdk\Objects\nihao.hex and let it run
& "D:\Program Files (x86)\SEGGER\JLink\JLink.exe" -NoGui 1 -ExitOnError 1 `
  -CommandFile flash.jlink
#  flash.jlink:  si SWD / speed 4000 / device STM32G474QE / connect
#                h / loadfile <repo>\mdk\Objects\nihao.hex / r / g / qc

# read channel 0 into a file (runs until Ctrl-C)
& "D:\Program Files (x86)\SEGGER\JLink\JLinkRTTLogger.exe" -Device STM32G474QE `
  -If SWD -Speed 4000 -RTTChannel 0 rtt.log
```

- A J-Link reset halts the core (`VC_CORERESET`), so the `g` in the script — or a
  press of the board's reset button — is what makes the image run.
- Channel 0, 1024 B up-buffer (`CONFIG_SEGGER_RTT_BUFFER_SIZE_UP`). The probe
  finds the control block by scanning RAM, so no address is needed.
- With no reader attached the backend drops lines instead of blocking, so an
  empty log on a running target means nothing is reading, not a firmware fault.

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
