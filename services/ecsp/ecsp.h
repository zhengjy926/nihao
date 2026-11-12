/* SPDX-License-Identifier: Apache-2.0 */
#ifndef NIHAO_ECSP_H_
#define NIHAO_ECSP_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ECSP_MAX_FRAME 512u

typedef bool (*ecsp_send_fn)(const uint8_t *frame, size_t length, void *user);
/* Return an ECSP response code; write at most 502 response data bytes. */
typedef uint8_t (*ecsp_command_fn)(uint8_t command, const uint8_t *request,
				   size_t request_length, uint8_t *response,
				   size_t *response_length, void *user);

struct ecsp_command {
	uint8_t code;
	bool sequenced;
	ecsp_command_fn handle;
};

/* All functions except ecsp_receive() run in main-loop context. */
void ecsp_init(uint8_t product, uint8_t board, const struct ecsp_command *commands,
	       size_t command_count, ecsp_send_fn send, void *user);
/* ISR-safe, bounded byte enqueue. Overflow invalidates the current frame. */
void ecsp_receive(const uint8_t *bytes, size_t length);
/* Consume at most byte_budget bytes and send at most one response. */
void ecsp_poll(size_t byte_budget, uint64_t now_ms);
bool ecsp_handshaken(void);
/* Main-loop context: emit the unconnected handshake report if transport is free. */
void ecsp_announce(void);

#endif
