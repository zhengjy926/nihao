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

/*
 * Exit hook for interrupt handlers installed directly in the vector table
 * (upstream: arch/arm/core/cortex_m/exc_exit.c). Its only job is to pend
 * PendSV when the ready queue changed, which cannot happen without a
 * scheduler; the SysTick driver calls it as the last statement of its ISR, so
 * the handler's own epilogue still performs the exception return.
 */
void z_arm_int_exit(void)
{
}
