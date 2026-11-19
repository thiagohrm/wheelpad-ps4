/*
 * Wheel report to pad state mapping. See docs/MAPPING.md for the table and the
 * reasoning. Keep this file free of platform headers so host tests can link it.
 */

#include "mapping.h"

static uint32_t map_buttons(const volante_report *wheel)
{
    uint32_t buttons = 0;

    /* Face buttons follow the labels on the wheel, not the order the wheel
     * reports them in. Button 1 is the square, 2 the cross, 3 the circle,
     * 4 the triangle. */
    if (wheel->buttons & VOLANTE_BTN_1) {
        buttons |= VOLANTE_PAD_SQUARE;
    }
    if (wheel->buttons & VOLANTE_BTN_2) {
        buttons |= VOLANTE_PAD_CROSS;
    }
    if (wheel->buttons & VOLANTE_BTN_3) {
        buttons |= VOLANTE_PAD_CIRCLE;
    }
    if (wheel->buttons & VOLANTE_BTN_4) {
        buttons |= VOLANTE_PAD_TRIANGLE;
    }
    if (wheel->buttons & VOLANTE_BTN_PADDLE_UP) {
        buttons |= VOLANTE_PAD_R1;
    }
    if (wheel->buttons & VOLANTE_BTN_PADDLE_DOWN) {
        buttons |= VOLANTE_PAD_L1;
    }
    if (wheel->buttons & VOLANTE_BTN_SE) {
        buttons |= VOLANTE_PAD_OPTIONS;
    }
    if (wheel->buttons & VOLANTE_BTN_ST) {
        buttons |= VOLANTE_PAD_TOUCH_PAD;
    }
    if (wheel->buttons & VOLANTE_BTN_HOME) {
        buttons |= VOLANTE_PAD_L3;
    }

    switch (wheel->hat) {
    case VOLANTE_HAT_UP:
        buttons |= VOLANTE_PAD_UP;
        break;
    case VOLANTE_HAT_RIGHT:
        buttons |= VOLANTE_PAD_RIGHT;
        break;
    case VOLANTE_HAT_DOWN:
        buttons |= VOLANTE_PAD_DOWN;
        break;
    case VOLANTE_HAT_LEFT:
        buttons |= VOLANTE_PAD_LEFT;
        break;
    default:
        break;
    }

    return buttons;
}

void volante_map(const volante_report *wheel, volante_pad_state *out)
{
    uint8_t l2;
    uint8_t r2;
    uint32_t buttons;

    if (wheel == NULL || out == NULL) {
        return;
    }

    buttons = map_buttons(wheel);

    /* Pedals are inverted on the wire: 0xFF is released. */
    l2 = (uint8_t)(0xFFu - wheel->brake);
    r2 = (uint8_t)(0xFFu - wheel->throttle);

    if (l2 >= VOLANTE_TRIGGER_THRESHOLD) {
        buttons |= VOLANTE_PAD_L2;
    }
    if (r2 >= VOLANTE_TRIGGER_THRESHOLD) {
        buttons |= VOLANTE_PAD_R2;
    }

    out->buttons = buttons;
    out->left_x = wheel->steering;
    out->left_y = (uint8_t)VOLANTE_PAD_STICK_CENTRE;
    out->l2 = l2;
    out->r2 = r2;
}
