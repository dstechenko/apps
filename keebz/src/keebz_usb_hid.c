#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include "keebz_usb_dev.h"

LOG_MODULE_REGISTER(keebz_usb_hid, CONFIG_KEEBZ_USB_HID_LOG_LEVEL);

#define SLEEP_TIME_MS 200
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led_alive), gpios);

int main(void)
{
	int ret;
	bool led_state = true;

	LOG_INF("Main started...");

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED GPIO not ready");
		return -EIO;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED GPIO");
		return -EIO;
	}

	while (true) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			LOG_ERR("Failed to toggle LED GPIO");
			return -EIO;
		}

		led_state = !led_state;
		k_msleep(SLEEP_TIME_MS);
		LOG_ERR("LED GPIO toggled!");
	}

	return 0;
}