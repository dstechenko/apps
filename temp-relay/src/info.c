#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define CONFIG_INFO_STACK_SIZE 500
#define CONFIG_INFO_PRIORITY 5
#define CONFIG_INFO_ALIVE_PERIOD_MS 1000

struct k_poll_signal info_signal;
struct k_poll_event info_events[1];
#define INFO_ALIVE_SIGNAL 0x42

static struct k_timer info_timer;
static const struct gpio_dt_spec led_alive = GPIO_DT_SPEC_GET(DT_ALIAS(led_alive), gpios);

void info_run_timer()
{
	k_poll_signal_raise(&info_signal, INFO_ALIVE_SIGNAL);
}

static void info_init_gpio(void)
{
	int err;

	err = gpio_is_ready_dt(&led_alive) ? 0 : -EBUSY;
	if (!err) {
		err = gpio_pin_configure_dt(&led_alive, GPIO_OUTPUT_ACTIVE);
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
	int sig_state = 0, sig_result = 0;

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
			gpio_pin_toggle_dt(&led_alive);
		}
	}
}

K_THREAD_DEFINE(info_thread, CONFIG_INFO_STACK_SIZE, info_run_thread, NULL, NULL, NULL, CONFIG_INFO_PRIORITY, 0, 0);