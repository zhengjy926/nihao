# Vendored sources

| Source | Version | Notes |
| --- | --- | --- |
| Zephyr | `v4.5.0` (`41d10b59730`) | `zephyr/` subtree |
| CMSIS 6 | as vendored by Zephyr 4.5.0 | `modules/hal/cmsis_6/CMSIS/Core/Include` |
| STM32CubeG4 | as vendored by Zephyr 4.5.0 | `modules/hal/stm32/stm32cube/stm32g4xx` |

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
- `zephyr/soc/st/stm32/stm32g4x/`
- `zephyr/modules/cmsis_6/`
- `modules/hal/cmsis_6/CMSIS/Core/Include/`
- `modules/hal/stm32/stm32cube/stm32g4xx/drivers/include/`
- `modules/hal/stm32/stm32cube/common_ll/include/`

Individual files:

- `zephyr/kernel/device.c`
- `zephyr/drivers/gpio/gpio_stm32.c`, `gpio_stm32.h`
- `zephyr/drivers/led/led_gpio.c`
- `zephyr/drivers/pinctrl/pinctrl_stm32.c`
- `zephyr/drivers/clock_control/clock_stm32g4.c`, `clock_stm32_ll_common.c`, `clock_stm32_ll_common.h`
- `zephyr/drivers/interrupt_controller/intc_gpio_stm32.c`, `intc_exti_stm32.c`, `intc_exti_stm32_priv.h`
- `zephyr/drivers/serial/uart_stm32.c`, `uart_stm32.h`
- `zephyr/drivers/console/uart_console.c`
- `zephyr/drivers/reset/reset_stm32.c`
- `zephyr/drivers/pinctrl/common.c`
- `zephyr/drivers/dma/dma_stm32.c`, `dma_stm32_v2.c`, `dmamux_stm32.c`, `dma_stm32.h`
- `zephyr/drivers/timer/cortex_m_systick.c`, `system_timer_generic.h`
- `zephyr/subsys/logging/log_core.c`, `log_msg.c`, `log_output.c`, `log_cache.c`, `log_cache.h`, `log_mgmt.c`
- `zephyr/subsys/logging/backends/log_backend_uart.c`
- `zephyr/lib/os/cbprintf.c`, `cbprintf_complete.c`, `cbprintf_packaged.c`, `printk.c`
- `zephyr/lib/libc/armstdc/src/libc-hooks.c`
- `modules/hal/stm32/stm32cube/stm32g4xx/soc/stm32g4xx.h`, `stm32g474xx.h`, `system_stm32g4xx.h`, `system_stm32g4xx.c`
- `modules/hal/stm32/stm32cube/stm32g4xx/drivers/src/stm32g4xx_ll_dma.c`
- `modules/hal/stm32/stm32cube/stm32g4xx/drivers/src/stm32g4xx_ll_utils.c`

Everything in `third_party/` is read-only for this project. Any adaptation
belongs in `port/`.
