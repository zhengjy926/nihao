/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal Cortex-M interrupt plumbing used by the ported STM32 drivers.
 */

#include <stdint.h>

#include <cmsis_core.h>
#include <zephyr/arch/arm/irq.h>

void arm_irq_enable(unsigned int irq)
{
	NVIC_EnableIRQ((IRQn_Type)irq);
}

void arm_irq_disable(unsigned int irq)
{
	NVIC_DisableIRQ((IRQn_Type)irq);
}

void arm_irq_priority_set(unsigned int irq, unsigned int prio, uint32_t flags)
{
	(void)flags;

	/* Zephyr passes the already positioned priority value. */
	NVIC->IPR[irq] = (uint8_t)prio;
}
