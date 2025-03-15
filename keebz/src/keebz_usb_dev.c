#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/logging/log.h>

#include "keebz_usb_dev.h"

LOG_MODULE_REGISTER(keebz_usb_dev, CONFIG_KEEBZ_USB_DEV_LOG_LEVEL);

USBD_DEVICE_DEFINE(keebz_usb_dev, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_KEEBZ_USB_DEV_VENDOR_ID,
		   CONFIG_KEEBZ_USB_DEV_PRODUCT_ID);

struct usbd_context *keebz_usb_dev_init(usbd_msg_cb_t cb)
{
	int err;

	if (!keebz_usb_dev_setup(cb)) {
		LOG_ERR("Failed to setup device");
		return NULL;
	}

	err = usbd_init(&keebz_usb_dev);
	if (err) {
		LOG_ERR("Failed to initialize device support, err=%d", err);
		return NULL;
	}

	return &keebz_usb_dev;
}

struct usbd_context *keebz_usb_dev_setup(usbd_msg_cb_t cb)
{
	(void)cb;
	return NULL;
}