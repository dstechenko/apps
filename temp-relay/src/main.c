#include <stdio.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 2000

#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define MY_STACK_SIZE 500
#define MY_PRIORITY 5

static void my_entry_point(void *, void *, void *)
{
	int ret;
	bool led_state = true;

	if (!gpio_is_ready_dt(&led)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}

		led_state = !led_state;
		printf("LED state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}
}

K_THREAD_DEFINE(my_tid, MY_STACK_SIZE, my_entry_point, NULL, NULL, NULL,
		MY_PRIORITY, 0, 0);