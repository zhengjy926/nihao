/* SPDX-License-Identifier: Apache-2.0 */
#include "services/ecsp/ecsp.h"
#include <assert.h>
#include <string.h>
#include <zephyr/sys/crc.h>

static uint8_t sent[ECSP_MAX_FRAME];
static size_t sent_length;
static unsigned int calls;
static ecsp_t context;

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

static size_t request(uint8_t *out, uint8_t product, uint8_t board,
		      uint8_t command, const uint8_t *payload, size_t length)
{
	size_t total = length + 9;
	uint16_t check;
	out[0] = 0xfa; out[1] = (uint8_t)total; out[2] = 0;
	out[3] = product; out[4] = command; out[5] = board;
	memcpy(out + 6, payload, length);
	check = crc(out + 1, total - 4);
	out[total - 3] = (uint8_t)check;
	out[total - 2] = (uint8_t)(check >> 8);
	out[total - 1] = 0x0d;
	return total;
}

static enum ecsp_link_result send(const uint8_t *frame, size_t length, void *user)
{
	(void)user;
	memcpy(sent, frame, length);
	sent_length = length;
	return ECSP_LINK_ACCEPTED;
}

static uint8_t command(uint8_t code, const uint8_t *input, size_t length,
		       uint8_t *output, size_t *output_length, void *user)
{
	(void)input; (void)user; (void)code;
	++calls;
	if (length == 0) return 2;
	output[0] = 0x42;
	*output_length = 1;
	return 0;
}

static void feed(const uint8_t *frame, size_t length)
{
	ecsp_receive(&context, frame, length);
	ecsp_poll(&context, 512, 1, 0);
	ecsp_tx_complete(&context, true, 0);
}

int main(void)
{
	assert(crc16_ansi((const uint8_t *)"123456789", 9) == 0x4b37);
	const struct ecsp_command table[] = {{0x30, true, command}};
	const uint8_t version[7] = {'1', '.', '0', '.', '0', '.', '0'};
	uint8_t bytes[ECSP_MAX_FRAME], seq = 7;
	size_t length;
	ecsp_init(&context, 3, 2, version, table, 1, send, 0);
	length = request(bytes, 3, 2, 2, &seq, 1);
	feed(bytes, length);
	assert(sent_length == 10 && sent[6] == 0x10 && calls == 0);
	length = request(bytes, 3, 2, 0x7f, 0, 0);
	feed(bytes, length);
	assert(sent[6] == 4 && calls == 0);
	sent_length = 0;
	length = request(bytes, 3, 4, 1, 0, 0);
	feed(bytes, length);
	assert(sent_length == 0 && !ecsp_handshaken(&context));
	length = request(bytes, 3, 2, 1, 0, 0);
	bytes[6] ^= 1;
	feed(bytes, length);
	assert(!ecsp_handshaken(&context));
	length = request(bytes, 3, 2, 1, 0, 0);
	feed(bytes, length);
	assert(ecsp_handshaken(&context) && sent[6] == 0);
	length = request(bytes, 3, 2, 2, 0, 0);
	feed(bytes, length);
	assert(sent_length == 17 && sent[6] == 0 &&
	       memcmp(sent + 7, "1.0.0.0", 7) == 0);
	/* A corrupt outer frame can contain a complete valid handshake. */
	length = request(bytes, 3, 2, 1, 0, 0);
	uint8_t nested[20] = {0xfa, 20, 0};
	memcpy(nested + 11, bytes, length);
	sent_length = 0;
	feed(nested, sizeof(nested));
	assert(sent_length == 10 && sent[4] == 1);
	/* A stale partial frame must release the parser after 50 ms. */
	ecsp_receive(&context, bytes, 4);
	ecsp_poll(&context, 4, 1, 1);
	ecsp_poll(&context, 0, 1, 51);
	sent_length = 0;
	feed(bytes, length);
	assert(sent_length == 10 && sent[4] == 1);
	assert(crc(sent + 1, sent_length - 4) ==
	       ((uint16_t)sent[sent_length - 2] << 8 | sent[sent_length - 3]));
	/* CRC failure must not execute the command or poison the next frame. */
	length = request(bytes, 3, 2, 0x30, &seq, 1);
	bytes[6] ^= 1;
	feed(bytes, length);
	assert(calls == 0);
	length = request(bytes, 3, 2, 0x30, &seq, 1);
	feed(bytes, length);
	assert(calls == 1 && sent[6] == 0 && sent[7] == 0x42);
	feed(bytes, length);
	assert(calls == 1 && sent[7] == 0x42);
	/* An overrun drops a partial frame and permits a fresh handshake. */
	ecsp_receive(&context, bytes, 4);
	for (unsigned int i = 0; i < 1100; ++i) ecsp_receive(&context, bytes, 1);
	ecsp_poll(&context, 64, 1, 0);
	length = request(bytes, 3, 2, 1, 0, 0);
	feed(bytes, length);
	assert(sent[4] == 1 && sent[6] == 0);
	return 0;
}
