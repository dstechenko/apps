#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <app_version.h>

LOG_MODULE_REGISTER(info, CONFIG_INFO_LOG_LEVEL);

#define INFO_SIGNAL_UNKNOWN 0x0
#define INFO_SIGNAL_ALIVE_LED 0x1
#define INFO_SIGNAL_ALIVE_LOG 0x2

static struct k_timer info_alive_led_timer;
static struct k_timer info_alive_log_timer;

static const struct gpio_dt_spec info_alive_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_alive), gpios);

static struct k_poll_signal info_alive_led_signal;
static struct k_poll_signal info_alive_log_signal;
static struct k_poll_event info_events[2];

static void info_alive_led_service()
{
	k_poll_signal_raise(&info_alive_led_signal, INFO_SIGNAL_ALIVE_LED);
}

static void info_alive_log_service()
{
	k_poll_signal_raise(&info_alive_log_signal, INFO_SIGNAL_ALIVE_LOG);
}

static void info_init_gpio(void)
{
	int err;

	err = gpio_is_ready_dt(&info_alive_led) ? 0 : -EBUSY;
	if (!err) {
		err = gpio_pin_configure_dt(&info_alive_led, GPIO_OUTPUT_ACTIVE);
	}

	__ASSERT_NO_MSG(!err);
}

static void info_init_signals(void)
{
	k_poll_signal_init(&info_alive_led_signal);
	k_poll_signal_init(&info_alive_log_signal);

	k_poll_event_init(&info_events[0], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &info_alive_led_signal);
	k_poll_event_init(&info_events[1], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &info_alive_log_signal);
}

static void info_init_timer(void)
{
	k_timer_init(&info_alive_led_timer, info_alive_led_service, NULL);
	k_timer_init(&info_alive_log_timer, info_alive_log_service, NULL);

	k_timer_start(&info_alive_led_timer, K_NO_WAIT, K_MSEC(CONFIG_INFO_ALIVE_LED_PERIOD_MS));
	k_timer_start(&info_alive_log_timer, K_NO_WAIT, K_MSEC(CONFIG_INFO_ALIVE_LOG_PERIOD_MS));
}

static void info_run_thread()
{
	int state = 0, type = INFO_SIGNAL_UNKNOWN;

	info_init_gpio();
	info_init_signals();
	info_init_timer();

	LOG_INF("running v%s", APP_VERSION_STRING);

	while (true) {
		k_poll(info_events, ARRAY_SIZE(info_events), K_FOREVER);

		if (info_events[0].state == K_POLL_STATE_SIGNALED) {
			k_poll_signal_check(&info_alive_led_signal, &state, (int *)&type);
			k_poll_signal_reset(&info_alive_led_signal);
			info_events[0].state = K_POLL_STATE_NOT_READY;
			if (state && type == INFO_SIGNAL_ALIVE_LED) {
				gpio_pin_toggle_dt(&info_alive_led);
			}
		}

		if (info_events[1].state == K_POLL_STATE_SIGNALED) {
			k_poll_signal_check(&info_alive_log_signal, &state, (int *)&type);
			k_poll_signal_reset(&info_alive_log_signal);
			info_events[1].state = K_POLL_STATE_NOT_READY;
			if (state && type == INFO_SIGNAL_ALIVE_LOG) {
				LOG_INF("thread is alive...");
			}
		}
	}
}

K_THREAD_DEFINE(info_thread, CONFIG_INFO_STACK_SIZE, info_run_thread, NULL, NULL, NULL, CONFIG_INFO_PRIORITY, 0, 0);