/* SPDX-License-Identifier: Apache-2.0 */
#include "services/ecsp/ecsp.h"
#include <assert.h>
#include <string.h>

static ecsp_t first, second;
static uint8_t sent[ECSP_MAX_FRAME];
static size_t sent_length;
static bool busy;
static bool broken;

static uint16_t crc(const uint8_t *data, size_t length)
{
	uint16_t value = 0xffff;
	for (size_t i = 0; i < length; ++i) {
		value ^= data[i];
		for (unsigned int b = 0; b < 8; ++b)
			value = value & 1 ? (value >> 1) ^ 0xa001 : value >> 1;
	}
	return value;
}

static size_t request(uint8_t *out, uint8_t board, uint8_t command)
{
	out[0] = 0xfa; out[1] = 9; out[2] = 0;
	out[3] = 3; out[4] = command; out[5] = board;
	uint16_t check = crc(out + 1, 5);
	out[6] = (uint8_t)check; out[7] = (uint8_t)(check >> 8); out[8] = 0x0d;
	return 9;
}

static size_t request_one(uint8_t *out, uint8_t board, uint8_t command,
			  uint8_t value)
{
	(void)request(out, board, command);
	out[1] = 10; out[6] = value;
	uint16_t check = crc(out + 1, 6);
	out[7] = (uint8_t)check; out[8] = (uint8_t)(check >> 8);
	out[9] = 0x0d;
	return 10;
}

static enum ecsp_link_result send(const uint8_t *frame, size_t length, void *user)
{
	(void)user;
	if (busy) return ECSP_LINK_BUSY;
	if (broken) return ECSP_LINK_FAULT;
	memcpy(sent, frame, length);
	sent_length = length;
	return ECSP_LINK_ACCEPTED;
}

int main(void)
{
	uint8_t frame[10];
	const uint8_t version[7] = {'1', '.', '0', '.', '0', '.', '0'};
	ecsp_init(&first, 3, 2, version, 0, 0, send, 0);
	ecsp_init(&second, 3, 4, version, 0, 0, send, 0);
	ecsp_poll(&first, 0, 1, 0);
	ecsp_poll(&first, 0, 1, 999);
	assert(sent_length == 0);
	ecsp_poll(&first, 0, 0, 1000);
	assert(sent_length == 0);
	ecsp_poll(&first, 0, 1, 1000);
	assert(sent_length == 9 && sent[4] == 1);
	ecsp_tx_complete(&first, true, 1001);
	assert(!ecsp_handshaken(&first) && !ecsp_handshaken(&second));

	/* A busy link must retain the handshake answer. */
	busy = true;
	ecsp_receive(&first, frame, request(frame, 2, 1));
	ecsp_poll(&first, 9, 1, 1010);
	assert(ecsp_handshaken(&first) && !ecsp_handshaken(&second));
	assert(sent[4] == 1 && sent_length == 9);
	busy = false;
	ecsp_poll(&first, 0, 1, 1011);
	assert(sent_length == 10 && sent[4] == 1 && sent[6] == 0);
	/* The heartbeat is not due before the answer finishes transmitting. */
	ecsp_poll(&first, 0, 1, 2010);
	assert(sent[4] == 1);
	ecsp_tx_complete(&first, true, 2011);
	ecsp_poll(&first, 0, 1, 3010);
	assert(sent[4] == 1);
	ecsp_poll(&first, 0, 1, 3011);
	assert(sent[4] == 0x10 && sent_length == 9);
	ecsp_tx_complete(&first, true, 3012);

	/* The addressed board alone enters its own session. */
	ecsp_receive(&second, frame, request(frame, 4, 1));
	ecsp_poll(&second, 9, 1, 3012);
	assert(ecsp_handshaken(&second));
	ecsp_tx_complete(&second, true, 3013);

	/* Re-handshake discards an old event that the busy link has not accepted. */
	ecsp_receive(&first, frame, request_one(frame, 2, 0x0f, 1));
	ecsp_poll(&first, 10, 1, 3020);
	assert(sent[4] == 0x0f && sent[6] == 0);
	ecsp_tx_complete(&first, true, 3021);
	assert(ecsp_publish_event(&first, 0x0a, (const uint8_t *)"\x00", 1, false));
	assert(ecsp_publish_event(&first, 0x11, (const uint8_t *)"\x02", 1, true));
	ecsp_poll(&first, 0, 1, 3022);
	assert(sent[4] == 0x11);
	ecsp_tx_complete(&first, true, 3022);
	ecsp_poll(&first, 0, 1, 3022);
	assert(sent[4] == 0x0a);
	ecsp_tx_complete(&first, true, 3022);
	assert(ecsp_publish_event(&first, 0x11, (const uint8_t *)"\x02", 1, true));
	busy = true;
	ecsp_poll(&first, 0, 1, 3022);
	ecsp_receive(&first, frame, request(frame, 2, 1));
	ecsp_poll(&first, 9, 1, 3023);
	busy = false;
	ecsp_poll(&first, 0, 1, 3024);
	assert(sent[4] == 1 && sent[6] == 0);
	ecsp_tx_complete(&first, true, 3025);
	sent_length = 0;
	ecsp_poll(&first, 0, 1, 3026);
	assert(sent_length == 0);
	/* An in-flight old answer must not erase the new handshake answer. */
	ecsp_receive(&first, frame, request(frame, 2, 2));
	ecsp_poll(&first, 9, 1, 3030);
	assert(sent[4] == 2 && sent_length == 17);
	ecsp_receive(&first, frame, request(frame, 2, 1));
	ecsp_poll(&first, 9, 1, 3031);
	ecsp_tx_complete(&first, true, 3032);
	ecsp_poll(&first, 0, 1, 3033);
	assert(sent[4] == 1 && sent_length == 10 && sent[6] == 0);
	ecsp_tx_complete(&first, true, 3034);
	broken = true;
	ecsp_receive(&first, frame, request(frame, 2, 2));
	ecsp_poll(&first, 9, 1, 3040);
	assert(ecsp_stats(&first).link_faults == 1u);
	return 0;
}
