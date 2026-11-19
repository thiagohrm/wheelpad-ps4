/*
 * Wheel report to pad state mapping.
 *
 * The values here are the PS4 pad button bits. They are duplicated from
 * OrbisPadButton on purpose: this file also compiles on the host, where the
 * Orbis headers do not exist. The plugin asserts at compile time that both
 * definitions agree, so a drift breaks the build instead of the game.
 */

#ifndef VOLANTE_MAPPING_H
#define VOLANTE_MAPPING_H

#include <stdint.h>

#include "report.h"

#define VOLANTE_PAD_L3 0x00000002u
#define VOLANTE_PAD_R3 0x00000004u
#define VOLANTE_PAD_OPTIONS 0x00000008u
#define VOLANTE_PAD_UP 0x00000010u
#define VOLANTE_PAD_RIGHT 0x00000020u
#define VOLANTE_PAD_DOWN 0x00000040u
#define VOLANTE_PAD_LEFT 0x00000080u
#define VOLANTE_PAD_L2 0x00000100u
#define VOLANTE_PAD_R2 0x00000200u
#define VOLANTE_PAD_L1 0x00000400u
#define VOLANTE_PAD_R1 0x00000800u
#define VOLANTE_PAD_TRIANGLE 0x00001000u
#define VOLANTE_PAD_CIRCLE 0x00002000u
#define VOLANTE_PAD_CROSS 0x00004000u
#define VOLANTE_PAD_SQUARE 0x00008000u
#define VOLANTE_PAD_TOUCH_PAD 0x00100000u

/* Left stick and triggers are 0-255. Triggers are 0 = released. */
#define VOLANTE_PAD_STICK_CENTRE 128u
#define VOLANTE_TRIGGER_THRESHOLD 32u

typedef struct {
    uint32_t buttons;
    uint8_t left_x;
    uint8_t left_y;
    uint8_t l2;
    uint8_t r2;
} volante_pad_state;

/*
 * Fill out from a parsed wheel report. Every field is rewritten, so the caller
 * can pass the same struct every frame without clearing it.
 */
void volante_map(const volante_report *wheel, volante_pad_state *out);

#endif /* VOLANTE_MAPPING_H */
