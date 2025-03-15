#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/logging/log.h>

#include "keebz_usb_dev.h"

LOG_MODULE_REGISTER(keebz_usb_dev, CONFIG_KEEBZ_USB_DEV_LOG_LEVEL);

USBD_DEVICE_DEFINE(keebz_usb_dev, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_KEEBZ_USB_DEV_VENDOR_ID,
		   CONFIG_KEEBZ_USB_DEV_PRODUCT_ID);

static const char *const keebz_usb_dev_blocklist[] = { "dfu_dfu", NULL };

USBD_DESC_LANG_DEFINE(keebz_usb_dev_lang);
USBD_DESC_MANUFACTURER_DEFINE(keebz_usb_dev_mfr, CONFIG_KEEBZ_USB_DEV_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(keebz_usb_dev_product, CONFIG_KEEBZ_USB_DEV_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(keebz_usb_dev_serial);

USBD_DESC_CONFIG_DEFINE(keebz_usb_dev_fs_desc, "Full-speed configuration");
USBD_DESC_CONFIG_DEFINE(keebz_usb_dev_hs_desc, "High-speed configuration");

static const uint8_t attributes = (IS_ENABLED(CONFIG_KEEBZ_USB_DEV_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_KEEBZ_USB_DEV_REMOTE_WAKEUP) ? USB_SCD_REMOTE_WAKEUP : 0);

USBD_CONFIGURATION_DEFINE(keebz_usb_dev_fs_cfg, attributes, CONFIG_KEEBZ_USB_DEV_MAX_POWER, &keebz_usb_dev_fs_desc);
USBD_CONFIGURATION_DEFINE(keebz_usb_dev_hs_cfg, attributes, CONFIG_KEEBZ_USB_DEV_MAX_POWER, &keebz_usb_dev_hs_desc);

static const struct usb_bos_capability_lpm keebz_usb_dev_bos_cap_lpm = {
	.bLength = sizeof(struct usb_bos_capability_lpm),
	.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
	.bmAttributes = 0UL,
};

USBD_DESC_BOS_DEFINE(keebz_usb_dev_ext_desc, sizeof(keebz_usb_dev_bos_cap_lpm), &keebz_usb_dev_bos_cap_lpm);

static void keebz_usb_dev_fix_code_triple(struct usbd_context *ctx, const enum usbd_speed speed)
{
	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) || IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) || IS_ENABLED(CONFIG_USBD_MIDI2_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS)) {
		usbd_device_set_code_triple(ctx, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(ctx, speed, 0, 0, 0);
	}
}

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
	int err;

	err = usbd_add_descriptor(&keebz_usb_dev, &keebz_usb_dev_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor, err=%d", err);
		return NULL;
	}

	err = usbd_add_descriptor(&keebz_usb_dev, &keebz_usb_dev_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor, err=%d", err);
		return NULL;
	}

	err = usbd_add_descriptor(&keebz_usb_dev, &keebz_usb_dev_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor, err=%d", err);
		return NULL;
	}

	err = usbd_add_descriptor(&keebz_usb_dev, &keebz_usb_dev_serial);
	if (err) {
		LOG_ERR("Failed to initialize serial number descriptor, err=%d", err);
		return NULL;
	}

	if (usbd_caps_speed(&keebz_usb_dev) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&keebz_usb_dev, USBD_SPEED_HS, &keebz_usb_dev_hs_cfg);
		if (err) {
			LOG_ERR("Failed to add high-speed configuration, err=%d", err);
			return NULL;
		}

		err = usbd_register_all_classes(&keebz_usb_dev, USBD_SPEED_HS, 1, keebz_usb_dev_blocklist);
		if (err) {
			LOG_ERR("Failed to add register classes for high-speed configuration, err=%d", err);
			return NULL;
		}

		keebz_usb_dev_fix_code_triple(&keebz_usb_dev, USBD_SPEED_HS);
	}

	err = usbd_add_configuration(&keebz_usb_dev, USBD_SPEED_FS, &keebz_usb_dev_fs_cfg);
	if (err) {
		LOG_ERR("Failed to add full-speed configuration, err=%d", err);
		return NULL;
	}

	err = usbd_register_all_classes(&keebz_usb_dev, USBD_SPEED_FS, 1, keebz_usb_dev_blocklist);
	if (err) {
		LOG_ERR("Failed to add register classes for full-speed configuration, err=%d", err);
		return NULL;
	}

	keebz_usb_dev_fix_code_triple(&keebz_usb_dev, USBD_SPEED_FS);
	usbd_self_powered(&keebz_usb_dev, attributes & USB_SCD_SELF_POWERED);

	if (cb) {
		err = usbd_msg_register_cb(&keebz_usb_dev, cb);
		if (err) {
			LOG_ERR("Failed to register message callback, err=%d", err);
			return NULL;
		}
	}

	if (IS_ENABLED(CONFIG_KEEBZ_USB_DEV_20_EXT_DESC)) {
		(void)usbd_device_set_bcd_usb(&keebz_usb_dev, USBD_SPEED_FS, 0x0201);
		(void)usbd_device_set_bcd_usb(&keebz_usb_dev, USBD_SPEED_HS, 0x0201);

		err = usbd_add_descriptor(&keebz_usb_dev, &keebz_usb_dev_ext_desc);
		if (err) {
			LOG_ERR("Failed to add USB 2.0 extension descriptor, err=%d", err);
			return NULL;
		}
	}

	return &keebz_usb_dev;
}