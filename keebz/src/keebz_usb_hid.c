#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include "keebz_usb_dev.h"

LOG_MODULE_REGISTER(keebz_usb_hid, CONFIG_KEEBZ_USB_HID_LOG_LEVEL);

#define SLEEP_TIME_MS 3000
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led_alive), gpios);
static const uint8_t hid_report_desc[] = HID_KEYBOARD_REPORT_DESC();

static bool kb_ready;
static uint32_t kb_duration;

struct kb_event {
	uint16_t code;
	int32_t value;
};

enum kb_report_idx {
	KB_MOD_KEY = 0,
	KB_RESERVED,
	KB_KEY_CODE1,
	KB_KEY_CODE2,
	KB_KEY_CODE3,
	KB_KEY_CODE4,
	KB_KEY_CODE5,
	KB_KEY_CODE6,
	KB_REPORT_COUNT,
};

K_MSGQ_DEFINE(kb_msgq, sizeof(struct kb_event), 2, 1);
UDC_STATIC_BUF_DEFINE(report, KB_REPORT_COUNT);
struct kb_event kb_evt;

static void input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	kb_evt.code = evt->code;
	kb_evt.value = evt->value;
	LOG_INF("Got new event code=%d, value=%d", kb_evt.code, kb_evt.value);

	if (k_msgq_put(&kb_msgq, &kb_evt, K_NO_WAIT) != 0) {
		LOG_ERR("Failed to put new input event");
	}
}

INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

static void msg_cb(struct usbd_context *const usbd_ctx, const struct usbd_msg *const msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (msg->type == USBD_MSG_CONFIGURATION) {
		LOG_INF("\tConfiguration value %d", msg->status);
	}

	if (usbd_can_detect_vbus(usbd_ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(usbd_ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(usbd_ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}
}
static void kb_iface_ready(const struct device *dev, const bool ready)
{
	LOG_INF("HID device %s interface is %s", dev->name, ready ? "ready" : "not ready");
	kb_ready = ready;
}

static int kb_get_report(const struct device *dev, const uint8_t type, const uint8_t id, const uint16_t len,
			 uint8_t *const buf)
{
	LOG_WRN("Get Report not implemented, Type %u ID %u", type, id);

	return 0;
}

static int kb_set_report(const struct device *dev, const uint8_t type, const uint8_t id, const uint16_t len,
			 const uint8_t *const buf)
{
	if (type != HID_REPORT_TYPE_OUTPUT) {
		LOG_WRN("Unsupported report type");
		return -ENOTSUP;
	}

	LOG_HEXDUMP_INF(buf, len, "set report");

	return 0;
}

static void kb_set_idle(const struct device *dev, const uint8_t id, const uint32_t duration)
{
	LOG_INF("Set Idle %u to %u", id, duration);
	kb_duration = duration;
}

static uint32_t kb_get_idle(const struct device *dev, const uint8_t id)
{
	LOG_INF("Get Idle %u to %u", id, kb_duration);
	return kb_duration;
}

static void kb_set_protocol(const struct device *dev, const uint8_t proto)
{
	LOG_INF("Protocol changed to %s", proto == 0U ? "Boot Protocol" : "Report Protocol");
}

static void kb_output_report(const struct device *dev, const uint16_t len, const uint8_t *const buf)
{
	LOG_HEXDUMP_INF(buf, len, "output report");
	kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
}

struct hid_device_ops kb_ops = {
	.iface_ready = kb_iface_ready,
	.get_report = kb_get_report,
	.set_report = kb_set_report,
	.set_idle = kb_set_idle,
	.get_idle = kb_get_idle,
	.set_protocol = kb_set_protocol,
	.output_report = kb_output_report,
};

int main(void)
{
	int err;
	bool led_state = true;
	const struct device *hid_dev;
	struct usbd_context *sample_usbd;

	LOG_INF("Main started...");

	hid_dev = DEVICE_DT_GET(DT_ALIAS(hid_kbd));
	if (!device_is_ready(hid_dev)) {
		LOG_ERR("HID Device is not ready");
		return -EIO;
	}

	err = hid_device_register(hid_dev, hid_report_desc, sizeof(hid_report_desc), &kb_ops);
	if (err != 0) {
		LOG_ERR("Failed to register HID Device, %d", err);
		return err;
	}

	sample_usbd = keebz_usb_dev_init(msg_cb);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(sample_usbd)) {
		err = usbd_enable(sample_usbd);
		if (err) {
			LOG_ERR("Failed to enable device support");
			return err;
		}
	}

	LOG_INF("HID keyboard sample is initialized");

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED GPIO not ready");
		return -EIO;
	}

	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (err < 0) {
		LOG_ERR("Failed to configure LED GPIO");
		return -EIO;
	}

	while (true) {
		struct kb_event kb_evt;

		k_msgq_get(&kb_msgq, &kb_evt, K_FOREVER);

		switch (kb_evt.code) {
		case INPUT_KEY_0:
			if (kb_evt.value) {
				report[KB_KEY_CODE1] = HID_KEY_1;
			} else {
				report[KB_KEY_CODE1] = 0;
			}
			break;
		default:
			LOG_ERR("Unrecognized input code %u value %d", kb_evt.code, kb_evt.value);
			continue;
		}

		if (!kb_ready) {
			LOG_WRN("USB HID device is not ready");
			continue;
		}

		err = hid_device_submit_report(hid_dev, KB_REPORT_COUNT, report);
		if (err) {
			LOG_ERR("HID submit report error, %d", err);
		} else {
			LOG_INF("HID submit report");
		}
	}

	while (true) {
		err = gpio_pin_toggle_dt(&led);
		if (err < 0) {
			LOG_ERR("Failed to toggle LED GPIO");
			return -EIO;
		}

		led_state = !led_state;
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}