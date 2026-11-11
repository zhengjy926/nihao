/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Time services the logging subsystem needs. There is no scheduler and no
 * tick interrupt here, so uptime is derived from the DWT cycle counter and
 * "sleeping" is a bounded spin.
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sleep.h>
#include <zephyr/sys/util.h>

#define DEMCR_ADDR 0xE000EDFCu
#define DWT_CTRL_ADDR 0xE0001000u
#define DWT_CYCCNT_ADDR 0xE0001004u

#define DEMCR (*(volatile uint32_t *)DEMCR_ADDR)
#define DWT_CTRL (*(volatile uint32_t *)DWT_CTRL_ADDR)
#define DWT_CYCCNT (*(volatile uint32_t *)DWT_CYCCNT_ADDR)

#define TRCENA BIT(24)
#define CYCCNTENA BIT(0)

#define CYCLES_PER_TICK                                                       \
	((uint64_t)CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC /                       \
	 (uint64_t)CONFIG_SYS_CLOCK_TICKS_PER_SEC)

static bool dwt_running;

static void dwt_enable(void)
{
	if (dwt_running) {
		return;
	}

	DEMCR |= TRCENA;
	DWT_CYCCNT = 0u;
	DWT_CTRL |= CYCCNTENA;
	dwt_running = true;
}

static uint64_t cycles(void)
{
	static uint32_t last;
	static uint64_t high;
	uint32_t now;

	dwt_enable();
	now = DWT_CYCCNT;

	if (now < last) {
		high += 1ull << 32;
	}
	last = now;

	return high | now;
}

int64_t z_impl_k_uptime_ticks(void)
{
	return (int64_t)(cycles() / CYCLES_PER_TICK);
}

uint32_t nihao_timestamp_get(void)
{
	return (uint32_t)k_uptime_get();
}

k_ticks_t z_impl_k_sleep_ticks(k_timeout_t timeout)
{
	uint64_t ticks;
	uint64_t deadline;
	uint64_t start;

	if (K_TIMEOUT_EQ(timeout, K_NO_WAIT) || K_TIMEOUT_EQ(timeout, K_FOREVER)) {
		return 0;
	}

	ticks = (uint64_t)timeout.ticks;
	start = cycles();
	deadline = start + ticks * CYCLES_PER_TICK;

	while (cycles() < deadline) {
	}

	return (k_ticks_t)ticks;
}

/*
 * Semaphores. There is no scheduler, so a blocking take is a bounded spin that
 * lets interrupts in (WFE) and gives up after a second, counting the event
 * instead of hanging forever.
 */
static volatile uint32_t sem_timeouts;

uint32_t nihao_sem_timeout_count(void)
{
	return sem_timeouts;
}

int z_impl_k_sem_init(struct k_sem *sem, unsigned int initial_count,
		      unsigned int limit)
{
	sem->count = initial_count;
	sem->limit = limit;

	return 0;
}

void z_impl_k_sem_give(struct k_sem *sem)
{
	unsigned int count = __atomic_load_n(&sem->count, __ATOMIC_ACQUIRE);

	while (count < sem->limit &&
	       !__atomic_compare_exchange_n(&sem->count, &count, count + 1u, false,
					    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
	}

	__asm volatile("sev");
}

int z_impl_k_sem_take(struct k_sem *sem, k_timeout_t timeout)
{
	const uint64_t budget = (uint64_t)CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
	uint64_t start = 0;

	for (;;) {
		unsigned int count = __atomic_load_n(&sem->count, __ATOMIC_ACQUIRE);

		if (count > 0u) {
			if (__atomic_compare_exchange_n(&sem->count, &count, count - 1u,
							false, __ATOMIC_ACQ_REL,
							__ATOMIC_ACQUIRE)) {
				return 0;
			}
			continue;
		}

		if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
			return -EBUSY;
		}

		if (start == 0u) {
			start = cycles();
		} else if (cycles() - start > budget) {
			sem_timeouts++;
			return -EBUSY;
		}

		__asm volatile("wfe");
	}
}

void z_impl_k_sem_reset(struct k_sem *sem)
{
	__atomic_store_n(&sem->count, 0u, __ATOMIC_RELEASE);
}

/*
 * Timers are only used by the log processing thread, which is not built here;
 * the deferred log queue is drained by the main loop instead.
 */
void z_impl_k_timer_start(struct k_timer *timer, k_timeout_t duration,
			  k_timeout_t period)
{
	ARG_UNUSED(timer);
	ARG_UNUSED(duration);
	ARG_UNUSED(period);
}

void z_impl_k_timer_stop(struct k_timer *timer)
{
	ARG_UNUSED(timer);
}

bool k_is_in_isr(void)
{
	uint32_t ipsr;

	__asm volatile("MRS %0, IPSR" : "=r"(ipsr));

	return (ipsr & 0x1ffu) != 0u;
}

/*
 * Delayable work. The UART driver uses a pair of them to report RX/TX idle
 * timeouts; without a work queue they are folded into nihao_work_poll(), which
 * the main loop calls on every pass.
 */
#define NIHAO_WORK_SLOTS 4

struct nihao_work {
	struct k_work_delayable *dwork;
	k_work_handler_t handler;
	uint64_t deadline;
	bool pending;
};

static struct nihao_work works[NIHAO_WORK_SLOTS];

static struct nihao_work *work_slot(struct k_work_delayable *dwork, bool create)
{
	struct nihao_work *slot = NULL;

	for (size_t i = 0; i < NIHAO_WORK_SLOTS; i++) {
		if (works[i].dwork == dwork) {
			return &works[i];
		}
		if (slot == NULL && works[i].dwork == NULL) {
			slot = &works[i];
		}
	}

	if (!create || slot == NULL) {
		return NULL;
	}

	slot->dwork = dwork;
	slot->pending = false;

	return slot;
}

void k_work_init_delayable(struct k_work_delayable *dwork, k_work_handler_t handler)
{
	struct nihao_work *slot = work_slot(dwork, true);

	if (slot != NULL) {
		slot->handler = handler;
		slot->pending = false;
	}
}

int k_work_reschedule(struct k_work_delayable *dwork, k_timeout_t delay)
{
	struct nihao_work *slot = work_slot(dwork, true);

	if (slot == NULL) {
		return -ENOMEM;
	}

	if (K_TIMEOUT_EQ(delay, K_FOREVER)) {
		slot->pending = false;
		return 0;
	}

	slot->deadline = cycles() + (K_TIMEOUT_EQ(delay, K_NO_WAIT)
					     ? 0u
					     : (uint64_t)delay.ticks * CYCLES_PER_TICK);
	slot->pending = true;

	return 1;
}

int k_work_cancel_delayable(struct k_work_delayable *dwork)
{
	struct nihao_work *slot = work_slot(dwork, false);
	bool was_pending;

	if (slot == NULL) {
		return 0;
	}

	was_pending = slot->pending;
	slot->pending = false;

	return was_pending ? 1 : 0;
}

void nihao_work_poll(void)
{
	for (size_t i = 0; i < NIHAO_WORK_SLOTS; i++) {
		struct nihao_work *slot = &works[i];

		if (!slot->pending || slot->dwork == NULL) {
			continue;
		}

		if (cycles() < slot->deadline) {
			continue;
		}

		slot->pending = false;

		if (slot->handler != NULL) {
			slot->handler(&slot->dwork->work);
		}
	}
}
