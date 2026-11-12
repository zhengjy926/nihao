/* SPDX-License-Identifier: Apache-2.0 */
#include "services/ecsp/ecsp.h"
#include <string.h>
#include <zephyr/sys/crc.h>

#define RX_CAPACITY 1024u
#define REPLAY_COUNT 8u

static uint8_t product_address, board_address;
static const struct ecsp_command *command_table;
static size_t command_count;
static ecsp_send_fn send_frame;
static void *callback_user;
static volatile unsigned int rx_head, rx_tail;
static volatile bool overflow;
static uint8_t rx[RX_CAPACITY];
static uint8_t frame[ECSP_MAX_FRAME];
static size_t frame_size, expected_size;
static uint64_t last_byte_ms;
static bool handshaken;
static struct {
	uint8_t command, sequence;
	size_t length;
	uint8_t payload[ECSP_MAX_FRAME - 9u];
	bool valid;
} replay[REPLAY_COUNT];
static unsigned int replay_next;
static uint8_t pending_frame[ECSP_MAX_FRAME];
static size_t pending_length;

static bool respond(uint8_t command, const uint8_t *payload, size_t length)
{
	uint8_t out[ECSP_MAX_FRAME];
	size_t total = length + 9u;
	uint16_t crc;
	if (total > sizeof(out)) return false;
	out[0] = 0xfa;
	out[1] = (uint8_t)total;
	out[2] = (uint8_t)(total >> 8);
	out[3] = product_address;
	out[4] = command;
	out[5] = board_address;
	memcpy(out + 6, payload, length);
	crc = crc16_ansi(out + 1, total - 4u);
	out[total - 3u] = (uint8_t)crc;
	out[total - 2u] = (uint8_t)(crc >> 8);
	out[total - 1u] = 0x0d;
	if (send_frame(out, total, callback_user)) return true;
	memcpy(pending_frame, out, total);
	pending_length = total;
	return false;
}

static void process_frame(void)
{
	const struct ecsp_command *entry = NULL;
	uint8_t result[ECSP_MAX_FRAME - 9u];
	size_t data_length = 0;
	uint8_t command = frame[4];
	size_t request_length = frame_size - 9u;
	const uint8_t *request = frame + 6;
	if (frame[3] != product_address || frame[5] != board_address) return;
	if (command == 1u) {
		result[0] = request_length == 0u ? 0u : 2u;
		if (result[0] == 0u) {
			handshaken = true;
			memset(replay, 0, sizeof(replay));
			replay_next = 0;
		}
		(void)respond(command, result, 1u);
		return;
	}
	for (size_t i = 0; i < command_count; ++i) {
		if (command_table[i].code == command) { entry = &command_table[i]; break; }
	}
	if (entry == NULL) result[0] = 4u;
	else if (!handshaken) result[0] = 0x10u;
	else if (entry->sequenced && request_length == 0u) result[0] = 2u;
	else {
		if (entry->sequenced) {
			for (size_t i = 0; i < REPLAY_COUNT; ++i) {
				if (replay[i].valid && replay[i].command == command &&
				    replay[i].sequence == request[0]) {
					(void)respond(command, replay[i].payload, replay[i].length);
					return;
				}
			}
		}
		data_length = sizeof(result) - 1u;
		result[0] = entry->handle(command, request, request_length,
					  result + 1, &data_length, callback_user);
		if (data_length > sizeof(result) - 1u) return;
		if (result[0] != 0u) data_length = 0u;
		if (entry->sequenced) {
			unsigned int slot = replay_next++ % REPLAY_COUNT;
			replay[slot].command = command;
			replay[slot].sequence = request[0];
			replay[slot].length = data_length + 1u;
			memcpy(replay[slot].payload, result, data_length + 1u);
			replay[slot].valid = true;
		}
	}
	(void)respond(command, result, data_length + 1u);
}

void ecsp_init(uint8_t product, uint8_t board, const struct ecsp_command *commands,
	       size_t count, ecsp_send_fn send, void *user)
{
	product_address = product; board_address = board;
	command_table = commands; command_count = count;
	send_frame = send; callback_user = user;
	rx_head = rx_tail = 0; overflow = false;
	frame_size = expected_size = 0; last_byte_ms = 0; handshaken = false;
	pending_length = 0;
	memset(replay, 0, sizeof(replay)); replay_next = 0;
}

void ecsp_receive(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; ++i) {
		unsigned int next = (rx_head + 1u) % RX_CAPACITY;
		if (next == rx_tail) { overflow = true; return; }
		rx[rx_head] = bytes[i];
		rx_head = next;
	}
}

void ecsp_poll(size_t byte_budget, uint64_t now_ms)
{
	if (frame_size != 0u && now_ms - last_byte_ms >= 50u) {
		frame_size = expected_size = 0;
	}
	if (pending_length != 0u) {
		if (send_frame(pending_frame, pending_length, callback_user)) pending_length = 0;
		return;
	}
	if (overflow) {
		rx_tail = rx_head; frame_size = expected_size = 0;
		overflow = false;
	}
	while (byte_budget-- != 0u && rx_tail != rx_head) {
		uint8_t byte = rx[rx_tail];
		rx_tail = (rx_tail + 1u) % RX_CAPACITY;
		last_byte_ms = now_ms;
		if (frame_size == 0u && byte != 0xfa) continue;
		frame[frame_size++] = byte;
		if (frame_size == 3u) {
			expected_size = (size_t)frame[1] | ((size_t)frame[2] << 8);
			if (expected_size < 9u || expected_size > sizeof(frame)) {
				frame_size = expected_size = 0;
			}
		}
		if (expected_size != 0u && frame_size == expected_size) {
			uint16_t crc = crc16_ansi(frame + 1, frame_size - 4u);
			bool valid = frame[frame_size - 1u] == 0x0d &&
			    frame[frame_size - 3u] == (uint8_t)crc &&
			    frame[frame_size - 2u] == (uint8_t)(crc >> 8);
			if (valid) {
				process_frame();
			}
			/* An invalid frame may contain the start of the next valid one. */
			size_t restart = 0;
			for (size_t i = 1; !valid && i < frame_size; ++i) {
				if (frame[i] == 0xfa) { restart = i; break; }
			}
			if (restart != 0u) {
				frame_size -= restart;
				memmove(frame, frame + restart, frame_size);
				expected_size = frame_size >= 3u ?
					((size_t)frame[1] | ((size_t)frame[2] << 8)) : 0u;
				if (expected_size != 0u &&
				    (expected_size < 9u || expected_size > sizeof(frame) ||
				     expected_size < frame_size)) {
					frame_size = expected_size = 0;
				} else if (expected_size != 0u && expected_size == frame_size) {
					uint16_t nested_crc = crc16_ansi(frame + 1, frame_size - 4u);
					if (frame[frame_size - 1u] == 0x0d &&
					    frame[frame_size - 3u] == (uint8_t)nested_crc &&
					    frame[frame_size - 2u] == (uint8_t)(nested_crc >> 8))
						process_frame();
					frame_size = expected_size = 0;
				}
			} else frame_size = expected_size = 0;
			return;
		}
	}
}

bool ecsp_handshaken(void) { return handshaken; }

void ecsp_announce(void)
{
	if (!handshaken && pending_length == 0u) (void)respond(1u, NULL, 0u);
}
