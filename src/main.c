/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <system_stm32f4xx.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>
#include <zephyr/version.h>

LOG_MODULE_REGISTER(nihao, CONFIG_LOG_DEFAULT_LEVEL);

#define LED0_NODE DT_ALIAS(led0)
#define CONSOLE_NODE DT_CHOSEN(zephyr_console)

#define RX_BUF_SIZE 64
#define RX_RING_SIZE 256
#define ECHO_CHUNK 32
#define BLINK_MS 500
#define RX_IDLE_TIMEOUT_US 1000

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct device *const console = DEVICE_DT_GET(CONSOLE_NODE);

static uint8_t rx_buf[2][RX_BUF_SIZE];
static const uint8_t *volatile rx_free;
static uint8_t rx_ring_data[RX_RING_SIZE];
static struct ring_buf rx_rb;
static volatile bool tx_done = true;
static volatile uint32_t echo_bytes;
static volatile uint32_t echo_dropped;

void nihao_init_run_all(void);
uint32_t nihao_sem_timeout_count(void);
void nihao_work_poll(void);
uint32_t nihao_unhandled_irq_count(void);
uint32_t nihao_irq_count(void);
uint32_t nihao_timestamp_get(void);

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
		unsigned int key = irq_lock();
		uint32_t written = ring_buf_put(&rx_rb, data, len);

		echo_dropped += len - written;
		irq_unlock(key);
		break;
	}
	case UART_TX_DONE:
		tx_done = true;
		break;
	default:
		break;
	}
}

static void echo_pump(void)
{
	uint8_t chunk[ECHO_CHUNK];

	while (tx_done && !ring_buf_is_empty(&rx_rb)) {
		uint32_t n = ring_buf_get(&rx_rb, chunk, sizeof(chunk));

		tx_done = false;
		if (uart_tx(console, chunk, n, SYS_FOREVER_US) != 0) {
			tx_done = true;
			echo_dropped += n;
			return;
		}

		for (uint32_t spin = 0; !tx_done && spin < 2000000u; spin++) {
		}

		if (!tx_done) {
			tx_done = true;
			echo_dropped += n;
			continue;
		}

		echo_bytes += n;
	}
}

static void idle_until(uint64_t deadline_ms)
{
	while (k_uptime_get() < deadline_ms) {
		nihao_work_poll();
		echo_pump();
		while (log_process()) {
		}
	}
}

int main(void)
{
	nihao_init_run_all();
	log_set_timestamp_func(nihao_timestamp_get, 1000);

	LOG_INF("nihao %s built %s %s", KERNEL_VERSION_STRING, __DATE__, __TIME__);
	LOG_INF("sysclk %u Hz, console %s at %u baud", SystemCoreClock,
		device_is_ready(console) ? "ready" : "NOT ready",
		DT_PROP(CONSOLE_NODE, current_speed));

	if (!device_is_ready(led.port)) {
		LOG_ERR("led0 port not ready");
		for (;;) {
		}
	}

	ring_buf_init(&rx_rb, RX_RING_SIZE, rx_ring_data);

	if (device_is_ready(console)) {
		(void)uart_callback_set(console, uart_cb, NULL);
		rx_free = rx_buf[1];
		if (uart_rx_enable(console, rx_buf[0], RX_BUF_SIZE, RX_IDLE_TIMEOUT_US) != 0) {
			LOG_ERR("uart_rx_enable failed");
		} else {
			LOG_INF("async uart ready: dma tx/rx, rx ring %u B", RX_RING_SIZE);
		}
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	uint32_t toggles = 0;

	for (;;) {
		nihao_work_poll();
		echo_pump();

		gpio_pin_toggle_dt(&led);
		toggles++;

		if ((toggles % 2u) == 0u) {
			LOG_INF("alive %u s, echo %u B, drop %u, irq %u, sem timeout %u, miss %u",
				toggles / 2u, echo_bytes, echo_dropped, nihao_irq_count(),
				nihao_sem_timeout_count(), nihao_unhandled_irq_count());
		}

		idle_until(k_uptime_get() + BLINK_MS);
	}
}
