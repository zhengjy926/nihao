/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_APP_H_
#define APP_APP_H_

/**
 * Bring the runtime up: run the init levels, install the log timestamp hook,
 * check the devicetree devices and start the asynchronous ECSP UART.
 *
 * Must be called exactly once, before the first app_task() call.
 *
 * @return 0 on success, -ENODEV when a required devicetree device is missing.
 */
int app_init(void);

/**
 * Run one step of the super-loop: poll deferred work, process a bounded number
 * of ECSP bytes, drain logs, and service the LED deadline. ECSP owns its
 * handshake and heartbeat deadlines.
 *
 * Non-blocking; call it in a tight loop from main().
 */
void app_task(void);

#endif /* APP_APP_H_ */
