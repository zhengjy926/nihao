/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log_ctrl.h>

#include "port/runtime.h"

void nihao_init_run_all(void);
void nihao_work_poll(void);
uint32_t nihao_timestamp_get(void);

void runtime_init(void)
{
	nihao_init_run_all();
	log_set_timestamp_func(nihao_timestamp_get, 1000);
}

void runtime_poll(void)
{
	nihao_work_poll();
	(void)log_process();
}

uint64_t runtime_millis(void)
{
	return k_uptime_get();
}
