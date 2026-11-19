#ifndef USB_MAIN_H
#define USB_MAIN_H

#include <stdint.h>
#include <stdbool.h>

void usb_cdc_init(void);
void usb_cdc_write(const void *p_buf, uint16_t length);
bool is_usb_working(void);
// True if a host has addressed the device and the bus is not suspended. USB power alone gives false.
bool usb_host_is_active(void);

#endif
