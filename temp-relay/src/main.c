#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 2000
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// TODO: move into Kconfig
#define CONFIG_COMMS_STACK_SIZE 500
#define CONFIG_COMMS_PRIORITY 5

#define CONFIG_INFO_STACK_SIZE 500
#define CONFIG_INFO_PRIORITY 5
#define CONFIG_INFO_ALIVE_PERIOD_MS 1000

#define CONFIG_MONITOR_STACK_SIZE 500
#define CONFIG_MONITOR_PRIORITY 5

extern float bits_to_temp(const uint8_t msb, const uint8_t lsb)
{
	int16_t val = ((int16_t)msb << 4) | (lsb >> 4);
	if (val > 0x7FF) {
		val |= 0xF000;
	}
	return (val * 0.0625) * 100;
}

extern uint16_t temp_to_bits(const float temp)
{
	int16_t bits = (int16_t)(temp / 0.0625);
	if (bits < 0) {
		bits &= 0x0FFF;
	}
	return bits;
}

static void comms_run_thread()
{
	while (true) {
		k_msleep(SLEEP_TIME_MS);
	}
}

K_THREAD_DEFINE(comms_thread, CONFIG_COMMS_STACK_SIZE, comms_run_thread, NULL, NULL, NULL, CONFIG_COMMS_PRIORITY, 0, 0);

struct k_poll_signal info_signal;
struct k_poll_event info_events[1];
#define INFO_ALIVE_SIGNAL 0x42

struct k_timer info_timer;

void info_run_timer()
{
	k_poll_signal_raise(&info_signal, INFO_ALIVE_SIGNAL);
}

static void info_init_gpio(void)
{
	int err;

	err = gpio_is_ready_dt(&led) ? 0 : -EBUSY;
	if (!err) {
		err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	}

	__ASSERT_NO_MSG(!err);
}

static void info_init_signals(void)
{
	k_poll_signal_init(&info_signal);
	k_poll_event_init(&info_events[0], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &info_signal);
}

static void info_init_timer(void)
{
	k_timer_init(&info_timer, info_run_timer, NULL);
	k_timer_start(&info_timer, K_NO_WAIT, K_MSEC(CONFIG_INFO_ALIVE_PERIOD_MS));
}

static void info_run_thread()
{
	int sig_state, sig_result;
	bool led_state = true;

	info_init_gpio();
	info_init_signals();
	info_init_timer();

	while (true) {
		k_poll(info_events, ARRAY_SIZE(info_events), K_FOREVER);

		if (info_events[0].state == K_POLL_STATE_SIGNALED) {
			k_poll_signal_check(&info_signal, &sig_state, &sig_result);
			k_poll_signal_reset(&info_signal);
			info_events[0].state = K_POLL_STATE_NOT_READY;
		}

		if (sig_state && sig_result == INFO_ALIVE_SIGNAL) {
			gpio_pin_toggle_dt(&led);
			led_state = !led_state;
			printf("LED state: %s\n", led_state ? "ON" : "OFF");
		}
	}
}

K_THREAD_DEFINE(info_thread, CONFIG_INFO_STACK_SIZE, info_run_thread, NULL, NULL, NULL, CONFIG_INFO_PRIORITY, 0, 0);

static void monitor_run_thread()
{
	while (true) {
		k_msleep(SLEEP_TIME_MS);
	}
}

K_THREAD_DEFINE(monitor_thread, CONFIG_MONITOR_STACK_SIZE, monitor_run_thread, NULL, NULL, NULL,
		CONFIG_MONITOR_PRIORITY, 0, 0);