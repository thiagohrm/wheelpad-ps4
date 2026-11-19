/*
 * Parser for the PS4 flavour of the Thrustmaster report.
 *
 * The console delivers the wheel state as a 27 byte reply to a vendor
 * control read. It is the same report as on the PC, shifted by one byte,
 * with the pedals reported the other way round. See docs/REPORTS.md.
 *
 * Pure C, no dependencies, so the same file compiles into the PS4 plugin
 * and into host tests.
 */

#include "report.h"

#define VOLANTE_CONSOLE_REPORT_LEN 27
#define VOLANTE_COMBO_REPORT_LEN 19

#define BYTE_BUTTONS_LOW 0u
#define BYTE_BUTTONS_HIGH 1u
#define BYTE_HAT 2u
#define BYTE_STEERING 3u
#define BYTE_BRAKE 4u
#define BYTE_THROTTLE 5u
#define BYTE_AXIS_EXTRA 6u

#define BUTTONS_HIGH_CONSTANT 0xE0u
#define HAT_MASK 0x0Fu

/*
 * On the console a pedal rests at 0x80 and reaches 0x00 when fully pressed,
 * so the travel is 0x80 wide. The shared representation uses 0xFF for
 * released and 0x00 for fully pressed, so the travel is rescaled from 0x80
 * to 0xFF. The wheel cannot travel past 0x00, so this is a straight linear
 * map of the reported range.
 */
#define CONSOLE_PEDAL_RELEASED 0x80u

/*
 * The console reports the hat as 0xFF when it is centred, while the PC
 * uses 0x0F. The direction bits are the same, so only the centred value
 * needs translating.
 */
#define CONSOLE_HAT_CENTRED 0xFFu

static uint8_t console_pedal(uint8_t value)
{
    uint32_t travel;

    if (value >= CONSOLE_PEDAL_RELEASED) {
        return 0xFFu;
    }

    travel = (uint32_t)(CONSOLE_PEDAL_RELEASED - value) * 255u / CONSOLE_PEDAL_RELEASED;
    return (uint8_t)(0xFFu - travel);
}

int volante_parse_console(const uint8_t *data, size_t len, volante_report *out)
{
    uint8_t low;
    uint8_t high;
    uint8_t brake;
    uint8_t throttle;
    uint8_t hat;
    uint16_t buttons = 0;

    if (data == NULL || out == NULL) {
        return -1;
    }
    if (len >= VOLANTE_COMBO_REPORT_LEN && len < VOLANTE_CONSOLE_REPORT_LEN) {
        uint8_t low = data[BYTE_BUTTONS_LOW];
        uint8_t high = (uint8_t)(data[BYTE_BUTTONS_HIGH] & (uint8_t)~BUTTONS_HIGH_CONSTANT);
        uint8_t hat = data[BYTE_HAT];
        uint16_t buttons = 0;

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

        if (hat == CONSOLE_HAT_CENTRED) {
            hat = VOLANTE_HAT_NONE;
        } else {
            hat = (uint8_t)(hat & HAT_MASK);
        }

        out->hat = hat;
        out->steering = data[BYTE_STEERING];
        /* In the 19 byte PC/combo report the pedals sit in bytes 4 and 5,
         * the same place as the documented PC layout, and use the shared
         * encoding already: 0xFF released, 0x00 fully pressed. */
        out->brake = data[BYTE_BRAKE];
        out->throttle = data[BYTE_THROTTLE];
        return 0;
    }

    if (len < VOLANTE_CONSOLE_REPORT_LEN) {
        return -1;
    }

    low = data[BYTE_BUTTONS_LOW];
    high = (uint8_t)(data[BYTE_BUTTONS_HIGH] & (uint8_t)~BUTTONS_HIGH_CONSTANT);

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

    if (high & 0x01u) {
        buttons |= VOLANTE_BTN_SE;
    }
    if (high & 0x02u) {
        buttons |= VOLANTE_BTN_ST;
    }
    if (high & 0x10u) {
        buttons |= VOLANTE_BTN_HOME;
    }

    brake = console_pedal(data[BYTE_BRAKE]);
    throttle = console_pedal(data[BYTE_THROTTLE]);

    out->buttons = buttons;

    hat = data[BYTE_HAT];
    if (hat == CONSOLE_HAT_CENTRED) {
        hat = VOLANTE_HAT_NONE;
    } else {
        hat = (uint8_t)(hat & HAT_MASK);
    }

    out->hat = hat;
    out->steering = data[BYTE_STEERING];
    out->brake = brake;
    out->throttle = throttle;
    return 0;
}
