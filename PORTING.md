# Porting notes

Upstream: Zephyr 4.5.0 (`v4.5.0`, `41d10b59730`), CMSIS 6 and STM32CubeF4
headers as vendored by that tree. Nothing under `third_party/` has been edited.

## What is used

| Upstream path | Role |
| --- | --- |
| `include/` | Zephyr headers (`device.h`, `init.h`, `devicetree.h`, drivers APIs, ...) |
| `kernel/device.c` | Device model: `do_device_init()`, `device_is_ready()`, device list |
| `drivers/gpio/gpio_stm32.c` | GPIO API implementation |
| `soc/st/stm32/common/gpioport_mgr.c` | Per-port device instances (4.5 moved them out of the GPIO driver) |
| `drivers/pinctrl/pinctrl_stm32.c` | Pin muxing from devicetree |
| `drivers/clock_control/clock_stm32f2_f4_f7.c`, `clock_stm32_ll_common.c` | PLL and peripheral clock control |
| `drivers/interrupt_controller/intc_gpio_stm32.c`, `intc_exti_stm32.c` | GPIO interrupt plumbing referenced unconditionally by the drivers |
| `drivers/pinctrl/common.c` | `pinctrl_lookup_state()`, referenced by the UART driver |
| `drivers/serial/uart_stm32.c` | UART API implementation, including TX/RX DMA |
| `drivers/dma/dma_stm32.c`, `dma_stm32_v1.c` | STM32 DMA controller (V1, as used by the F4) |
| `drivers/console/uart_console.c` | Console backend that `log_backend_uart` depends on |
| `drivers/reset/reset_stm32.c` | RCC reset controller; the UART node carries a `resets` property |
| `subsys/logging/log_core.c`, `log_msg.c`, `log_output.c`, `log_cache.c`, `log_mgmt.c` | Logging subsystem, deferred mode (`mpsc_pbuf`, drained by `log_process()`) |
| `subsys/logging/backends/log_backend_uart.c` | UART log backend (polled, not the async variant) |
| `lib/os/cbprintf.c`, `cbprintf_complete.c`, `cbprintf_packaged.c` | Formatting behind `LOG_*` |
| `lib/utils/mpsc_pbuf.c` | The multi-producer buffer behind deferred logging |
| `lib/os/printk.c` | `snprintk()` and the printk char-out hook used by the console |
| `lib/libc/armstdc/` | Zephyr's Arm Compiler C-library glue (`errno.c`) |
| `soc/st/stm32/common/stm32_bitops.h`, `stm32_hsem.h`, `stm32_gpio_shared.h`, `pinctrl_soc.h` | SoC helpers |
| `modules/hal/cmsis_6`, `modules/hal/stm32/stm32cube/stm32f4xx`, `common_ll` | CMSIS core and STM32F4 device/LL headers |
| `arch/common/include` | `gen_offset.h` and friends |

Everything else (kernel, scheduler, logging, UART, ...) is intentionally absent.

## Shim layer (`port/`)

`port/` is the only place where this project owns code. It contains:

- `init_runner.c` — walks the init table instead of `z_cstart()`. `scatter.sct`
  gives every Zephyr init level its own execution region, bracketed by markers,
  so levels run in order and priorities keep their upstream (name-sorted) order.
- `irq_loader.c` — the interrupt loader: copies the `<IRQ_CONNECT>` registrations
  out of `.intList` into a RAM table at boot and dispatches by exception number.
  The startup file's default handler branches here (see ADR-0004).
- `device_list_bounds.c` — publishes `_device_list_start`/`_device_list_end`,
  which `kernel/device.c` expects from the linker.
- `arch_irq_shim.c` — `arm_irq_enable/disable/priority_set` on top of CMSIS NVIC.
- `errno_shim.c` — `z_impl_z_errno()` and `__aeabi_errno_addr()` for the Arm
  Compiler C library (the upstream hook needs a kernel thread pointer).
- `hal_shim.c` — `HAL_RCC_GetSysClockFreq()` and `LL_SetFlashLatency()`. Pulling
  in the full HAL RCC driver would bring a second clock path into the image.
- `log_section_bounds.c` — `_log_*_list_start`/`_end` for the iterable lists the
  logging subsystem walks; `scatter.sct` brackets each list with markers.
- `rodata_bounds.s` — `__rodata_region_start`/`_end`, the flash window cbprintf
  checks before referencing a format string in place.
- `kernel_shim.c` — tick-based time (`k_uptime_ticks`, `k_sleep_ticks`) on top of
  the DWT cycle counter, because there is no SysTick interrupt or scheduler;
  counting semaphores whose blocking take is a bounded `WFE` spin; and the
  `k_work_delayable` calls the UART uses, folded into `nihao_work_poll()` (see
  [Delayed work](#delayed-work-k_work_delayable-without-a-work-queue)).
- `libc_shim.c` — `__stdout_hook_install()`/`fputc()` for the console. The
  upstream version lives in `armstdc/src/libc-hooks.c` but also defines
  `__aeabi_errno_addr()` in terms of the current thread, which does not exist
  here.

Patch count applied to upstream sources: **0**.

## Delayed work: `k_work_delayable` without a work queue

`uart_stm32`'s async path owns two `k_work_delayable` items, `dma_rx.timeout_work`
and `dma_tx.timeout_work`, and expects a system work queue to run them. The RX
one is what turns `uart_rx_enable()`'s `timeout` into a `UART_RX_RDY` event: the
USART IDLE interrupt arms the work for `timeout`, and when the work runs it
flushes whatever the DMA collected. No work queue exists here, so
`port/src/kernel_shim.c` keeps a small table of delayed items and a polling
executor, `nihao_work_poll()`, that the main loop calls on every pass (`main()`
and `idle_until()`).

### Data model

```c
struct nihao_work {
	struct k_work_delayable *dwork; /* identity: Zephyr hands out no cookie */
	k_work_handler_t handler;
	uint64_t deadline;              /* absolute, in DWT cycles */
	bool pending;
};

static struct nihao_work works[NIHAO_WORK_SLOTS]; /* 4 */
```

A slot is claimed by the first `k_work_init_delayable()`/`k_work_reschedule()`
for a given `dwork` and then matched by pointer identity; lookup is a linear
scan. Two slots carry the UART; the other two are slack for the next driver that
wants one.

### API surface

| Call | Behaviour here | Upstream contract |
| --- | --- | --- |
| `k_work_init_delayable(dwork, handler)` | claims a slot, stores the handler, clears `pending`. Returns `void`, so a failed claim is **silent** | create an idle work item |
| `k_work_reschedule(dwork, delay)` | `K_FOREVER` → cancel, return 0; `K_NO_WAIT` → due now; otherwise `deadline = cycles() + delay.ticks * CYCLES_PER_TICK`. Returns 1, or `-ENOMEM` when no slot is free | queue on the system work queue; 1 if it was pending, 0 if not, negative on error |
| `k_work_cancel_delayable(dwork)` | clears `pending`, returns 1 if it was pending | cancel; 1 if it was pending |
| everything else | absent: `k_work_submit`, `k_work_schedule`, `k_work_flush`, `k_work_delayable_busy_get`, `k_work_delayable_remaining_get`, `K_WORK_DELAYABLE_DEFINE` | |

### Time base

`k_timeout_t.ticks` is already in ticks, not in the unit the caller passed:
`K_USEC()`/`K_MSEC()`/`K_TICKS()` go through `Z_TIMEOUT_US()` →
`k_us_to_ticks_ceil32()`, which rounds up at compile time. (This is also why
`K_FOREVER` is just `{.ticks = -1}` and `K_NO_WAIT` is `{.ticks = 0}`, so
`K_TIMEOUT_EQ()` is a comparison of two integers.) The shim therefore only has to
scale ticks to cycles:

```
CYCLES_PER_TICK = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / CONFIG_SYS_CLOCK_TICKS_PER_SEC
                = 180000000 / 10000 = 18000 cycles = 100 us
```

100 µs is the resolution of every deadline here. The re-arm the driver does after
a short DMA burst is `K_TICKS(1)` = 100 µs; `main.c` asks for `K_USEC(1000)` =
10 ticks.

`cycles()` is called from both the main loop and interrupt context, and it keeps
the counter's 32-bit wrap high word in `static` state, so the read/update pair
runs inside a PRIMASK-masked critical section (`irq_save()`/`irq_restore()` in
`kernel_shim.c`). The saved mask is restored rather than unconditionally enabled,
so a call from inside `irq_lock()` stays locked; the section is a handful of
cycles, which matters because `k_sleep_ticks()` and `k_sem_take()` call it in
their spin loops.

### Execution

`nihao_work_poll()` runs each due handler inline, in main context, to completion,
and clears `pending` *before* the call so the handler may reschedule itself.
Arms are one-shot; there is no periodic mode, which is exactly how `uart_stm32`
uses them.

Latency from deadline to run is the longest gap between two polls: one
`log_process()` drain plus one echo chunk's TX wait, i.e. tens of microseconds
when the board is idle. So the RX idle flush lands at "1 ms plus loop latency"
rather than exactly 1 ms, and the same delay applies to `UART_RX_RDY` after a
short packet.

### Concurrency

`k_work_reschedule()` is reached from interrupt context: the USART1 IDLE
interrupt (`uart_stm32_isr`) arms the RX idle timer as `K_USEC(timeout)`, and the
DMA RX callback re-arms it as `K_TICKS(1)` when a buffer fills with no next
buffer queued. `nihao_work_poll()` runs in main context. There is no lock; the
publication order is what makes it work:

- the writer stores `deadline` before setting `pending = true`;
- the reader tests `pending` before reading `deadline`.

Every store is a single 32-bit STR on Cortex-M4, so a poll that observes
`pending == true` sees a `deadline` written before that flag. The residual race
is a *stale* deadline: an ISR re-arming between the reader's `pending` test and
its `deadline` read can make the handler run at the earlier deadline. For an idle
flush that is harmless (an early flush is still a flush). This is not a general
work queue and should not be grown into one without revisiting this section.

### Failure modes

- **Slot exhaustion.** A fifth distinct `k_work_delayable` gets `-ENOMEM` from
  `k_work_reschedule()`, and because `k_work_init_delayable()` returns `void`
  nothing reports the failure. Symptom: bytes arrive on the UART, `irq` in the
  alive line climbs, but the application never gets `UART_RX_RDY` because the
  idle flush never fires. Raise `NIHAO_WORK_SLOTS`.
- **64-bit `deadline` compare** is plain unsigned arithmetic; wrapping needs
  ~10^11 s of uptime.

## Configuration snapshot

`config_gen/4.5.0/` holds the headers produced by a Zephyr configure run for
`boards/alientek/f429igt6`. Keil consumes them with
`-imacros .../autoconf.h` and `-I config_gen/4.5.0`.

Regenerate with `python tools/gen_config.py`. The script:

1. applies `tools/patches/0001-armclang-dts-preprocessor.patch` to the Zephyr
   tree, then reverts it;
2. configures with `ZEPHYR_TOOLCHAIN_VARIANT=armclang` so the generated
   `autoconf.h` matches Arm Compiler's C library (a GCC/picolibc configuration
   breaks on `errno`);
3. builds `version_h`, `syscall_list_h_target`, `kobj_types_h_target` and
   `heap_constants_h` and copies their outputs into the snapshot.

The local patch exists because Arm Compiler 6.23 writes `-E` output in a
container that python-devicetree cannot read on this workstation; it substitutes
a Zephyr SDK GCC for the devicetree preprocessing step only. It does not affect
the Keil build.

## Known deviations and gotchas

- **Init order** is by level region, then by section name inside a level —
  the same ordering rule Zephyr's own linker uses.
- **`-O1` or higher is required**: at `-O0` armclang emits unused `static
  inline` kernel helpers, which drag in unresolved kernel symbols.
- **Stack is 8 KiB** (`Stack_Size` in `startup_stm32f429xx.s`). `scatter.sct`
  gives the stack and heap their own fixed execution regions (`ER_STACK`,
  `ER_HEAP`), so static data that grows into them fails to link instead of
  corrupting the stack at run time. `armasm` cannot reference the linker's
  `Image$$...` symbols, so the sizes stay in the startup file and the scatter
  regions only enforce them.
- **armlink ignores section selectors inside regions with a fixed base address**:
  in `ER_IROM1` and `RW_IRAM1` only `.ANY` takes effect. Every iterable list
  therefore has its own `+0` execution region, and the bracketing marker
  sections use `+First`/`+Last` because zero-length sections are otherwise
  placed at the end of their region. `.intList` is marked RW by the compiler, so
  `+Last` does not work there: its end marker sits in a separate region
  (`ER_INTLIST_END`) that follows it.
- **The endpoint encryption product mangles files written by `python.exe`**:  they are stored encrypted and only whitelisted readers (`armclang`, `python`)
  see plaintext. `armasm` is not whitelisted, so any `.s` file must be written
  by a plaintext writer (PowerShell) or the assembler reads `%TSD-Header-###%`
  and reports `A1271E: Line too long`.
- **A zero-length marker array must be indexed from another translation unit.**
  `const char x[0]` indexed in the unit that defines it is undefined behaviour, so
  `-O1` deletes the whole loop (`nihao_irq_table_load` compiled to a single store
  before this was fixed). Define the markers in one file and declare them as
  `extern const struct ... []` where they are walked.
- **`uart_rx_enable()`'s `timeout` is the idle-flush timeout, not an enable
  deadline.** `SYS_FOREVER_US` makes the driver reschedule its flush work for
  ~35 minutes (`K_USEC(INT32_MAX)`), so a short input never reaches the caller
  until the 64-byte buffer fills. Use a small value (1 ms here).
- **Deferred logging needs `log_core_init()`.** `z_cstart()` runs `LOG_CORE_INIT()`
  before any SYS_INIT level, and that is where `z_log_msg_init()` does
  `mpsc_pbuf_init()` and sets the current message buffer. Running only the init
  levels leaves `curr_log_buffer` NULL: `LOG_INF()` then succeeds, costs the
  timestamp, and drops the message, so nothing ever reaches the backend.
  `init_runner.c` calls it in the same place.
- **A UART device has exactly one async callback.** `LOG_BACKEND_UART_ASYNC`'s
  `char_out` installs its own callback and waits on a semaphore that only that
  callback gives, so an application that also calls `uart_callback_set()` (for
  RX) takes the backend's slot and the backend waits forever. Either the log
  backend or the application may own the callback, not both - this port keeps it
  for the application and lets the log backend poll (`LOG_BACKEND_UART`).
- **`clk_hse` must be enabled in the board devicetree.** The SoC dtsi ships it
  with `status = "disabled"`. If the PLL selects HSE but the node stays
  disabled, `set_up_fixed_clock_sources()` never starts the crystal and
  `stm32_clock_control_init()` spins forever on `PLLRDY`. Symptom: the image
  hangs before `main`, with `RCC->CR = 0x01007D83` (PLLON set, PLLRDY clear,
  HSEON clear).
- **`HAL_RCC_GetSysClockFreq()` returns the reset-state HSI frequency.** It is
  only used once, to compute the flash latency before the PLL is programmed.
- **No `SystemInit` clock setup**: `system_stm32f4xx.c` provides the startup
  hook, but the PLL/bus configuration comes from `clock_stm32` and the
  devicetree, so `f429igt6.dts` is the single source of truth for clocks.

## Next steps

1. Add a fault handler that can report through UART (the one thing that still
   loses all diagnostics).
2. Add `k_mutex`/`k_work` (non-delayable) shims once a driver needs them.
