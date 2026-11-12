/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_APP_H_
#define APP_APP_H_

/**
 * Bring the runtime up: run the init levels, install the log timestamp hook,
 * check the devicetree devices and start the asynchronous console UART.
 *
 * Must be called exactly once, before the first app_task() call.
 *
 * @return 0 on success, -ENODEV when a required devicetree device is missing.
 */
int app_init(void);

/**
 * Run one step of the super-loop: poll the deferred work queue, drain the echo
 * ring and the log buffer, and flip the status LED / log the health line on
 * their own deadlines.
 *
 * Non-blocking; call it in a tight loop from main().
 */
void app_task(void);

#endif /* APP_APP_H_ */
