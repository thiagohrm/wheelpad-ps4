/*
 * Reading the Thrustmaster wheel on the PS4.
 *
 * The console exposes libSceUsbd, a libusb compatible wrapper, so the wheel
 * is opened the same way the PC opens it: find the device by vendor and
 * product id, claim its HID interface and read the interrupt endpoint.
 *
 * The wheel reports only when something changes, so the poll thread keeps the
 * newest report and the consumer decides whether it is still fresh.
 */

#ifndef VOLANTE_WHEEL_USB_H
#define VOLANTE_WHEEL_USB_H

#include <stdint.h>

#include "report.h"

/* The wheel on the PC reports pid 0xB655, on the PS4 the same device shows up
 * as 0xB658. Both are accepted. */
#define VOLANTE_VID 0x044F
#define VOLANTE_PID_PC 0xB655
#define VOLANTE_PID_PS4 0xB658

/* Interrupt endpoint that carries the input report. The real address is
 * read from the configuration descriptor, because the wheel does not use
 * the usual endpoint 1. */
#define VOLANTE_EP_FALLBACK 0x81

/* Poll interval in microseconds, about 125 Hz. */
#define VOLANTE_POLL_US 8000

/* Transfer timeout in milliseconds. A wheel that stops reporting must not
 * block the poll thread forever. */
#define VOLANTE_TIMEOUT_MS 20

/* 1 when a wheel is open and reports have been seen. */
int volante_usb_available(void);

/* The newest report, and whether it is still fresh. Returns 0 when there is
 * nothing to report. */
int volante_usb_latest(volante_report *out);

/* Opens the wheel and starts the poll thread. Returns 0 on success. */
int volante_usb_start(void);

/* Stops the poll thread and closes the device. */
void volante_usb_stop(void);

/* Finds the wheel without opening it. Used to report what the console sees. */
int volante_usb_probe(void);

#endif /* VOLANTE_WHEEL_USB_H */
