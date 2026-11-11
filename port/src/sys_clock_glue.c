/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * The kernel half of the upstream system timer.
 *
 * drivers/timer/cortex_m_systick.c plus drivers/timer/system_timer_generic.h
 * provide the SysTick driver, the cycle counter and the tick accounting. What
 * they expect from the kernel is sys_clock_announce_locked(), which upstream
 * lives in kernel/timeout.c and does three things this port has no use for:
 * it owns a timeout queue, wakes threads and drives the scheduler.
 *
 * Here it only advances a tick counter and asks the driver for a fixed 1 ms
 * cadence, so every announce covers ten ticks (100 us each). Deferred work is
 * still run from nihao_work_poll() in main context; nothing in this file calls
 * a driver callback.
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/arch/cpu.h>
#include <zephyr/drivers/timer/system_timer.h>
#include <zephyr/kernel.h>

#define NIHAO_TICKS_PER_MS (CONFIG_SYS_CLOCK_TICKS_PER_SEC / 1000u)

/* Written under the clock lock from the SysTick ISR. */
static volatile uint64_t announced_ticks;

/*
 * Before the driver's PRE_KERNEL_2 init has announced anything, SysTick has no
 * load value and the cycle counter means nothing; report zero rather than a
 * number derived from an unprogrammed down-counter. Only logs emitted during
 * device init can see this.
 */
static volatile bool clock_ready;

void sys_clock_announce_locked(uint32_t ticks, k_spinlock_key_t key)
{
	announced_ticks += ticks;
	clock_ready = true;

	/*
	 * A kernel would program the next timeout here. There is no timeout
	 * queue, so ask for a fixed 1 ms interval instead: the driver reloads
	 * SysTick and every announce then covers NIHAO_TICKS_PER_MS ticks.
	 * Called with the clock lock held, as the interface requires.
	 */
	sys_clock_set_timeout(NIHAO_TICKS_PER_MS, false);

	/* Consumes the key: this is the unlock for sys_clock_lock(). */
	arch_irq_unlock(key.key);
}

int64_t z_impl_k_uptime_ticks(void)
{
	k_spinlock_key_t key;
	uint64_t ticks;

	if (!clock_ready) {
		return 0;
	}

	key = sys_clock_lock();
	ticks = announced_ticks + sys_clock_elapsed();
	arch_irq_unlock(key.key);

	return (int64_t)ticks;
}
