/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>

#include "app/app.h"

LOG_MODULE_DECLARE(nihao, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
	int rc = app_init();

	if (rc != 0) {
		LOG_ERR("app init failed: %d", rc);
		for (;;) {
		}
	}

	for (;;) {
		app_task();
	}
}
