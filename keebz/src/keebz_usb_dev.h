#ifndef KEEBZ_USB_DEV_H
#define KEEBZ_USB_DEV_H

#include <stdint.h>

#include <zephyr/usb/usbd.h>

struct usbd_context *keebz_usb_dev_init(usbd_msg_cb_t cb);
struct usbd_context *keebz_usb_dev_setup(usbd_msg_cb_t cb);

#endif /* KEEBZ_USB_DEV_H */