/*
 * Parser for the Thrustmaster Red Legend Edition input report.
 * See docs/REPORTS.md for the byte map and the captures that prove it.
 */

#include "report.h"

#define BYTE_BUTTONS_LOW 0u
#define BYTE_BUTTONS_HIGH 1u
#define BYTE_HAT 2u
#define BYTE_STEERING 3u
#define BYTE_BRAKE 4u
#define BYTE_THROTTLE 5u

#define BUTTONS_HIGH_CONSTANT 0xE0u
#define HAT_MASK 0x0Fu

int volante_parse(const uint8_t *data, size_t len, volante_report *out)
{
    uint8_t low;
    uint8_t high;
    uint16_t buttons = 0;

    if (data == NULL || out == NULL) {
        return -1;
    }
    if (len < VOLANTE_REPORT_LEN) {
        return -1;
    }

    low = data[BYTE_BUTTONS_LOW];
    high = (uint8_t)(data[BYTE_BUTTONS_HIGH] & (uint8_t)~BUTTONS_HIGH_CONSTANT);

    /* Buttons 1-6 live in the low byte, bits 0-5. */
    if (low & 0x01u) {
        buttons |= VOLANTE_BTN_1;
    }
    if (low & 0x02u) {
        buttons |= VOLANTE_BTN_2;
    }
    if (low & 0x04u) {
        buttons |= VOLANTE_BTN_3;
    }
    if (low & 0x08u) {
        buttons |= VOLANTE_BTN_4;
    }
    if (low & 0x10u) {
        buttons |= VOLANTE_BTN_PADDLE_DOWN;
    }
    if (low & 0x20u) {
        buttons |= VOLANTE_BTN_PADDLE_UP;
    }

    /* Buttons 7-11 live in the high byte, bits 0-4. */
    if (high & 0x01u) {
        buttons |= VOLANTE_BTN_SE;
    }
    if (high & 0x02u) {
        buttons |= VOLANTE_BTN_ST;
    }
    if (high & 0x10u) {
        buttons |= VOLANTE_BTN_HOME;
    }

    out->buttons = buttons;
    out->hat = (uint8_t)(data[BYTE_HAT] & HAT_MASK);
    out->steering = data[BYTE_STEERING];
    out->brake = data[BYTE_BRAKE];
    out->throttle = data[BYTE_THROTTLE];
    return 0;
}
