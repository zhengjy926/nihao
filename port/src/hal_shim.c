/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Replacements for the STM32 HAL/LL helpers referenced by
 * clock_stm32_ll_common.c.
 *
 * LL_SetFlashLatency() comes from upstream stm32f4xx_ll_utils.c: that file is
 * self-contained and also gives the voltage-scaling aware table for free.
 *
 * HAL_RCC_GetSysClockFreq() stays here. Compiling upstream
 * stm32f4xx_hal_rcc.c was tried: armlink reports the HAL_GetTick(),
 * HAL_InitTick(), uwTickPrio and HAL_GPIO_Init() references of the sections it
 * drops anyway (HAL_RCC_OscConfig, HAL_RCC_ClockConfig, HAL_RCC_MCOConfig), so
 * that file cannot be linked without pulling in the HAL core and its SysTick
 * time base - a second clock path fighting this one.
 */

#include <stdint.h>

#include <stm32f4xx.h>

uint32_t HAL_RCC_GetSysClockFreq(void)
{
	/* Clock bring-up runs once from the reset state, where SYSCLK is HSI. */
	return 16000000U;
}
