#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Re-copy the vendored upstream files from a local Zephyr 4.5.0 tree.

Files are copied verbatim; never edit them afterwards. See
third_party/MANIFEST.md for the list and the rationale.
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

ZEPHYR_DIRS = (
    "include",
    "arch/common/include",
    "lib/libc/armstdc",
    "lib/libc/common/include",
    "soc/st/stm32/common",
    "soc/st/stm32/stm32g4x",
    "modules/cmsis_6",
)

ZEPHYR_FILES = (
    "kernel/device.c",
    "drivers/gpio/gpio_stm32.c",
    "drivers/gpio/gpio_stm32.h",
    "drivers/led/led_gpio.c",
    "drivers/pinctrl/pinctrl_stm32.c",
    "drivers/clock_control/clock_stm32g4.c",
    "drivers/clock_control/clock_stm32_ll_common.c",
    "drivers/clock_control/clock_stm32_ll_common.h",
    "drivers/interrupt_controller/intc_gpio_stm32.c",
    "drivers/interrupt_controller/intc_exti_stm32.c",
    "drivers/interrupt_controller/intc_exti_stm32_priv.h",
    "drivers/serial/uart_stm32.c",
    "drivers/serial/uart_stm32.h",
    "drivers/console/uart_console.c",
    "drivers/reset/reset_stm32.c",
    "drivers/pinctrl/common.c",
    "drivers/dma/dma_stm32.c",
    "drivers/dma/dma_stm32_v2.c",
    "drivers/dma/dma_stm32.h",
    "drivers/dma/dmamux_stm32.c",
    "drivers/timer/cortex_m_systick.c",
    "drivers/timer/system_timer_generic.h",
    "lib/os/printk.c",
    "lib/libc/armstdc/src/libc-hooks.c",
    "subsys/logging/log_core.c",
    "subsys/logging/log_msg.c",
    "subsys/logging/log_output.c",
    "subsys/logging/log_cache.c",
    "subsys/logging/log_cache.h",
    "subsys/logging/log_mgmt.c",
    "subsys/logging/backends/log_backend_uart.c",
    "lib/os/cbprintf.c",
    "lib/os/cbprintf_complete.c",
    "lib/os/cbprintf_packaged.c",
    "lib/os/mpsc_pbuf.c",
)

MODULE_DIRS = (
    "hal/cmsis_6/CMSIS/Core/Include",
    "hal/stm32/stm32cube/stm32g4xx/drivers/include",
    "hal/stm32/stm32cube/common_ll/include",
)

MODULE_FILES = (
    "hal/stm32/stm32cube/stm32g4xx/soc/stm32g4xx.h",
    "hal/stm32/stm32cube/stm32g4xx/soc/stm32g474xx.h",
    "hal/stm32/stm32cube/stm32g4xx/soc/system_stm32g4xx.h",
    "hal/stm32/stm32cube/stm32g4xx/soc/system_stm32g4xx.c",
    "hal/stm32/stm32cube/stm32g4xx/drivers/src/stm32g4xx_ll_dma.c",
    "hal/stm32/stm32cube/stm32g4xx/drivers/src/stm32g4xx_ll_utils.c",
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--zephyr-base", default=os.environ.get("ZEPHYR_BASE", ""))
    parser.add_argument("--modules", default=os.environ.get("ZEPHYR_MODULES", ""))
    args = parser.parse_args()

    zephyr_base = args.zephyr_base
    if not zephyr_base:
        candidate = Path(r"D:\Zephyr\zephyrproject\zephyr")
        if (candidate / "VERSION").exists():
            zephyr_base = str(candidate)
    if not zephyr_base:
        print("ZEPHYR_BASE is not set", file=sys.stderr)
        return 3
    zephyr = Path(zephyr_base)
    if not (zephyr / "VERSION").exists():
        print(f"not a Zephyr tree: {zephyr}", file=sys.stderr)
        return 3

    if args.modules:
        modules = Path(args.modules)
    else:
        modules = zephyr.parent / "modules"
    if not (modules / "hal").is_dir():
        print(f"Zephyr modules not found: {modules}", file=sys.stderr)
        return 3

    repo = args.repo.resolve()
    tp = repo / "third_party"

    for rel in ZEPHYR_DIRS:
        src = zephyr / rel
        dst = tp / "zephyr" / rel
        if not src.is_dir():
            print(f"missing upstream directory: {src}", file=sys.stderr)
            return 3
        shutil.copytree(src, dst, dirs_exist_ok=True)

    for rel in ZEPHYR_FILES:
        src = zephyr / rel
        dst = tp / "zephyr" / rel
        if not src.is_file():
            print(f"missing upstream file: {src}", file=sys.stderr)
            return 3
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

    for rel in MODULE_DIRS:
        src = modules / rel
        dst = tp / "modules" / rel
        if not src.is_dir():
            print(f"missing module directory: {src}", file=sys.stderr)
            return 3
        shutil.copytree(src, dst, dirs_exist_ok=True)

    for rel in MODULE_FILES:
        src = modules / rel
        dst = tp / "modules" / rel
        if not src.is_file():
            print(f"missing module file: {src}", file=sys.stderr)
            return 3
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

    print(f"Synced upstream sources into {tp}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
