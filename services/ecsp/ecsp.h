/* SPDX-License-Identifier: Apache-2.0 */
#ifndef NIHAO_ECSP_H_
#define NIHAO_ECSP_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ECSP_MAX_FRAME 512u
#define ECSP_CONTEXT_BYTES 8192u
/* Opaque, caller-owned storage. The implementation checks its size at build. */
typedef union {
	uint64_t alignment;
	uint8_t opaque[ECSP_CONTEXT_BYTES];
} ecsp_t;

enum ecsp_link_result {
	ECSP_LINK_ACCEPTED,
	ECSP_LINK_BUSY,
	ECSP_LINK_FAULT,
};

/* ACCEPTED means the adapter owns a copy until it reports completion. */
typedef enum ecsp_link_result (*ecsp_send_fn)(const uint8_t *frame,
						   size_t length, void *user);
/* Return an ECSP response code; write at most 502 response data bytes. */
typedef uint8_t (*ecsp_command_fn)(uint8_t command, const uint8_t *request,
				   size_t request_length, uint8_t *response,
				   size_t *response_length, void *user);

struct ecsp_command {
	uint8_t code;
	bool sequenced;
	ecsp_command_fn handle;
};

struct ecsp_stats {
	uint32_t rx_overflows;
	uint32_t tx_overflows;
	uint32_t link_faults;
	size_t rx_pending;
};

/* All functions except ecsp_receive() run in main-loop context. */
void ecsp_init(ecsp_t *ctx, uint8_t product, uint8_t board,
	       const uint8_t software_version[7],
	       const struct ecsp_command *commands, size_t command_count,
	       ecsp_send_fn send, void *user);
/* ISR-safe, bounded byte enqueue. Overflow invalidates the current frame. */
void ecsp_receive(ecsp_t *ctx, const uint8_t *bytes, size_t length);
/* Consume at most byte_budget bytes and one frame; tx_budget is clamped to one. */
void ecsp_poll(ecsp_t *ctx, size_t byte_budget, size_t tx_budget, uint32_t now_ms);
/* Adapter completion; only a successful whole-frame completion counts as sent. */
void ecsp_tx_complete(ecsp_t *ctx, bool success, uint32_t now_ms);
/* Queue a single event. Fault reports are high priority; others obey upload mode. */
bool ecsp_publish_event(ecsp_t *ctx, uint8_t command, const uint8_t *payload,
			size_t length, bool fault);
bool ecsp_handshaken(const ecsp_t *ctx);
struct ecsp_stats ecsp_stats(const ecsp_t *ctx);

#endif
