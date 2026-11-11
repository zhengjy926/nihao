# Vendored sources

| Source | Version | Notes |
| --- | --- | --- |
| Zephyr | `v4.5.0` (`41d10b59730`) | `zephyr/` subtree |
| CMSIS 6 | as vendored by Zephyr 4.5.0 | `modules/hal/cmsis_6/CMSIS/Core/Include` |
| STM32CubeF4 | as vendored by Zephyr 4.5.0 | `modules/hal/stm32/stm32cube/stm32f4xx` |

Files are copied byte-for-byte, with the upstream directory layout preserved so
that includes and future diffs stay recognizable. Re-sync with:

```powershell
$env:ZEPHYR_BASE = 'D:\Zephyr\zephyrproject\zephyr'
$env:ZEPHYR_MODULES = 'D:\Zephyr\zephyrproject\modules'
python tools\sync_upstream.py
```
Directories copied wholesale:

- `zephyr/include/`
- `zephyr/arch/common/include/`
- `zephyr/lib/libc/armstdc/`
- `zephyr/lib/libc/common/include/`
- `zephyr/soc/st/stm32/common/`
- `zephyr/soc/st/stm32/stm32f4x/`
- `zephyr/modules/cmsis_6/`
- `modules/hal/cmsis_6/CMSIS/Core/Include/`
- `modules/hal/stm32/stm32cube/stm32f4xx/drivers/include/`
- `modules/hal/stm32/stm32cube/common_ll/include/`

Individual files:

- `zephyr/kernel/device.c`
- `zephyr/drivers/gpio/gpio_stm32.c`, `gpio_stm32.h`
- `zephyr/drivers/pinctrl/pinctrl_stm32.c`
- `zephyr/drivers/clock_control/clock_stm32f2_f4_f7.c`, `clock_stm32_ll_common.c`, `clock_stm32_ll_common.h`
- `zephyr/drivers/interrupt_controller/intc_gpio_stm32.c`, `intc_exti_stm32.c`, `intc_exti_stm32_priv.h`
- `zephyr/drivers/serial/uart_stm32.c`, `uart_stm32.h`
- `zephyr/drivers/console/uart_console.c`
- `zephyr/drivers/reset/reset_stm32.c`
- `zephyr/drivers/pinctrl/common.c`
- `zephyr/drivers/dma/dma_stm32.c`, `dma_stm32_v1.c`, `dma_stm32.h`
- `zephyr/subsys/logging/log_core.c`, `log_msg.c`, `log_output.c`, `log_cache.c`, `log_cache.h`, `log_mgmt.c`
- `zephyr/subsys/logging/backends/log_backend_uart.c`
- `zephyr/lib/os/cbprintf.c`, `cbprintf_complete.c`, `cbprintf_packaged.c`, `printk.c`
- `zephyr/lib/libc/armstdc/src/libc-hooks.c`
- `modules/hal/stm32/stm32cube/stm32f4xx/soc/stm32f4xx.h`, `stm32f429xx.h`, `system_stm32f4xx.h`, `system_stm32f4xx.c`
- `modules/hal/stm32/stm32cube/stm32f4xx/drivers/src/stm32f4xx_ll_dma.c`

Everything in `third_party/` is read-only for this project. Any adaptation
belongs in `port/`.
