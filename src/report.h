/*
 * Parser for the Thrustmaster Red Legend Edition input report.
 *
 * Layout is documented in docs/REPORTS.md. Pure C, no dependencies, so the same
 * file compiles into the PS4 plugin and into host tests.
 */

#ifndef VOLANTE_REPORT_H
#define VOLANTE_REPORT_H

#include <stddef.h>
#include <stdint.h>

#define VOLANTE_REPORT_LEN 19

/* Button bits, 1-based numbering as in the HID descriptor. */
#define VOLANTE_BTN_1 0x0001u
#define VOLANTE_BTN_2 0x0002u
#define VOLANTE_BTN_3 0x0004u
#define VOLANTE_BTN_4 0x0008u
#define VOLANTE_BTN_PADDLE_DOWN 0x0010u
#define VOLANTE_BTN_PADDLE_UP 0x0020u
#define VOLANTE_BTN_SE 0x0040u
#define VOLANTE_BTN_ST 0x0080u
#define VOLANTE_BTN_HOME 0x0400u

#define VOLANTE_HAT_UP 0u
#define VOLANTE_HAT_RIGHT 2u
#define VOLANTE_HAT_DOWN 4u
#define VOLANTE_HAT_LEFT 6u
#define VOLANTE_HAT_NONE 0x0Fu

typedef struct {
    /* 0 = full left, 128 = centre, 255 = full right. */
    uint8_t steering;
    /* 255 = released, 0 = fully pressed. */
    uint8_t brake;
    /* 255 = released, 0 = fully pressed. */
    uint8_t throttle;
    /* 0-7 clockwise from up, 0x0F = centred. */
    uint8_t hat;
    /* VOLANTE_BTN_* bits. */
    uint16_t buttons;
} volante_report;

/*
 * Returns 0 on success, -1 if the report is the wrong length.
 * Out-of-range values are clamped by the caller, not here.
 */
int volante_parse(const uint8_t *data, size_t len, volante_report *out);

/* The PS4 flavour of the report: 27 bytes, shifted by one, pedals the other
 * way round. See docs/REPORTS.md. */
#define VOLANTE_CONSOLE_REPORT_LEN 27

int volante_parse_console(const uint8_t *data, size_t len, volante_report *out);

#endif /* VOLANTE_REPORT_H */
