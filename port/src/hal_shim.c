/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Replacements for the two STM32 HAL/LL helpers referenced by
 * clock_stm32_ll_common.c. Pulling in the full HAL RCC driver for them would
 * bring a second clock configuration path into the image.
 */

#include <stdint.h>

#include <stm32f4xx.h>

uint32_t HAL_RCC_GetSysClockFreq(void)
{
	/* Clock bring-up runs once from the reset state, where SYSCLK is HSI. */
	return 16000000U;
}

ErrorStatus LL_SetFlashLatency(uint32_t hclk)
{
	uint32_t latency;

	if (hclk <= 30000000U) {
		latency = FLASH_ACR_LATENCY_0WS;
	} else if (hclk <= 60000000U) {
		latency = FLASH_ACR_LATENCY_1WS;
	} else if (hclk <= 90000000U) {
		latency = FLASH_ACR_LATENCY_2WS;
	} else if (hclk <= 120000000U) {
		latency = FLASH_ACR_LATENCY_3WS;
	} else if (hclk <= 150000000U) {
		latency = FLASH_ACR_LATENCY_4WS;
	} else {
		latency = FLASH_ACR_LATENCY_5WS;
	}

	FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | latency;
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != latency) {
	}

	return SUCCESS;
}
