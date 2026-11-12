/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Application layer: brings the runtime up and runs one super-loop step per
 * app_task() call. Board-independent functionality belongs in services/.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <system_stm32g4xx.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>
#include <zephyr/version.h>

#include "app/app.h"
#include "port/runtime.h"
#include "services/ecsp/ecsp.h"

LOG_MODULE_REGISTER(nihao, CONFIG_LOG_DEFAULT_LEVEL);

#define LED0_NODE DT_ALIAS(led0)
#define UART_NODE DT_CHOSEN(zephyr_protocol_uart)

#define RX_BUF_SIZE 64
#define BLINK_MS 500
#define RX_IDLE_TIMEOUT_US 1000

static const struct led_dt_spec led = LED_DT_SPEC_GET(LED0_NODE);
static const struct device *const uart = DEVICE_DT_GET(UART_NODE);

static uint8_t rx_buf[2][RX_BUF_SIZE];
static const uint8_t *volatile rx_free;
static volatile bool tx_done = true;
static uint8_t tx_frame[ECSP_MAX_FRAME];

static uint32_t toggles;
static uint64_t next_blink_ms;
static uint64_t next_handshake_ms;

static bool ecsp_send(const uint8_t *frame, size_t length, void *user)
{
	ARG_UNUSED(user);
	if (!tx_done || length > sizeof(tx_frame)) return false;
	memcpy(tx_frame, frame, length);
	tx_done = false;
	if (uart_tx(uart, tx_frame, length, SYS_FOREVER_US) != 0) {
		tx_done = true;
		return false;
	}
	return true;
}

static uint8_t board_command(uint8_t command, const uint8_t *request,
			     size_t length, uint8_t *response,
			     size_t *response_length, void *user)
{
	static const uint8_t version[] = {'1', '.', '0', '.', '0', '.', '0'};
	ARG_UNUSED(request);
	ARG_UNUSED(user);
	if (command != 2u) return 4u;
	if (length != 0u) return 2u;
	memcpy(response, version, sizeof(version));
	*response_length = sizeof(version);
	return 0u;
}

static const struct ecsp_command commands[] = {
	{2u, false, board_command},
};

uint32_t nihao_sem_timeout_count(void);
uint32_t nihao_unhandled_irq_count(void);
uint32_t nihao_irq_count(void);

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	switch (evt->type) {
	case UART_RX_BUF_REQUEST:
		if (rx_free != NULL) {
			(void)uart_rx_buf_rsp(dev, (uint8_t *)rx_free, RX_BUF_SIZE);
			rx_free = NULL;
		}
		break;
	case UART_RX_BUF_RELEASED:
		rx_free = evt->data.rx_buf.buf;
		break;
	case UART_RX_RDY: {
		const uint8_t *data = evt->data.rx.buf + evt->data.rx.offset;
		uint32_t len = evt->data.rx.len;
		ecsp_receive(data, len);
		break;
	}
	case UART_TX_DONE:
		tx_done = true;
		break;
	default:
		break;
	}
}

int app_init(void)
{
	runtime_init();

	LOG_INF("nihao %s built %s %s", KERNEL_VERSION_STRING, __DATE__, __TIME__);
	LOG_INF("sysclk %u Hz, uart %s at %u baud", SystemCoreClock,
		device_is_ready(uart) ? "ready" : "NOT ready",
		DT_PROP(UART_NODE, current_speed));

	if (!led_is_ready_dt(&led)) {
		return -ENODEV;
	}

	ecsp_init(3u, 2u, commands, ARRAY_SIZE(commands), ecsp_send, NULL);

	if (device_is_ready(uart)) {
		(void)uart_callback_set(uart, uart_cb, NULL);
		rx_free = rx_buf[1];
		if (uart_rx_enable(uart, rx_buf[0], RX_BUF_SIZE, RX_IDLE_TIMEOUT_US) != 0) {
			LOG_ERR("uart_rx_enable failed");
		} else {
			LOG_INF("async ECSP uart ready");
		}
	}

	return 0;
}

void app_task(void)
{
	uint64_t now;

	ecsp_poll(64u, runtime_millis());
	runtime_poll();

	now = runtime_millis();
	if (now >= next_handshake_ms && !ecsp_handshaken()) {
		ecsp_announce();
		next_handshake_ms = now + 1000u;
	}
	if (now < next_blink_ms) {
		return;
	}
	next_blink_ms = now + BLINK_MS;

	if ((toggles % 2u) == 0u) {
		led_on_dt(&led);
	} else {
		led_off_dt(&led);
	}
	toggles++;

	if ((toggles % 2u) == 0u) {
		LOG_INF("alive %u s, irq %u, sem timeout %u, miss %u",
			toggles / 2u, nihao_irq_count(),
			nihao_sem_timeout_count(), nihao_unhandled_irq_count());
	}
}
