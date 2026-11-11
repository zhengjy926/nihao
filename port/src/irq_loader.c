/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Interrupt dispatcher. Upstream's IRQ_CONNECT() leaves a registration in the
 * .intList section instead of writing a vector-table entry (see ADR-0004).
 * This module turns those registrations into a RAM table at boot; the startup
 * file routes every unclaimed IRQ vector here.
 */

#include <stddef.h>
#include <stdint.h>

#include <zephyr/sw_isr_table.h>

extern const struct _isr_list _intlist_list_start[];
extern const struct _isr_list _intlist_list_end[];

struct nihao_isr_entry {
	void (*isr)(const void *);
	const void *arg;
};

static struct nihao_isr_entry isr_table[CONFIG_NUM_IRQS];
static volatile uint32_t unhandled_irqs;
static volatile uint32_t irq_count;

uint32_t nihao_unhandled_irq_count(void)
{
	return unhandled_irqs;
}

uint32_t nihao_irq_count(void)
{
	return irq_count;
}

#define NVIC_ICER0 ((volatile uint32_t *)0xE000E180u)

static void irq_disable_raw(uint32_t irq)
{
	NVIC_ICER0[irq >> 5] = 1u << (irq & 31u);
}

void nihao_irq_table_load(void)
{
	const size_t count = (size_t)(_intlist_list_end - _intlist_list_start);

	for (size_t i = 0; i < count; i++) {
		const struct _isr_list *entry = &_intlist_list_start[i];

		if (entry->irq < 0 || (uint32_t)entry->irq >= CONFIG_NUM_IRQS) {
			continue;
		}

		isr_table[entry->irq].isr = (void (*)(const void *))entry->func;
		isr_table[entry->irq].arg = entry->param;
	}
}

void nihao_irq_dispatch(void)
{
	uint32_t ipsr;
	uint32_t irq;

	__asm volatile("MRS %0, IPSR" : "=r"(ipsr));

	irq = (ipsr & 0x1ffu) - 16u;

	if (irq >= CONFIG_NUM_IRQS) {
		return;
	}

	if (isr_table[irq].isr != NULL) {
		irq_count++;
		isr_table[irq].isr(isr_table[irq].arg);
		return;
	}

	/* Nothing registered: mask the line so a stuck source cannot storm. */
	unhandled_irqs++;
	irq_disable_raw(irq);
}