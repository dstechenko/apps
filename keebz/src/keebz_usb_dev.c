#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/logging/log.h>

#include "keebz_usb_dev.h"

LOG_MODULE_REGISTER(keebz_usb_dev, CONFIG_KEEBZ_USB_DEV_LOG_LEVEL);

struct usbd_context *keebz_usb_dev_init(usbd_msg_cb_t cb)
{
	(void)cb;
	return NULL;
}

struct usbd_context *keebz_usb_dev_setup(usbd_msg_cb_t cb)
{
	(void)cb;
	return NULL;
}