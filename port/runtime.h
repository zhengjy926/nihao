/* SPDX-License-Identifier: Apache-2.0 */
#ifndef NIHAO_RUNTIME_H_
#define NIHAO_RUNTIME_H_

#include <stdint.h>

/* Call once from main before using devices or starting the application. */
void runtime_init(void);

/* Main-loop context only: execute due delayed work and drain pending logs. */
void runtime_poll(void);

/* Main-loop context: monotonic milliseconds since boot. */
uint64_t runtime_millis(void);

#endif
