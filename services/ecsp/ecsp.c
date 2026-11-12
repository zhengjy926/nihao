/* SPDX-License-Identifier: Apache-2.0 */
#include "services/ecsp/ecsp.h"
#include <string.h>
#include <zephyr/sys/crc.h>

#define RX_CAPACITY 1024u
#define REPLAY_COUNT 8u
#define EVENT_COUNT 2u /* slot 0: fault; slot 1: other single event */

struct queued_frame {
	uint8_t bytes[ECSP_MAX_FRAME];
	size_t length;
	bool fault;
};

struct replay_entry {
	uint8_t command;
	uint8_t sequence;
	uint8_t payload[ECSP_MAX_FRAME - 9u];
	size_t length;
	bool valid;
};

struct ecsp_state {
	uint8_t product, board;
	uint8_t software_version[7];
	const struct ecsp_command *commands;
	size_t command_count;
	ecsp_send_fn send;
	void *user;
	volatile unsigned int rx_head, rx_tail;
	volatile bool rx_overflow;
	uint8_t rx[RX_CAPACITY];
	uint8_t frame[ECSP_MAX_FRAME];
	size_t frame_size, expected_size;
	uint32_t last_byte_ms, last_report_ms, last_sent_ms;
	bool clock_started, handshaken, upload_on, received_command;
	struct replay_entry replay[REPLAY_COUNT];
	unsigned int replay_next;
	struct queued_frame reply;
	struct queued_frame events[EVENT_COUNT];
	/* 0 idle, 1 reply, 2.. events, 4 periodic report. */
	unsigned int in_flight;
	bool replaced_reply;
	uint32_t rx_overflows, tx_overflows, link_faults;
};

_Static_assert(sizeof(struct ecsp_state) <= ECSP_CONTEXT_BYTES,
	       "increase ECSP_CONTEXT_BYTES for the private state");

static struct ecsp_state *state(ecsp_t *ctx)
{
	return (struct ecsp_state *)ctx;
}

static const struct ecsp_state *const_state(const ecsp_t *ctx)
{
	return (const struct ecsp_state *)ctx;
}

static bool encode(const struct ecsp_state *s, struct queued_frame *out,
		   uint8_t command, const uint8_t *payload, size_t length)
{
	uint16_t crc;
	if (length > ECSP_MAX_FRAME - 9u) return false;
	out->length = length + 9u;
	out->bytes[0] = 0xfa;
	out->bytes[1] = (uint8_t)out->length;
	out->bytes[2] = (uint8_t)(out->length >> 8);
	out->bytes[3] = s->product;
	out->bytes[4] = command;
	out->bytes[5] = s->board;
	if (length != 0u) memcpy(out->bytes + 6, payload, length);
	crc = crc16_ansi(out->bytes + 1, out->length - 4u);
	out->bytes[out->length - 3u] = (uint8_t)crc;
	out->bytes[out->length - 2u] = (uint8_t)(crc >> 8);
	out->bytes[out->length - 1u] = 0x0d;
	return true;
}

static void clear_session(struct ecsp_state *s)
{
	if (s->in_flight == 1u) s->replaced_reply = true;
	memset(s->replay, 0, sizeof(s->replay));
	s->replay_next = 0;
	for (size_t i = 0; i < EVENT_COUNT; ++i) s->events[i].length = 0;
	s->reply.length = 0;
	s->upload_on = false;
	s->received_command = false;
}

static void answer(struct ecsp_state *s, uint8_t command,
		   const uint8_t *payload, size_t length)
{
	if (s->reply.length != 0u || !encode(s, &s->reply, command, payload, length))
		s->tx_overflows++;
}

static void process_frame(struct ecsp_state *s)
{
	const struct ecsp_command *entry = NULL;
	uint8_t result[ECSP_MAX_FRAME - 9u];
	size_t data_length = 0;
	uint8_t command = s->frame[4];
	size_t request_length = s->frame_size - 9u;
	const uint8_t *request = s->frame + 6;
	if (s->frame[3] != s->product || s->frame[5] != s->board) return;
	if (command == 1u) {
		result[0] = request_length == 0u ? 0u : 2u;
		if (result[0] == 0u) {
			clear_session(s);
			s->handshaken = true;
		}
		answer(s, command, result, 1u);
		return;
	}
	if (command == 2u) {
		if (!s->handshaken) result[0] = 0x10u;
		else if (request_length != 0u) result[0] = 2u;
		else {
			result[0] = 0u;
			memcpy(result + 1, s->software_version, 7u);
			data_length = 7u;
			s->received_command = true;
		}
		answer(s, command, result, data_length + 1u);
		return;
	}
	if (command == 0x0fu) {
		if (!s->handshaken) result[0] = 0x10u;
		else if (request_length != 1u) result[0] = 2u;
		else if (request[0] > 1u) result[0] = 3u;
		else {
			s->upload_on = request[0] != 0u;
			s->received_command = true;
			result[0] = 0u;
		}
		answer(s, command, result, 1u);
		return;
	}
	for (size_t i = 0; i < s->command_count; ++i) {
		if (s->commands[i].code == command) { entry = &s->commands[i]; break; }
	}
	if (entry == NULL) result[0] = 4u;
	else if (!s->handshaken) result[0] = 0x10u;
	else if (entry->sequenced && request_length == 0u) result[0] = 2u;
	else {
		s->received_command = true;
		if (entry->sequenced) {
			for (size_t i = 0; i < REPLAY_COUNT; ++i) {
				struct replay_entry *r = &s->replay[i];
				if (r->valid && r->command == command &&
				    r->sequence == request[0]) {
					answer(s, command, r->payload, r->length);
					return;
				}
			}
		}
		data_length = sizeof(result) - 1u;
		result[0] = entry->handle(command, request, request_length,
					  result + 1, &data_length, s->user);
		if (data_length > sizeof(result) - 1u) { s->tx_overflows++; return; }
		if (result[0] != 0u) data_length = 0u;
		if (entry->sequenced) {
			struct replay_entry *r = &s->replay[s->replay_next++ % REPLAY_COUNT];
			r->command = command;
			r->sequence = request[0];
			r->length = data_length + 1u;
			memcpy(r->payload, result, r->length);
			r->valid = true;
		}
	}
	answer(s, command, result, data_length + 1u);
}

static void send_next(struct ecsp_state *s, uint32_t now_ms)
{
	struct queued_frame periodic;
	struct queued_frame *next = NULL;
	unsigned int kind = 0;
	if (s->in_flight != 0u) return;
	if (s->reply.length != 0u) { next = &s->reply; kind = 1u; }
	else {
		for (size_t i = 0; i < EVENT_COUNT; ++i) {
			if (s->events[i].length != 0u && s->events[i].fault) {
				next = &s->events[i]; kind = (unsigned int)i + 2u; break;
			}
		}
		if (next == NULL) for (size_t i = 0; i < EVENT_COUNT; ++i) {
			if (s->events[i].length != 0u) {
				next = &s->events[i]; kind = (unsigned int)i + 2u; break;
			}
		}
	}
	if (next == NULL) {
		if (!s->handshaken && (uint32_t)(now_ms - s->last_report_ms) >= 1000u) {
			(void)encode(s, &periodic, 1u, NULL, 0u);
			next = &periodic; kind = 4u;
		} else if (s->handshaken && (uint32_t)(now_ms - s->last_sent_ms) >= 1000u) {
			(void)encode(s, &periodic, 0x10u, NULL, 0u);
			next = &periodic; kind = 4u;
		}
	}
	if (next == NULL) return;
	switch (s->send(next->bytes, next->length, s->user)) {
	case ECSP_LINK_ACCEPTED:
		s->in_flight = kind;
		if (kind == 1u) s->replaced_reply = false;
		if (kind == 4u && !s->handshaken) s->last_report_ms = now_ms;
		break;
	case ECSP_LINK_FAULT:
		s->link_faults++;
		if (kind != 4u) next->length = 0u;
		else if (!s->handshaken) s->last_report_ms = now_ms;
		break;
	case ECSP_LINK_BUSY:
		break;
	}
}

void ecsp_init(ecsp_t *ctx, uint8_t product, uint8_t board,
	       const uint8_t software_version[7],
	       const struct ecsp_command *commands, size_t count,
	       ecsp_send_fn send, void *user)
{
	struct ecsp_state *s = state(ctx);
	memset(ctx, 0, sizeof(*ctx));
	s->product = product; s->board = board;
	memcpy(s->software_version, software_version, 7u);
	s->commands = commands; s->command_count = count;
	s->send = send; s->user = user;
}

void ecsp_receive(ecsp_t *ctx, const uint8_t *bytes, size_t length)
{
	struct ecsp_state *s = state(ctx);
	for (size_t i = 0; i < length; ++i) {
		unsigned int next = (s->rx_head + 1u) % RX_CAPACITY;
		if (next == s->rx_tail) { s->rx_overflow = true; return; }
		s->rx[s->rx_head] = bytes[i];
		s->rx_head = next;
	}
}

void ecsp_poll(ecsp_t *ctx, size_t byte_budget, size_t tx_budget, uint32_t now_ms)
{
	struct ecsp_state *s = state(ctx);
	if (!s->clock_started) {
		s->last_sent_ms = s->last_report_ms = now_ms;
		s->clock_started = true;
	}
	if (s->frame_size && (uint32_t)(now_ms - s->last_byte_ms) >= 50u)
		s->frame_size = s->expected_size = 0;
	if (s->rx_overflow) {
		s->rx_tail = s->rx_head;
		s->frame_size = s->expected_size = 0;
		s->rx_overflow = false;
		s->rx_overflows++;
	}
	/* A complete request can wait for its response slot without executing twice. */
	if (s->expected_size != 0u && s->frame_size == s->expected_size) {
		if (s->reply.length != 0u) {
			if (tx_budget != 0u) send_next(s, now_ms);
			return;
		}
		process_frame(s);
		s->frame_size = s->expected_size = 0;
	}
	while (byte_budget-- != 0u && s->rx_tail != s->rx_head) {
		uint8_t byte = s->rx[s->rx_tail];
		s->rx_tail = (s->rx_tail + 1u) % RX_CAPACITY;
		s->last_byte_ms = now_ms;
		if (s->frame_size == 0u && byte != 0xfa) continue;
		s->frame[s->frame_size++] = byte;
		if (s->frame_size == 3u) {
			s->expected_size = (size_t)s->frame[1] | ((size_t)s->frame[2] << 8);
			if (s->expected_size < 9u || s->expected_size > ECSP_MAX_FRAME)
				s->frame_size = s->expected_size = 0;
		}
		if (s->expected_size != 0u && s->frame_size == s->expected_size) {
			uint16_t crc = crc16_ansi(s->frame + 1, s->frame_size - 4u);
			bool valid = s->frame[s->frame_size - 1u] == 0x0d &&
			    s->frame[s->frame_size - 3u] == (uint8_t)crc &&
			    s->frame[s->frame_size - 2u] == (uint8_t)(crc >> 8);
			if (valid && s->reply.length != 0u && s->frame[4] != 1u)
				break;
			if (valid)
				process_frame(s);
			else {
				for (size_t i = 1; i < s->frame_size; ++i) {
					if (s->frame[i] != 0xfa) continue;
					size_t remaining = s->frame_size - i;
					if (remaining < 3u) {
						memmove(s->frame, s->frame + i, remaining);
						s->frame_size = remaining;
						s->expected_size = 0;
						break;
					}
					size_t nested = (size_t)s->frame[i + 1u] |
						((size_t)s->frame[i + 2u] << 8);
					if (nested < 9u || nested > ECSP_MAX_FRAME ||
					    nested < remaining) continue;
					memmove(s->frame, s->frame + i, remaining);
					s->frame_size = remaining;
					s->expected_size = nested;
					if (nested == remaining) {
						uint16_t check = crc16_ansi(s->frame + 1,
									    remaining - 4u);
						if (s->frame[remaining - 1u] == 0x0d &&
						    s->frame[remaining - 3u] == (uint8_t)check &&
						    s->frame[remaining - 2u] == (uint8_t)(check >> 8))
							process_frame(s);
						s->frame_size = s->expected_size = 0;
					}
					break;
				}
			}
			if (valid || s->frame_size == s->expected_size)
				s->frame_size = s->expected_size = 0;
			break;
		}
	}
	if (tx_budget != 0u) send_next(s, now_ms);
}

void ecsp_tx_complete(ecsp_t *ctx, bool success, uint32_t now_ms)
{
	struct ecsp_state *s = state(ctx);
	if (s->in_flight == 0u) return;
	if (success) s->last_sent_ms = now_ms;
	else s->link_faults++;
	if (s->in_flight == 1u) {
		if (!s->replaced_reply) s->reply.length = 0u;
	} else if (s->in_flight <= 3u) s->events[s->in_flight - 2u].length = 0u;
	s->in_flight = 0u;
	s->replaced_reply = false;
}

bool ecsp_publish_event(ecsp_t *ctx, uint8_t command, const uint8_t *payload,
			size_t length, bool fault)
{
	struct ecsp_state *s = state(ctx);
	if (!s->handshaken || (!s->upload_on && !(fault && s->received_command)))
		return false;
	size_t slot = fault ? 0u : 1u;
	if (s->events[slot].length == 0u && s->in_flight != slot + 2u &&
	    encode(s, &s->events[slot], command, payload, length)) {
		s->events[slot].fault = fault;
		return true;
	}
	s->tx_overflows++;
	return false;
}

bool ecsp_handshaken(const ecsp_t *ctx) { return const_state(ctx)->handshaken; }

struct ecsp_stats ecsp_stats(const ecsp_t *ctx)
{
	const struct ecsp_state *s = const_state(ctx);
	return (struct ecsp_stats){s->rx_overflows, s->tx_overflows, s->link_faults,
				   (s->rx_head + RX_CAPACITY - s->rx_tail) % RX_CAPACITY};
}
