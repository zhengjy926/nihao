# nihao

Zephyr's device model, devicetree configuration and STM32 drivers running on a
bare-metal runtime, built with Keil MDK and Arm Compiler 6.

Target: PSG pulse generator board (STM32G474QET6, Cortex-M4F, 8 MHz HSE, 170 MHz SYSCLK).

## Status

`led0` (PB9, active low) blinks at 1 Hz from a devicetree-declared GPIO through
Zephyr's `gpio_stm32` + `pinctrl_stm32` + `clock_stm32` drivers.

`uart_stm32` runs through the async API on DMA1 through DMAMUX (TX request 25,
RX request 24) on `USART1` (PC4/PC5).
Received bytes are echoed back by the main loop, and once a second it logs a
counter line. Logging is deferred (`LOG_MODE_DEFERRED`, no log thread): `LOG_INF()`
only records the message, and the main loop drains the buffer with
`log_process()`. The time base is the upstream `cortex_m_systick` system timer
driver: `port/src/sys_clock_glue.c` supplies the kernel-side
`sys_clock_announce_locked()` and re-arms a 1 ms tick, and timestamps are built
from its cycle counter. Lines look like
`[00:00:12.500,000] <inf> nihao: alive 13 s, ...`. `IRQ_CONNECT()` registrations
are turned into a RAM dispatch table at boot by `port/src/irq_loader.c`; the
startup file routes unclaimed vectors there (see ADR-0004).

Not yet ported: the kernel timeout queue behind `k_timer`/`k_sleep`, power
management, fault reporting.

## Requirements

- Keil MDK 5.42 with Arm Compiler 6.23.
- For header regeneration only: Zephyr 4.5.0 tree, Zephyr SDK, CMake, Python.

A normal build needs nothing but Keil: the generated devicetree/Kconfig headers
are committed under `config_gen/`.

## Build

```powershell
python tools\gen_project.py   # regenerate mdk/nihao.uvprojx + mdk/scatter.sct
python tools\build.py         # batch build with UV4, prints the log
```

Both scripts use the Python standard library only.

Or open `mdk/nihao.uvprojx` in uVision and press Build.

Output: `mdk/Objects/nihao.axf` and `nihao.hex`.

On this workstation the source files are stored as transparent-encryption
containers, so PowerShell `Get-Content` sees ciphertext. Read them with the
workstation's reader, which raises on a container instead of returning ciphertext:

```powershell
python $env:USERPROFILE\.dsh\tools\readsrc.py third_party\zephyr\kernel\device.c --head 40
python $env:USERPROFILE\.dsh\tools\readsrc.py third_party\zephyr\include\zephyr\kernel.h --grep z_timeout
```

See [docs/agents/environment.md](./docs/agents/environment.md) for the full
reader matrix and the other environment quirks.

## Flash and verify

Flash `mdk/Objects/nihao.hex` over SWD (J-Link, ST-Link, or uVision Download).

`BOOT0` is tied low on this board, so the image at `0x08000000` runs after every
reset; there is no boot-mode jumper to check.

Then open the console UART (`USART1` on PC4/PC5) at **115200
8N1**. Expected:

```
[00:00:00.0xx,xxx] <inf> nihao: nihao 4.5.0 built <date> <time>
[00:00:00.0xx,xxx] <inf> nihao: sysclk 170000000 Hz, console ready at 115200 baud
[00:00:00.0xx,xxx] <inf> nihao: async uart ready: dma tx/rx, rx ring 256 B
[00:00:01.0xx,xxx] <inf> nihao: alive 1 s, echo 0 B, drop 0, irq 0, sem timeout 0, miss 0
```

- LED0 (PB9) blinks at 1 Hz.
- Typing into the terminal is echoed back, and `echo` in the next alive line
  grows by the number of bytes sent.
- `drop 0`, `sem timeout 0` and `miss 0` are the health indicators: dropped echo
  bytes, semaphore spins that hit their bound, and unhandled interrupt vectors.
- `irq` counts every dispatched interrupt and should climb with traffic.

A J-Link connection that resets the core and then quits leaves it halted, so a
board that "goes silent" right after flashing is usually not running: press reset
(or use uVision Download) before expecting console output.

## Regenerate the configuration snapshot

Only needed after editing `boards/psg/g474qet6/g474qet6.dts` or the board
defconfig:

```powershell
$env:ZEPHYR_BASE = 'D:\Zephyr\zephyrproject\zephyr'
python tools\gen_config.py
```

## Layout

| Path | Contents |
| --- | --- |
| `boards/psg/g474qet6/` | Devicetree and Kconfig board definition, plus the CMSIS startup file |
| `config_gen/4.5.0/` | Committed snapshot of `autoconf.h`, `devicetree_generated.h`, `version.h`, syscall and kobject headers |
| `third_party/` | Unmodified upstream sources (Zephyr 4.5.0, CMSIS, STM32 HAL headers) |
| `port/` | The shim layer: everything this port has to provide itself |
| `app/` | Application: entry point, super-loop, and the services it drives |
| `services/` | Board-independent functional domains; see `services/README.md` |
| `tools/` | Config generation, project generation, batch build, upstream sync |

See [PORTING.md](./PORTING.md) for what was changed, why, and what is still
missing.
