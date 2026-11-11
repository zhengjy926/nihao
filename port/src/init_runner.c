/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Replacement for the initialization part of the Zephyr kernel's z_cstart().
 * scatter.sct gives each Zephyr init level its own execution region, bracketed
 * by markers, so the levels run in order and each level is walked linearly.
 */

#include <stdint.h>

#include <zephyr/init.h>
#include <zephyr/device.h>

#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_MODE_MINIMAL)
#include <zephyr/logging/log_ctrl.h>
#endif

#define INIT_MARKERS(level)                                                              \
	const uint32_t init_##level##_begin_marker[1]                                      \
		__attribute__((section(".aaa_" #level "_begin"), used, aligned(4)));      \
	const uint32_t init_##level##_end_marker[1]                                        \
		__attribute__((section(".zzz_" #level "_end"), used, aligned(4)))

INIT_MARKERS(EARLY);
INIT_MARKERS(PRE_KERNEL_1);
INIT_MARKERS(PRE_KERNEL_2);
INIT_MARKERS(POST_KERNEL);
INIT_MARKERS(APPLICATION);
INIT_MARKERS(SMP);

#define INIT_RANGE(level)                                                                 \
	{                                                                                 \
		(const struct init_entry *)(init_##level##_begin_marker + 1),             \
		(const struct init_entry *)init_##level##_end_marker                      \
	}

static const struct init_entry *const init_ranges[][2] = {
	INIT_RANGE(EARLY),
	INIT_RANGE(PRE_KERNEL_1),
	INIT_RANGE(PRE_KERNEL_2),
	INIT_RANGE(POST_KERNEL),
	INIT_RANGE(APPLICATION),
	INIT_RANGE(SMP),
};

int do_device_init(const struct device *dev);

void nihao_irq_table_load(void);

void nihao_init_run_all(void)
{
	nihao_irq_table_load();

	/* z_cstart() runs LOG_CORE_INIT() before any SYS_INIT level; deferred
	 * logging depends on it to create the message buffer.
	 */
#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_MODE_MINIMAL)
	LOG_CORE_INIT();
#endif

	for (size_t level = 0; level < ARRAY_SIZE(init_ranges); level++) {
		const struct init_entry *begin = init_ranges[level][0];
		const struct init_entry *end = init_ranges[level][1];

		for (const struct init_entry *entry = begin; entry < end; entry++) {
			if (entry->dev != NULL) {
				(void)do_device_init(entry->dev);
			} else if (entry->init_fn != NULL) {
				(void)entry->init_fn();
			}
		}
	}
}
